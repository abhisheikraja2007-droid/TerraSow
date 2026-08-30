#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ================= NETWORK SETTINGS =================
#define WIFI_SSID "PEC-10"
#define WIFI_PASSWORD "cb:dd:1c"

// ================= FIREBASE KEYS =================
#define API_KEY "YOUR_GOOGLE_MAPS_API_KEY"
#define DATABASE_URL "https://agricontrol-rover-default-rtdb.asia-southeast1.firebasedatabase.app"

FirebaseData stream; 
FirebaseData fbdo;   
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

// ================= DYNAMIC AUTOMATION SETTINGS =================
unsigned long actuatorExtendTime = 10000;
unsigned long actuatorRetractTime = 10000;
int autoDriveSpeedPWM = 70;                 
unsigned long hopperOpenTime = 800;       

const int HOPPER_CLOSE_ANGLE = 180;                  
const int HOPPER_OPEN_ANGLE = 90;                    
const int DRILL_SWEEP_STEP = 5;                      
const unsigned long DRILL_SWEEP_SPEED_MS = 100;      

// --- Continuous Loop Kinematics ---
const unsigned long ALIGN_HOPPER_TIME_MS = 2500; 
unsigned long totalSpacingTime = 3000;          
unsigned long nextHoleDriveTime = 2200;         

// ================= PIN DEFINITIONS =================
const int LEFT_DIR_PIN = 5;
const int LEFT_PWM_PIN = 4;
const int RIGHT_DIR_PIN = 7;
const int RIGHT_PWM_PIN = 6;
const int ACTUATOR_ENA = 11;
const int ACTUATOR_IN1 = 12;
const int ACTUATOR_IN2 = 14;
const int SERVO1_PIN = 16; 
const int SERVO2_PIN = 10; 

Servo drillServo;
Servo hopperServo;

int roverSpeed = 200; 

// --- Sequence State Machine ---
bool autoPlantingActive = false;
bool isDrilling = false;
unsigned long lastSweepTime = 0;
int sweepAngle = 90;
int currentSweepStep = DRILL_SWEEP_STEP;
int holeCounter = 1; // Tracks how many seeds we planted

enum PlantState { IDLE, DRILL_DOWN, DRILL_UP, ALIGN_HOPPER, DROP_SEED, MOVE_TO_NEXT };
PlantState currentState = IDLE;
unsigned long sequenceTimer = 0;

// ================= HARDWARE FUNCTIONS =================
void stopMotors() {
  analogWrite(LEFT_PWM_PIN, 0); analogWrite(RIGHT_PWM_PIN, 0);
}
void moveForward() {
  digitalWrite(LEFT_DIR_PIN, HIGH); digitalWrite(RIGHT_DIR_PIN, LOW);
  analogWrite(LEFT_PWM_PIN, roverSpeed); analogWrite(RIGHT_PWM_PIN, roverSpeed);
}
void moveBackward() {
  digitalWrite(LEFT_DIR_PIN, LOW); digitalWrite(RIGHT_DIR_PIN, HIGH);
  analogWrite(LEFT_PWM_PIN, roverSpeed); analogWrite(RIGHT_PWM_PIN, roverSpeed);
}
void rotateLeft() {
  digitalWrite(LEFT_DIR_PIN, LOW); digitalWrite(RIGHT_DIR_PIN, LOW); 
  analogWrite(LEFT_PWM_PIN, roverSpeed); analogWrite(RIGHT_PWM_PIN, roverSpeed);
}
void rotateRight() {
  digitalWrite(LEFT_DIR_PIN, HIGH); digitalWrite(RIGHT_DIR_PIN, HIGH); 
  analogWrite(LEFT_PWM_PIN, roverSpeed); analogWrite(RIGHT_PWM_PIN, roverSpeed);
}
void actuatorExtend() {
  digitalWrite(ACTUATOR_IN1, HIGH); digitalWrite(ACTUATOR_IN2, LOW);
  analogWrite(ACTUATOR_ENA, 255);
}
void actuatorRetract() {
  digitalWrite(ACTUATOR_IN1, LOW); digitalWrite(ACTUATOR_IN2, HIGH);
  analogWrite(ACTUATOR_ENA, 255);
}
void actuatorStop() {
  digitalWrite(ACTUATOR_IN1, LOW); digitalWrite(ACTUATOR_IN2, LOW);
  analogWrite(ACTUATOR_ENA, 0);
}

// ================= COMMAND HANDLER =================
void processCommand(char cmd) {
  if (cmd == 'e' || cmd == 's') {
    autoPlantingActive = false;
    currentState = IDLE; 
    isDrilling = false;
    stopMotors(); 
    actuatorStop();
    hopperServo.write(HOPPER_CLOSE_ANGLE); 
    
    if (cmd == 'e') Serial.println("\n🚨 EMERGENCY STOP TRIGGERED! All systems halted.");
    else Serial.println("⏸️ COMMAND: STOP (Motors & Actuator Halted)");
    return;
  }

  if (autoPlantingActive) {
    Serial.println("⚠️ IGNORING COMMAND: Automated planting loop is currently active.");
    return; 
  }

  if (cmd == 'p') {
    Serial.println("\n=============================================");
    Serial.println("▶️ STARTING CONTINUOUS PLANTING OPERATION");
    Serial.println("=============================================");
    Serial.println("📡 1. Fetching Crop Profile from Firebase...");
    
    if (Firebase.RTDB.getString(&fbdo, "/rover_telemetry/current_crop")) {
      String currentCrop = fbdo.stringData();
      Serial.print("   -> Selected Crop: "); Serial.println(currentCrop);
      
      String path = "/crop_profiles/" + currentCrop + "/";
      
      if (Firebase.RTDB.getInt(&fbdo, path + "actuator_extend_ms")) actuatorExtendTime = fbdo.intData();
      if (Firebase.RTDB.getInt(&fbdo, path + "actuator_retract_ms")) actuatorRetractTime = fbdo.intData();
      if (Firebase.RTDB.getInt(&fbdo, path + "auto_drive_speed_pwm")) autoDriveSpeedPWM = fbdo.intData();
      if (Firebase.RTDB.getInt(&fbdo, path + "hopper_open_time_ms")) hopperOpenTime = fbdo.intData();
      if (Firebase.RTDB.getInt(&fbdo, path + "auto_drive_time_ms")) {
        totalSpacingTime = fbdo.intData();
        if (totalSpacingTime > ALIGN_HOPPER_TIME_MS) {
          nextHoleDriveTime = totalSpacingTime - ALIGN_HOPPER_TIME_MS;
        } else {
          nextHoleDriveTime = 0; 
        }
      }
      
      Serial.println("✅ 2. Profile Downloaded Successfully!");
      Serial.printf("   -> Drill Depth Time: %lu ms\n", actuatorExtendTime);
      Serial.printf("   -> Next Hole Drive Time: %lu ms\n", nextHoleDriveTime);
      
      Serial.println("\n🚀 3. Initiating First Sequence...");
      Serial.println("[STATE] DRILL_DOWN: Pushing actuator into soil...");
      holeCounter = 1;
      autoPlantingActive = true;
      currentState = DRILL_DOWN;
      isDrilling = true;
      actuatorExtend();
      sequenceTimer = millis();
    } else {
      Serial.println("❌ ERROR: Failed to fetch crop profile. Aborting sequence.");
    }
  }
  else {
    // Manual Drive & Actuator commands
    if (cmd == 'f') { Serial.println("🕹️ MANUAL: Driving Forward"); moveForward(); }
    else if (cmd == 'b') { Serial.println("🕹️ MANUAL: Driving Backward"); moveBackward(); }
    else if (cmd == 'l') { Serial.println("🕹️ MANUAL: Rotating Left"); rotateLeft(); }
    else if (cmd == 'r') { Serial.println("🕹️ MANUAL: Rotating Right"); rotateRight(); }
    else if (cmd == 'u') { Serial.println("⚙️ MANUAL: Actuator UP (Retracting)"); actuatorRetract(); }
    else if (cmd == 'd') { Serial.println("⚙️ MANUAL: Actuator DOWN (Extending)"); actuatorExtend(); }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n--- AGRICONTROL PRO ESP32 BOOTING ---");
  
  pinMode(LEFT_DIR_PIN, OUTPUT); pinMode(LEFT_PWM_PIN, OUTPUT);
  pinMode(RIGHT_DIR_PIN, OUTPUT); pinMode(RIGHT_PWM_PIN, OUTPUT);
  pinMode(ACTUATOR_ENA, OUTPUT); pinMode(ACTUATOR_IN1, OUTPUT); pinMode(ACTUATOR_IN2, OUTPUT);
  
  stopMotors(); actuatorStop();

  ESP32PWM::allocateTimer(0); ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2); ESP32PWM::allocateTimer(3);
  
  drillServo.setPeriodHertz(50); drillServo.attach(SERVO1_PIN, 500, 2400);
  hopperServo.setPeriodHertz(50); hopperServo.attach(SERVO2_PIN, 500, 2400);
  
  drillServo.write(90);
  hopperServo.write(HOPPER_CLOSE_ANGLE); 

  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(300);
  }
  Serial.println("\n✅ Wi-Fi Connected!");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ Firebase Authenticated!");
    signupOK = true;
  } else {
    Serial.printf("❌ Firebase Auth Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.RTDB.beginStream(&stream, "/rover_control/command")) {
    Serial.printf("❌ Stream Error: %s\n", stream.errorReason().c_str());
  } else {
    Serial.println("✅ Stream Active! Rover is ready for web commands.");
    Serial.println("---------------------------------------------------");
  }
}

// ================= MAIN LOOP =================
void loop() {
  unsigned long currentMillis = millis();

  if (Firebase.ready() && signupOK) {
    if (!Firebase.RTDB.readStream(&stream)) {
      Serial.printf("Stream read error: %s\n", stream.errorReason().c_str());
    }
    if (stream.streamTimeout()) Serial.println("Stream timeout, reconnecting...");
    
    if (stream.streamAvailable()) {
      String val = stream.stringData(); 
      val.replace("\"", ""); 
      if (val.length() > 0) processCommand(val.charAt(0));
    }
  }

  if (autoPlantingActive) {
    switch (currentState) {
      case DRILL_DOWN:
        if (currentMillis - sequenceTimer >= actuatorExtendTime) {
          Serial.println("[STATE] DRILL_UP: Target depth reached. Retracting actuator...");
          actuatorRetract();
          sequenceTimer = currentMillis;
          currentState = DRILL_UP;
        }
        break;
        
      case DRILL_UP:
        if (currentMillis - sequenceTimer >= actuatorRetractTime) {
          Serial.println("[STATE] ALIGN_HOPPER: Actuator raised. Driving 15cm forward to align hopper...");
          actuatorStop();
          isDrilling = false; 
          drillServo.write(90); 
          roverSpeed = autoDriveSpeedPWM; 
          moveForward();
          sequenceTimer = currentMillis;
          currentState = ALIGN_HOPPER;
        }
        break;
        
      case ALIGN_HOPPER:
        if (currentMillis - sequenceTimer >= ALIGN_HOPPER_TIME_MS) {
          Serial.println("[STATE] DROP_SEED: Hopper aligned. Opening seed chute...");
          stopMotors();
          hopperServo.write(HOPPER_OPEN_ANGLE); 
          sequenceTimer = currentMillis;
          currentState = DROP_SEED;
        }
        break;
        
      case DROP_SEED:
        if (currentMillis - sequenceTimer >= hopperOpenTime) {
          Serial.printf("✅ Seed #%d Planted! Closing hopper.\n", holeCounter);
          Serial.println("[STATE] MOVE_TO_NEXT: Driving to next plant spacing...");
          holeCounter++;
          hopperServo.write(HOPPER_CLOSE_ANGLE); 
          roverSpeed = autoDriveSpeedPWM; 
          moveForward();
          sequenceTimer = currentMillis;
          currentState = MOVE_TO_NEXT;
        }
        break;
        
      case MOVE_TO_NEXT:
        if (currentMillis - sequenceTimer >= nextHoleDriveTime) {
          Serial.println("[STATE] DRILL_DOWN: Reached target location. Drilling next hole...");
          stopMotors();
          currentState = DRILL_DOWN;
          isDrilling = true;
          actuatorExtend();
          sequenceTimer = currentMillis;
        }
        break;
        
      default:
        break;
    }
  }

  // --- DRILL SWEEP LOGIC ---
  if (isDrilling && (currentMillis - lastSweepTime >= DRILL_SWEEP_SPEED_MS)) { 
    lastSweepTime = currentMillis;
    sweepAngle += currentSweepStep;
    if (sweepAngle >= 180) { sweepAngle = 180; currentSweepStep = -DRILL_SWEEP_STEP; }
    else if (sweepAngle <= 0) { sweepAngle = 0; currentSweepStep = DRILL_SWEEP_STEP; }
    drillServo.write(sweepAngle);
  }
  
  delay(2); 
}