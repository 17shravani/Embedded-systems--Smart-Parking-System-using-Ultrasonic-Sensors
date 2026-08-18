# 🔌 Hardware Pin Mapping & Circuit Schematic Documentation

## 1. Complete ESP32 Pin Allocation Table

| Component | Component Pin | ESP32 GPIO | Logic Level | Function / Notes |
|---|---|---|---|---|
| **OLED (SSD1306)** | SDA | **GPIO 21** | 3.3V | I2C Data line |
| **OLED (SSD1306)** | SCL | **GPIO 22** | 3.3V | I2C Clock line |
| **OLED (SSD1306)** | VCC / GND | 3.3V / GND | Power | OLED logic power supply |
| **Barrier Servo (SG90)** | PWM Signal | **GPIO 18** | 3.3V/5V | Hardware PWM via LEDC Channel 2 |
| **Barrier Servo (SG90)** | V+ / GND | 5V (VIN) / GND | 5V Power | High current branch; use shared GND |
| **Piezo Buzzer** | Positive (+) | **GPIO 19** | 3.3V | Active Buzzer driving pin |
| **Piezo Buzzer** | Negative (-) | GND | Ground | Common Ground |
| **Ultrasonic Bay 1** | TRIG | **GPIO 5** | 3.3V | Output 10µs trigger pulse |
| **Ultrasonic Bay 1** | ECHO | **GPIO 17** | 3.3V (Divider) | High pulse echo detection |
| **Ultrasonic Bay 2** | TRIG | **GPIO 16** | 3.3V | Output 10µs trigger pulse |
| **Ultrasonic Bay 2** | ECHO | **GPIO 4** | 3.3V (Divider) | High pulse echo detection |
| **Ultrasonic Bay 3** | TRIG | **GPIO 27** | 3.3V | Output 10µs trigger pulse |
| **Ultrasonic Bay 3** | ECHO | **GPIO 26** | 3.3V (Divider) | High pulse echo detection |
| **Ultrasonic Bay 4** | TRIG | **GPIO 25** | 3.3V | Output 10µs trigger pulse |
| **Ultrasonic Bay 4** | ECHO | **GPIO 33** | 3.3V (Divider) | High pulse echo detection |
| **Slot 1 Green LED** | Anode (+) | **GPIO 12** | 3.3V via 220Ω | FREE status indicator |
| **Slot 1 Red LED** | Anode (+) | **GPIO 2** | 3.3V via 220Ω | OCCUPIED status indicator |
| **Slot 2 Green LED** | Anode (+) | **GPIO 14** | 3.3V via 220Ω | FREE status indicator |
| **Slot 2 Red LED** | Anode (+) | **GPIO 13** | 3.3V via 220Ω | OCCUPIED status indicator |
| **Slot 3 Green LED** | Anode (+) | **GPIO 32** | 3.3V via 220Ω | FREE status indicator |
| **Slot 3 Red LED** | Anode (+) | **GPIO 23** | 3.3V via 220Ω | OCCUPIED status indicator |
| **Slot 4 Green LED** | Anode (+) | **GPIO 15** | 3.3V via 220Ω | FREE status indicator |
| **Slot 4 Red LED** | Anode (+) | **GPIO 0** | 3.3V via 220Ω | OCCUPIED status indicator |

---

## 2. Voltage Level Shifting on Echo Pins (Real Hardware Safety)

HC-SR04 sensors powered by 5V return a **5.0V TTL logic level** on their `ECHO` pins. Because the ESP32 GPIO inputs are rated for **3.3V maximum**, direct connection can degrade or damage GPIO silicon over time.

### Voltage Divider Circuit:
```
HC-SR04 ECHO (5V) ----[ 1 kΩ Resistor ]----+---- ESP32 GPIO (3.3V Safe)
                                            |
                                    [ 2 kΩ Resistor ]
                                            |
                                           GND
```
**Formula:**
$$V_{out} = V_{in} \times \frac{R_2}{R_1 + R_2} = 5.0\text{V} \times \frac{2\text{k}\Omega}{1\text{k}\Omega + 2\text{k}\Omega} = 3.33\text{V}$$

---

## 3. Sensor Placement & Acoustic Isolation Guidelines
1. **Mounting Height:** $25\text{ cm} - 35\text{ cm}$ above floor level (aligned with vehicle bumper center).
2. **Mounting Angle:** $90^\circ$ perpendicular to vehicle surface to maximize echo reflection back to the transducer.
3. **Sensor Cones & Spacing:** Minimum $30\text{ cm}$ lateral separation between adjacent parking sensors to prevent acoustic cross-talk interference.
