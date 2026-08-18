# 🌐 Virtual Simulation Guide (Wokwi & Tinkercad)

This guide provides step-by-step instructions to run the **Smart Parking System** virtually without physical hardware.

---

## ⚡ Option 1: Run in Wokwi (Recommended - 1 Click)

1. Open [https://wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32)
2. In the code tab (`sketch.ino`), copy & paste all contents of [`arduino_code/smart_parking_esp32/smart_parking_esp32.ino`](../arduino_code/smart_parking_esp32/smart_parking_esp32.ino).
3. In the `diagram.json` tab, replace everything with [`simulation/diagram.json`](diagram.json).
4. In the `Library Manager` tab (left side panel), add:
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`
5. Click **Start Simulation (Play button ▶️)**.

---

## 🧪 Simulation Verification Steps

| Step | Action | Expected Output | Proof Screenshot |
|---|---|---|---|
| **1** | All 4 sensors set to default `120 cm` | OLED shows `AVAILABLE: 4 / 4`, all 4 **Green LEDs ON**, Red LEDs OFF, Servo Gate **OPEN** | `screenshots/04_all_slots_free.png` |
| **2** | Click **Sensor 1**, slide distance slider to `< 35 cm` (e.g. `15 cm`) | Slot 1 **Red LED turns ON**, Green LED OFF. OLED updates to `AVAILABLE: 3 / 4`, `S1:[OCC]` | `screenshots/05_slot1_occupied.png` |
| **3** | Set **Sensor 2 & 3** to `< 35 cm` | Slots 1, 2, 3 Red LEDs ON. OLED updates to `AVAILABLE: 1 / 4` | `screenshots/07_three_slots_occupied.png` |
| **4** | Set **Sensor 4** to `< 35 cm` (All 4 occupied) | OLED flashes `STATUS: !PARKING FULL!`, all **Red LEDs ON**, Buzzer chirps warning alert, Servo Barrier moves to `0°` (Closed) | `screenshots/08_parking_full_alert.png` |
| **5** | Reset **Sensor 1** distance to `80 cm` | Slot 1 Green LED turns ON. OLED returns to `AVAILABLE: 1 / 4`, Buzzer stops, Servo opens | `screenshots/09_slot_freed.png` |

---

## 📱 Web Dashboard in Wokwi

Wokwi supports simulated WiFi through the private network `Wokwi-GUEST`.
When connected, open the simulated browser window or local link provided in the serial console to view the real-time glassmorphic dashboard!
