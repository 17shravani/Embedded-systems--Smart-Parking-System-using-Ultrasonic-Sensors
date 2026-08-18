# 🚗 Smart Parking System using Ultrasonic Sensors (IoT & Embedded Systems)

<img width="1385" height="896" alt="Screenshot 2026-08-18 223002" src="https://github.com/user-attachments/assets/f8de7beb-032d-4927-8b98-5e1e3e540bae" />


[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Microcontroller](https://img.shields.io/badge/Microcontroller-ESP32%20%2F%20Arduino%20UNO-blue)](https://www.espressif.com/)
[![Simulation](https://img.shields.io/badge/Simulation-Wokwi%20Ready-brightgreen)](https://wokwi.com/)
[![Embedded C++](https://img.shields.io/badge/Language-Embedded%20C%2FC%2B%2B-orange.svg)](https://isocpp.org/)

An industry-oriented, production-ready **Smart Parking Guidance & Automated Access Control System** powered by ultrasonic sensors, real-time OLED matrix display, servo-controlled barrier gates, and an embedded responsive web dashboard.

---

## 📌 Table of Contents
1. [Overview & Problem Statement](#-overview--problem-statement)
2. [Key Features](#-key-features)
3. [System Architecture](#-system-architecture)
4. [Hardware & Components List](#-hardware--components-list)
5. [Circuit Schematic & Pin Configuration](#-circuit-schematic--pin-configuration)
6. [Mathematical Modeling & Distance Logic](#-mathematical-modeling--distance-logic)
7. [Repository Structure](#-repository-structure)
8. [Virtual Simulation Guide (Wokwi 1-Click)](#-virtual-simulation-guide-wokwi-1-click)
9. [Physical Hardware Setup & Flashing](#-physical-hardware-setup--flashing)
10. [REST API Documentation](#-rest-api-documentation)
11. [Testing & Verification Matrix](#-testing--verification-matrix)
12. [Day-Wise Git Proof-Building Strategy](#-day-wise-git-proof-building-strategy)
13. [Technical Interview Preparation (Q&A)](#-technical-interview-preparation-qa)
14. [License](#-license)

---

## 📖 Overview & Problem Statement

Urban congestion studies indicate that over **30% of downtown city traffic** is caused by drivers searching for vacant parking spaces. Traditional parking facilities suffer from:
- Lack of live space visibility at entry barriers.
- Traffic choke points and increased carbon emissions.
- Reliance on manual parking attendants.

### 💡 The Solution
This embedded system continuously tracks bay occupancy across 4 parking slots using HC-SR04 ultrasonic sensors. An **Espressif ESP32** processes the sensor telemetry, applies a **5-point median & outlier-trimmed filter**, and updates:
1. **Dual LED indicators** on each bay (Green = Free, Red = Occupied).
2. **I2C SSD1306 OLED Display** showing available bay counts and grid status.
3. **Automated Entry Barrier** (SG90 Servo) that opens for arriving vehicles and closes automatically.
4. **Active Buzzer Alarm** alerting motorists when the facility is 100% full.
5. **Real-time Web Dashboard & REST API** over WiFi.

---

## 🚀 Key Features

- **High-Precision Ultrasonic Sensing:** 2cm to 400cm detection with time-division multiplexing to eliminate acoustic cross-talk.
- **Robust Digital Filtering:** 5-sample median window + 600ms software state debouncer to reject transient noise.
- **Automated Gate Actuation:** Non-blocking hardware PWM servo control with automated close timers.
- **Fail-Safe Alarm System:** Audio-visual warning sequence triggered upon full occupancy.
- **Dual-Mode IoT Connectivity:** Connects to existing WiFi networks (Station Mode) or creates its own fallback Access Point (`SmartParking-IoT-Hub`).
- **Glassmorphic Web Dashboard:** Responsive, auto-refreshing interface with live vehicle telemetry and remote gate override.

---

## 🏗 System Architecture

```
                                 [ PHYSICAL INPUTS ]
                                          |
                +-------------------------+-------------------------+
                |                         |                         |
        [Ultrasonic Bay 1-4]      [Vehicle Approach]        [Remote Web API]
        (HC-SR04 ToF Sensors)      (Ultrasonic/Button)       (HTTP REST Client)
                |                         |                         |
                +-------------------------+-------------------------+
                                          |
                                          v
                +---------------------------------------------------+
                |            ESP32 EMBEDDED CONTROLLER              |
                |  - Noise Rejection Filter (5-Point Trimmed Mean)  |
                |  - Debounce Engine (600ms Stability Window)       |
                |  - Availability Counter (Free = 4 - Occupied)     |
                |  - Hardware PWM LEDC Servo Engine (50Hz)          |
                |  - Non-Blocking WebServer & JSON REST API         |
                +---------------------------------------------------+
                                          |
                +-------------------------+-------------------------+
                |                         |                         |
                v                         v                         v
        [LOCAL VISUALS]            [ACTUATORS]             [IOT DASHBOARD]
        - 4x Dual LEDs (G/R)       - SG90 Barrier Gate     - Browser Web UI
        - 128x64 OLED Display      - Active Buzzer Alarm   - REST `/api/status`
```

---

## 🛠 Hardware & Components List

| Component | Quantity | Purpose / Role |
|---|---|---|
| **ESP32 DevKit V1** (or Arduino UNO) | 1 | Primary central embedded processing unit |
| **HC-SR04 Ultrasonic Sensors** | 4 | Real-time vehicle detection per parking bay |
| **SSD1306 0.96" OLED (I2C)** | 1 | Local availability matrix display |
| **SG90 Micro Servo Motor** | 1 | Automated entry barrier gate mechanism |
| **5V Active Piezo Buzzer** | 1 | Parking full acoustic warning alarm |
| **Green 5mm LEDs** | 4 | Visual indicator for FREE parking bays |
| **Red 5mm LEDs** | 4 | Visual indicator for OCCUPIED parking bays |
| **220Ω Resistors** | 8 | Current limiting for LEDs |
| **1kΩ & 2kΩ Resistors** | 4 pairs | Voltage divider level shifters for 5V Echo $\to$ 3.3V ESP32 |
| **Breadboard & Jumper Wires** | 1 set | Circuit prototyping & interconnects |

---

## 🔌 Circuit Schematic & Pin Configuration

| Component | Pin | ESP32 GPIO | Description |
|---|---|---|---|
| **OLED (SSD1306)** | SDA / SCL | **GPIO 21 / GPIO 22** | Hardware I2C Bus |
| **Gate Servo** | PWM Signal | **GPIO 18** | LEDC Hardware PWM Channel 2 |
| **Piezo Buzzer** | Positive (+) | **GPIO 19** | Digital Active Buzzer Trigger |
| **Bay 1 Sensor** | TRIG / ECHO | **GPIO 5 / GPIO 17** | Bay 1 Ultrasonic Sensor |
| **Bay 2 Sensor** | TRIG / ECHO | **GPIO 16 / GPIO 4** | Bay 2 Ultrasonic Sensor |
| **Bay 3 Sensor** | TRIG / ECHO | **GPIO 27 / GPIO 26** | Bay 3 Ultrasonic Sensor |
| **Bay 4 Sensor** | TRIG / ECHO | **GPIO 25 / GPIO 33** | Bay 4 Ultrasonic Sensor |
| **Bay 1 LEDs** | Green / Red | **GPIO 12 / GPIO 2** | Bay 1 Dual Status Indicator |
| **Bay 2 LEDs** | Green / Red | **GPIO 14 / GPIO 13** | Bay 2 Dual Status Indicator |
| **Bay 3 LEDs** | Green / Red | **GPIO 32 / GPIO 23** | Bay 3 Dual Status Indicator |
| **Bay 4 LEDs** | Green / Red | **GPIO 15 / GPIO 0** | Bay 4 Dual Status Indicator |

> **⚠️ Safe Wiring Note for Real Hardware:**  
> HC-SR04 Echo lines output 5V logic. Connect a voltage divider ($1\text{ k}\Omega$ and $2\text{ k}\Omega$) between each Echo pin and ESP32 GPIO to step down 5.0V to a safe 3.3V.

---

## 📐 Mathematical Modeling & Distance Logic

### 1. Speed of Sound & Time of Flight
The HC-SR04 emits an 8-cycle 40kHz acoustic burst. Sound speed in dry air at $20^\circ\text{C}$ is $v = 343\text{ m/s} = 0.0343\text{ cm}/\mu\text{s}$.

Since the pulse travels to the vehicle and returns:
$$\text{Distance (cm)} = \frac{\text{Echo Pulse Duration } (\mu\text{s}) \times 0.0343}{2} = \frac{\text{Duration } (\mu\text{s})}{58.3}$$

### 2. Occupancy Decision Threshold
$$\text{Status} = \begin{cases} \text{OCCUPIED}, & 1.0\text{ cm} \le \text{Distance} < 35.0\text{ cm} \\ \text{FREE}, & \text{Distance} \ge 35.0\text{ cm} \end{cases}$$

---

## 📁 Repository Structure

```
Smart-Parking-Ultrasonic-Embedded-System/
│
├── arduino_code/
│   ├── smart_parking_esp32/
│   │   └── smart_parking_esp32.ino    # Complete ESP32 Firmware with IoT Web UI
│   └── smart_parking_arduino_uno/
│       └── smart_parking_arduino_uno.ino # Lightweight Arduino UNO Sketch
│
├── simulation/
│   ├── diagram.json                  # Wokwi 4-Slot Circuit Schematic
│   ├── wokwi.toml                    # Wokwi Environment Config
│   └── README.md                     # 1-Click Simulation Instructions
│
├── circuit_diagram/
│   └── pin_mapping.md                # Pin Allocation & Level Shifting Schematics
│
├── docs/
│   ├── API_SPECIFICATION.md          # REST API Endpoints & Payloads
│   └── INTERVIEW_QA.md               # 10+ Technical Interview Q&A with deep insights
│
├── test_cases/
│   └── test_matrix.md                # System Validation Matrix & Edge Scenarios
│
├── reports/
│   └── PROJECT_REPORT.md             # Publication-grade Academic Project Report
│
├── .gitignore                        # Git exclusion rules
└── README.md                         # Master Documentation
```

---

## 🌐 Virtual Simulation Guide (Wokwi 1-Click)

1. Open [Wokwi ESP32 Simulator](https://wokwi.com/projects/new/esp32).
2. Paste [`arduino_code/smart_parking_esp32/smart_parking_esp32.ino`](arduino_code/smart_parking_esp32/smart_parking_esp32.ino) into `sketch.ino`.
3. Paste [`simulation/diagram.json`](simulation/diagram.json) into `diagram.json`.
4. Install libraries in Wokwi (`Adafruit SSD1306`, `Adafruit GFX Library`).
5. Click **Start Simulation (▶️)**.
6. Click any ultrasonic sensor to adjust its distance slider and observe real-time LED, OLED, Buzzer, and Servo reactions!

---

## 💻 Physical Hardware Setup & Flashing

1. Install **Arduino IDE** (v2.0+) and add the ESP32 board package (`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`).
2. Install dependencies via **Library Manager**:
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`
3. Wire the circuit following [circuit_diagram/pin_mapping.md](circuit_diagram/pin_mapping.md).
4. Connect ESP32 via Micro-USB, select `ESP32 Dev Module`, and choose your COM port.
5. Upload the code and open **Serial Monitor** at **115200 baud**.
6. Access the live Web Dashboard by navigating to the printed IP address in your browser.

---

## 📡 REST API Documentation

- **`GET /api/status`**: Returns real-time slot occupancy and distances in JSON.
- **`POST /api/gate/open`**: Manually actuates the barrier gate (if slots are available).

See [docs/API_SPECIFICATION.md](docs/API_SPECIFICATION.md) for full response schemas.

---

## 📋 Proof-Building Git Commit History

Build a credible GitHub development history using these step-by-step commits:

1. `feat: initial project structure and circuit schematics`
2. `feat: add HC-SR04 ultrasonic sensor driver with median filter`
3. `feat: implement bay occupancy detection and software debouncing`
4. `feat: integrate dual-LED slot indicators and active buzzer alerts`
5. `feat: add I2C SSD1306 OLED dynamic status matrix`
6. `feat: implement SG90 servo automated barrier gate control`
7. `feat: integrate embedded web dashboard and REST API telemetry`
8. `test: add comprehensive test cases and Wokwi simulation files`
9. `docs: add academic project report and technical interview guide`

---

## 🎓 Technical Interview Highlights

- **Acoustic Multipath & Cross-Talk:** Solved via sequential time-multiplexed pings and outlier-trimmed median filtering.
- **5V to 3.3V Logic Protection:** Addressed via resistive potential dividers on HC-SR04 Echo lines.
- **Non-Blocking Architecture:** Timed operations run via `millis()` and hardware LEDC PWM without blocking CPU loops.

---

## 📄 License
This project is open-source and licensed under the [MIT License](LICENSE).
