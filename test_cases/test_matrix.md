# 🧪 Test Cases & Verification Matrix

| TC ID | Test Scenario | Input Conditions | Expected Behavior | Verification Point | Pass/Fail Criteria |
|---|---|---|---|---|---|
| **TC-01** | All Slots Free | S1=120cm, S2=110cm, S3=130cm, S4=100cm | Free Count = 4/4; All 4 Green LEDs ON; 4 Red LEDs OFF; Gate Servo = 90° (Open) | OLED & Web API show 4 available | Green LEDs illuminate, OLED shows 4/4 |
| **TC-02** | Single Slot Occupied (Bay 1) | S1=15cm, S2=100cm, S3=120cm, S4=130cm | S1 Green OFF, S1 Red ON; Free Count = 3/4; OLED S1:[OCC] | S1 Red LED turns ON within 800ms | Slot 1 toggles state accurately |
| **TC-03** | Multiple Slots Occupied (Bays 1 & 3) | S1=12cm, S2=95cm, S3=18cm, S4=110cm | S1 & S3 Red LEDs ON; S2 & S4 Green LEDs ON; Free Count = 2/4 | OLED shows `AVAILABLE: 2 / 4` | Accurate count on display & web API |
| **TC-04** | Parking Full Condition | S1=10cm, S2=14cm, S3=8cm, S4=16cm | Free Count = 0/4; All Red LEDs ON; OLED shows `!PARKING FULL!`; Buzzer emits warning beeps; Servo = 0° (Closed) | Gate locked, buzzer active | No vehicles allowed entry |
| **TC-05** | Vehicle Exit (Full -> 1 Free) | S1 changes from 10cm to 120cm | S1 transitions to Free (Green ON); Free Count = 1/4; Buzzer stops; Gate Servo re-opens to 90° | Immediate recovery within debounce window | Buzzer stops immediately |
| **TC-06** | Noisy / Glitch Sensor Reading | S2 drops to 5cm for a single 20ms read, then returns to 120cm | Filter & Debouncer suppresses transient noise; S2 remains FREE | No false state change on OLED or LED | Filter successfully discards single spike |
| **TC-07** | Sensor Timeout / Disconnected Echo | Echo pin floating / disconnected (pulseIn timeout) | Distance defaults to safe out-of-range value (400cm); Slot treated as FREE with debug warning | System does not freeze or lock watchdog | FreeRTOS / loop continues smoothly |
| **TC-08** | REST API Query `/api/status` | HTTP GET request sent to ESP32 IP | Returns valid JSON payload containing total, available, occupied counts and array of bay distances | Browser / Postman receives HTTP 200 OK | Response time < 50ms |
| **TC-09** | Remote Gate Control `/api/gate/open` | HTTP POST request sent to ESP32 IP | Opens gate if slots available; Returns JSON `{ "status": "success" }` | Servo arm actuates to 90° for 3 seconds | Auto-closes after hold delay |
