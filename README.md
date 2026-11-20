# 🚜 TerraSow™ — Autonomous Agriculture Rover Firmware & Telemetry Dashboard

[![ESP32](https://img.shields.io/badge/Microcontroller-ESP32-red.svg?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![C++](https://img.shields.io/badge/Firmware-C%2B%2B-blue.svg?logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![HTML5](https://img.shields.io/badge/Interface-HTML5%20%2F%20JavaScript-orange.svg?logo=html5&logoColor=white)](https://developer.mozilla.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> Firmware and interactive web control console for the TerraSow autonomous precision seeding rover, combining low-latency ESP32 motor driver actuation with real-time browser-based telemetry and directional navigation.

---

## 🌟 Key Features

1. **Embedded ESP32 Firmware (`ESP32_Code.cpp`)**
   - High-frequency Dual-H-Bridge PWM motor speed & steering control.
   - Built-in Wi-Fi SoftAP / Station mode web server serving real-time control packets over WebSockets / HTTP.
   - Actuator control routines for seed metering dispensor mechanisms and furrow depth calibration.

2. **Real-Time Rover Control Dashboard (`RoverUI.html`)**
   - Zero-dependency, single-file responsive operator console accessible from smartphones, tablets, or field laptops.
   - Multi-axis virtual touch joystick for precise field maneuvering.
   - Live telemetry gauges displaying battery voltage levels, Wi-Fi RSSI signal quality, motor RPMs, and hopper capacity.

---

## 🛠️ Hardware Requirements & Pinout

| Peripheral | ESP32 GPIO Pin | Description |
| :--- | :--- | :--- |
| **Motor Driver (L298N / TB6612FNG)** | GPIO 18, 19, 21, 22 | Directional forward/reverse logic and PWM speed |
| **Seeder Actuator / Servo** | GPIO 23 | Micro-servo gate trigger for seed release |
| **Battery Voltage Divider** | GPIO 34 (ADC1) | Analog voltage sensor for pack health monitoring |
| **Status LEDs** | GPIO 2, 4 | Power and connectivity state indicators |

---

## 🚀 Setup & Flashing Guide

### 1. Flashing ESP32 Firmware
1. Open `ESP32_Code.cpp` in the **Arduino IDE** or **PlatformIO**.
2. Install the `ESP32 Board Package` by Espressif.
3. Configure your Wi-Fi SSID and Password in the firmware configuration block.
4. Select board **ESP32 Dev Module** and flash via USB-to-UART.

### 2. Operating the Web Console
1. Connect your smartphone or computer to the TerraSow Wi-Fi network (or local subnet).
2. Open `RoverUI.html` in any modern web browser or navigate to `http://192.168.4.1`.
3. Use the interactive touch joystick and toggle buttons to control the rover.

---

## 📝 License
This project is licensed under the MIT License.
