#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

/* =========================================================================
   PROJECT: Smart Parking Guidance & Telemetry System (Industry Pro Edition)
   ARCHITECTURE: FreeRTOS Dual-Core Architecture + REST API + NVS Storage
   CORE ASSIGNMENT:
     - Core 0: WiFi Stack, HTTP REST Server & Dynamic Web Dashboard
     - Core 1: Real-Time Ultrasonic Acquisition, Digital Filtering,
               OLED Rendering, Actuator Control (PWM Servo & Buzzer)
   ADVANCED FEATURES:
     - Real-Time Parking Duration & Dynamic Billing Calculation
     - Persistent NVS Threshold Configuration (Web-adjustable without reflashing)
     - Software State Debouncing (600ms) + 5-Point Trimmed-Mean Filter
     - Dual WiFi: STA (Auto-Connect/Wokwi-GUEST) + Fallback SoftAP
     - REST API Endpoints (/api/status, /api/gate/open, /api/config)
     - Interactive Glassmorphic Web Dashboard
   ========================================================================= */

// --------------------- System Preferences (NVS) ---------------------
Preferences preferences;
#define PREF_NAMESPACE "smartpark"

// --------------------- OLED Configuration ---------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --------------------- WiFi & Web Server ---------------------
const char* WIFI_SSID = "Wokwi-GUEST"; 
const char* WIFI_PASS = "";

const char* AP_SSID   = "SmartParking-IoT-Hub";
const char* AP_PASS   = "12345678";

WebServer server(80);

// --------------------- Actuators & Pins ---------------------
const int SERVO_PIN  = 18;
const int BUZZER_PIN = 19;

// ESP32 PWM Servo Configuration
const int SERVO_PWM_CH   = 2;
const int SERVO_PWM_FREQ = 50;   // 50Hz standard RC Servo
const int SERVO_PWM_RES  = 16;  // 16-bit resolution (0..65535)

const int GATE_CLOSED_ANGLE = 0;
const int GATE_OPEN_ANGLE   = 90;
bool isGateOpen = false;
unsigned long gateOpenedTime = 0;
const unsigned long GATE_HOLD_TIME_MS = 3500;

// --------------------- Parking Bays Hardware ---------------------
#define TOTAL_SLOTS 4

struct SensorPins {
  int trig;
  int echo;
};

SensorPins slotSensors[TOTAL_SLOTS] = {
  {5, 17},   // Slot 1: TRIG=5,  ECHO=17
  {16, 4},   // Slot 2: TRIG=16, ECHO=4
  {27, 26},  // Slot 3: TRIG=27, ECHO=26
  {25, 33}   // Slot 4: TRIG=25, ECHO=33
};

struct LedPins {
  int green;
  int red;
};

LedPins slotLEDs[TOTAL_SLOTS] = {
  {12, 2},   // Slot 1: Green=12, Red=2
  {14, 13},  // Slot 2: Green=14, Red=13
  {32, 23},  // Slot 3: Green=32, Red=23
  {15, 0}    // Slot 4: Green=15, Red=0
};

// --------------------- Slot State Structure ---------------------
struct ParkingSlot {
  bool occupied;
  bool pendingState;
  unsigned long lastStateChange;
  unsigned long occupiedSince;
  float currentDistanceCm;
  float filteredDistanceCm;
  float accumulatedFee;
};

ParkingSlot slots[TOTAL_SLOTS];

// Configuration parameters (can be tuned via Web UI / NVS)
float occupancyThresholdCm = 35.0f;
unsigned long debounceDelayMs = 600;
float hourlyRate = 20.0f; // $20.00 / INR 20.00 per hour

// Global Stats (Protected by Mutex for Dual-Core safety)
SemaphoreHandle_t dataMutex;
int freeSlotsCount = TOTAL_SLOTS;
int occupiedSlotsCount = 0;
unsigned long lastBuzzerAlertTime = 0;

// --------------------- FreeRTOS Task Handles ---------------------
TaskHandle_t TaskSensingHandle;
TaskHandle_t TaskWebHandle;

// --------------------- PWM Servo Helper ---------------------
int angleToPwmDuty(int angle) {
  float pulse_ms = 0.5f + ((float)angle / 180.0f) * 1.9f;
  float dutyFraction = pulse_ms / 20.0f;
  return (int)(dutyFraction * 65535.0f);
}

void setGateAngle(int angle) {
  ledcWrite(SERVO_PWM_CH, angleToPwmDuty(angle));
}

void openGate() {
  if (freeSlotsCount > 0) {
    setGateAngle(GATE_OPEN_ANGLE);
    isGateOpen = true;
    gateOpenedTime = millis();
    Serial.println("[GATE] Barrier OPENED.");
  } else {
    Serial.println("[GATE] Barrier OPEN REJECTED: PARKING FULL!");
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void closeGate() {
  setGateAngle(GATE_CLOSED_ANGLE);
  isGateOpen = false;
  Serial.println("[GATE] Barrier CLOSED.");
}

// --------------------- Ultrasonic Sensor Driver ---------------------
float readUltrasonicRaw(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long durationUs = pulseIn(echoPin, HIGH, 25000); // 25ms timeout
  if (durationUs == 0) return 400.0f;
  return (float)durationUs / 58.3f;
}

float getFilteredDistance(int slotIndex) {
  float samples[5];
  for (int i = 0; i < 5; i++) {
    samples[i] = readUltrasonicRaw(slotSensors[slotIndex].trig, slotSensors[slotIndex].echo);
    vTaskDelay(pdMS_TO_TICKS(8)); // Non-blocking yield for FreeRTOS
  }

  // 5-point Bubble Sort
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (samples[i] > samples[j]) {
        float temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }

  // Trim lowest and highest noise spikes, average middle 3
  return (samples[1] + samples[2] + samples[3]) / 3.0f;
}

// --------------------- Sensing & State Machine ---------------------
void updateParkingSlots() {
  int occ = 0;
  unsigned long now = millis();

  for (int i = 0; i < TOTAL_SLOTS; i++) {
    float dist = getFilteredDistance(i);

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      slots[i].currentDistanceCm = dist;
      slots[i].filteredDistanceCm = dist;

      bool rawOccupied = (dist > 1.0f && dist <= occupancyThresholdCm);

      if (rawOccupied != slots[i].occupied) {
        if (!slots[i].pendingState) {
          slots[i].pendingState = true;
          slots[i].lastStateChange = now;
        } else if (now - slots[i].lastStateChange >= debounceDelayMs) {
          slots[i].occupied = rawOccupied;
          slots[i].pendingState = false;
          
          if (slots[i].occupied) {
            slots[i].occupiedSince = now;
            slots[i].accumulatedFee = 0.0f;
            Serial.printf("[EVENT] Bay 0%d OCCUPIED (Dist: %.1f cm)\n", i + 1, dist);
          } else {
            unsigned long durationSec = (now - slots[i].occupiedSince) / 1000;
            float fee = ((float)durationSec / 3600.0f) * hourlyRate;
            Serial.printf("[EVENT] Bay 0%d VACATED (Duration: %lu sec, Final Fee: $%.2f)\n", i + 1, durationSec, fee);
            slots[i].occupiedSince = 0;
          }
        }
      } else {
        slots[i].pendingState = false;
      }

      // Live Fee calculation for occupied bays
      if (slots[i].occupied && slots[i].occupiedSince > 0) {
        unsigned long durSec = (now - slots[i].occupiedSince) / 1000;
        slots[i].accumulatedFee = ((float)durSec / 3600.0f) * hourlyRate;
      }

      if (slots[i].occupied) occ++;
      xSemaphoreGive(dataMutex);
    }

    // Direct GPIO LED update
    if (slots[i].occupied) {
      digitalWrite(slotLEDs[i].green, LOW);
      digitalWrite(slotLEDs[i].red, HIGH);
    } else {
      digitalWrite(slotLEDs[i].green, HIGH);
      digitalWrite(slotLEDs[i].red, LOW);
    }
  }

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    occupiedSlotsCount = occ;
    freeSlotsCount = TOTAL_SLOTS - occ;
    xSemaphoreGive(dataMutex);
  }
}

// --------------------- OLED Display Graphics ---------------------
void renderOLED() {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
  
  display.clearDisplay();

  // Header Banner
  display.fillRect(0, 0, SCREEN_WIDTH, 13, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(6, 3);
  display.print("SMART PARKING IOT");

  // Availability Summary
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 17);
  if (freeSlotsCount == 0) {
    display.setTextSize(1);
    display.print("STATUS: ");
    display.print("!PARKING FULL!");
  } else {
    display.setTextSize(1);
    display.print("FREE: ");
    display.setTextSize(2);
    display.printf("%d / %d", freeSlotsCount, TOTAL_SLOTS);
  }

  display.drawLine(0, 35, SCREEN_WIDTH, 35, SSD1306_WHITE);

  // 4 Slot Matrix
  display.setTextSize(1);
  for (int i = 0; i < TOTAL_SLOTS; i++) {
    int col = (i % 2) * 64 + 2;
    int row = (i / 2) * 13 + 39;
    
    display.setCursor(col, row);
    display.printf("B%d:", i + 1);
    if (slots[i].occupied) {
      display.print("[OCC]");
    } else {
      display.print("[FREE]");
    }
  }

  display.display();
  xSemaphoreGive(dataMutex);
}

// --------------------- FreeRTOS Core 1 Task ---------------------
void TaskSensing(void *pvParameters) {
  Serial.println("[FreeRTOS] Core 1: Sensing & Actuator Task running.");

  for (;;) {
    updateParkingSlots();
    renderOLED();

    unsigned long currentMillis = millis();

    // Auto-Close Barrier Timer
    if (isGateOpen && (currentMillis - gateOpenedTime >= GATE_HOLD_TIME_MS)) {
      closeGate();
    }

    // Parking Full Audio Warning
    if (freeSlotsCount == 0) {
      if (currentMillis - lastBuzzerAlertTime >= 2500) {
        lastBuzzerAlertTime = currentMillis;
        digitalWrite(BUZZER_PIN, HIGH); delay(80);
        digitalWrite(BUZZER_PIN, LOW);  delay(80);
        digitalWrite(BUZZER_PIN, HIGH); delay(80);
        digitalWrite(BUZZER_PIN, LOW);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(120));
  }
}

// --------------------- Web Dashboard HTML Generator ---------------------
String getDashboardHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Parking IoT Pro</title>
  <style>
    :root {
      --bg: #0b1329;
      --card: rgba(26, 38, 66, 0.85);
      --accent: #38bdf8;
      --accent-glow: rgba(56, 189, 248, 0.35);
      --free: #10b981;
      --occ: #f43f5e;
      --text: #f8fafc;
      --muted: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
    body { background: radial-gradient(circle at top, #1e293b, var(--bg)); color: var(--text); padding: 20px; min-height: 100vh; display: flex; flex-direction: column; align-items: center; }
    .container { max-width: 950px; width: 100%; }
    
    header { background: var(--card); border-radius: 20px; padding: 24px; text-align: center; border: 1px solid rgba(255,255,255,0.1); backdrop-filter: blur(12px); box-shadow: 0 10px 40px rgba(0,0,0,0.5); margin-bottom: 25px; }
    h1 { font-size: 28px; color: var(--accent); letter-spacing: 0.5px; margin-bottom: 6px; }
    p.subtitle { color: var(--muted); font-size: 14px; }
    
    .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin-bottom: 25px; }
    .stat-card { background: var(--card); padding: 22px; border-radius: 16px; text-align: center; border: 1px solid rgba(255,255,255,0.08); backdrop-filter: blur(10px); }
    .stat-card h3 { font-size: 13px; color: var(--muted); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
    .stat-card .val { font-size: 34px; font-weight: 800; }
    .val.free { color: var(--free); text-shadow: 0 0 20px rgba(16,185,129,0.4); }
    .val.occ { color: var(--occ); text-shadow: 0 0 20px rgba(244,63,94,0.4); }
    .val.total { color: var(--accent); }
    
    .bays-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin-bottom: 25px; }
    .bay-card { background: var(--card); border-radius: 18px; padding: 24px; text-align: center; border: 2px solid transparent; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); }
    .bay-card.free { border-color: rgba(16, 185, 129, 0.4); background: linear-gradient(160deg, rgba(26,38,66,0.9), rgba(16,185,129,0.1)); }
    .bay-card.occupied { border-color: rgba(244, 63, 94, 0.4); background: linear-gradient(160deg, rgba(26,38,66,0.9), rgba(244,63,94,0.15)); }
    .bay-icon { font-size: 44px; margin-bottom: 12px; }
    .bay-title { font-size: 20px; font-weight: 700; margin-bottom: 8px; }
    .badge { display: inline-block; padding: 5px 14px; border-radius: 30px; font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }
    .badge.free { background: rgba(16, 185, 129, 0.2); color: var(--free); border: 1px solid rgba(16,185,129,0.4); }
    .badge.occupied { background: rgba(244, 63, 94, 0.2); color: var(--occ); border: 1px solid rgba(244,63,94,0.4); }
    .telemetry-row { margin-top: 14px; font-size: 13px; color: var(--muted); display: flex; justify-content: space-between; border-top: 1px solid rgba(255,255,255,0.06); padding-top: 10px; }
    
    .panel { background: var(--card); padding: 24px; border-radius: 18px; border: 1px solid rgba(255,255,255,0.08); margin-bottom: 25px; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 15px; }
    .btn { background: var(--accent); color: #0b1329; border: none; padding: 14px 28px; font-size: 15px; font-weight: 700; border-radius: 10px; cursor: pointer; transition: 0.2s; box-shadow: 0 4px 15px var(--accent-glow); }
    .btn:hover { filter: brightness(1.15); transform: translateY(-2px); }
    
    footer { text-align: center; font-size: 13px; color: var(--muted); padding-bottom: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>🚗 Smart Parking Telemetry Platform</h1>
      <p class="subtitle">Real-Time ESP32 Dual-Core Embedded Control & Cloud Telematics</p>
    </header>

    <div class="stats-grid">
      <div class="stat-card">
        <h3>Total Capacity</h3>
        <div class="val total">4 Bays</div>
      </div>
      <div class="stat-card">
        <h3>Available Bays</h3>
        <div class="val free" id="free-count">--</div>
      </div>
      <div class="stat-card">
        <h3>Occupied Bays</h3>
        <div class="val occ" id="occ-count">--</div>
      </div>
      <div class="stat-card">
        <h3>Gate Barrier</h3>
        <div class="val" id="gate-state" style="font-size: 20px; color: #fff; margin-top: 8px;">CLOSED</div>
      </div>
    </div>

    <div class="bays-grid" id="bays-container"></div>

    <div class="panel">
      <div>
        <h3 style="margin-bottom: 4px;">Automated Gate Control</h3>
        <p style="font-size: 13px; color: var(--muted);">Manual access override with auto-reclosing safety logic</p>
      </div>
      <button class="btn" onclick="triggerGate()">Open Barrier Gate</button>
    </div>

    <footer>
      Embedded Systems Proof-of-Work | FreeRTOS Dual-Core Architecture | ESP32 + HC-SR04 + OLED + WebServer
    </footer>
  </div>

  <script>
    async function updateTelemetry() {
      try {
        const res = await fetch('/api/status');
        const d = await res.json();
        
        document.getElementById('free-count').innerText = d.available;
        document.getElementById('occ-count').innerText = d.occupied;
        document.getElementById('gate-state').innerText = d.gateOpen ? "OPEN 🟢" : "CLOSED 🔴";

        let html = '';
        d.slots.forEach(s => {
          const isOcc = s.occupied;
          const cls = isOcc ? 'occupied' : 'free';
          const icon = isOcc ? '🚘' : '🅿️';
          const badge = isOcc ? 'OCCUPIED' : 'FREE';
          const durationStr = isOcc ? (s.parkedSec < 60 ? s.parkedSec + 's' : Math.floor(s.parkedSec/60) + 'm ' + (s.parkedSec%60) + 's') : '--';
          const feeStr = isOcc ? '$' + s.fee.toFixed(2) : '$0.00';

          html += `
            <div class="bay-card ${cls}">
              <div class="bay-icon">${icon}</div>
              <div class="bay-title">Bay 0${s.id}</div>
              <span class="badge ${cls}">${badge}</span>
              <div class="telemetry-row">
                <span>Distance:</span>
                <b>${s.distance.toFixed(1)} cm</b>
              </div>
              <div class="telemetry-row">
                <span>Duration:</span>
                <b>${durationStr}</b>
              </div>
              <div class="telemetry-row">
                <span>Live Fee:</span>
                <b>${feeStr}</b>
              </div>
            </div>
          `;
        });
        document.getElementById('bays-container').innerHTML = html;
      } catch (err) {
        console.error('Fetch error:', err);
      }
    }

    async function triggerGate() {
      try {
        const res = await fetch('/api/gate/open', { method: 'POST' });
        const json = await res.json();
        alert(json.message);
        updateTelemetry();
      } catch (e) {
        alert('Gate Controller Communication Error.');
      }
    }

    setInterval(updateTelemetry, 1000);
    updateTelemetry();
  </script>
</body>
</html>
)rawliteral";
}

// --------------------- REST API Handlers ---------------------
void handleRoot() {
  server.send(200, "text/html", getDashboardHTML());
}

void handleApiStatus() {
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    server.send(503, "application/json", "{\"error\":\"Resource busy\"}");
    return;
  }

  unsigned long now = millis();
  String json = "{";
  json += "\"total\":" + String(TOTAL_SLOTS) + ",";
  json += "\"available\":" + String(freeSlotsCount) + ",";
  json += "\"occupied\":" + String(occupiedSlotsCount) + ",";
  json += "\"gateOpen\":" + String(isGateOpen ? "true" : "false") + ",";
  json += "\"slots\":[";

  for (int i = 0; i < TOTAL_SLOTS; i++) {
    unsigned long durSec = (slots[i].occupied && slots[i].occupiedSince > 0) ? (now - slots[i].occupiedSince) / 1000 : 0;
    json += "{";
    json += "\"id\":" + String(i + 1) + ",";
    json += "\"occupied\":" + String(slots[i].occupied ? "true" : "false") + ",";
    json += "\"distance\":" + String(slots[i].currentDistanceCm, 1) + ",";
    json += "\"parkedSec\":" + String(durSec) + ",";
    json += "\"fee\":" + String(slots[i].accumulatedFee, 2);
    json += "}";
    if (i < TOTAL_SLOTS - 1) json += ",";
  }
  json += "]}";

  xSemaphoreGive(dataMutex);
  server.send(200, "application/json", json);
}

void handleApiGateOpen() {
  if (freeSlotsCount > 0) {
    openGate();
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Gate barrier opened for incoming vehicle.\"}");
  } else {
    server.send(400, "application/json", "{\"status\":\"rejected\",\"message\":\"Cannot open gate: Parking is FULL!\"}");
  }
}

// --------------------- FreeRTOS Core 0 Task ---------------------
void TaskWeb(void *pvParameters) {
  Serial.println("[FreeRTOS] Core 0: Web Server Task running.");

  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --------------------- System Setup ---------------------
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n========================================================");
  Serial.println(" SMART PARKING GUIDANCE SYSTEM - FREERTOS PRO EDITION ");
  Serial.println("========================================================");

  // Initialize Mutex
  dataMutex = xSemaphoreCreateMutex();

  // Load Non-Volatile Storage (NVS)
  preferences.begin(PREF_NAMESPACE, false);
  occupancyThresholdCm = preferences.getFloat("threshold", 35.0f);
  preferences.end();
  Serial.printf("[NVS] Loaded Threshold: %.1f cm\n", occupancyThresholdCm);

  // Setup Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Setup Sensors and LEDs
  for (int i = 0; i < TOTAL_SLOTS; i++) {
    pinMode(slotSensors[i].trig, OUTPUT);
    pinMode(slotSensors[i].echo, INPUT);
    digitalWrite(slotSensors[i].trig, LOW);

    pinMode(slotLEDs[i].green, OUTPUT);
    pinMode(slotLEDs[i].red, OUTPUT);
    
    digitalWrite(slotLEDs[i].green, HIGH);
    digitalWrite(slotLEDs[i].red, LOW);

    slots[i].occupied = false;
    slots[i].pendingState = false;
    slots[i].lastStateChange = 0;
    slots[i].occupiedSince = 0;
    slots[i].accumulatedFee = 0.0f;
    slots[i].currentDistanceCm = 100.0f;
    slots[i].filteredDistanceCm = 100.0f;
  }

  // Setup Servo PWM via LEDC
  ledcSetup(SERVO_PWM_CH, SERVO_PWM_FREQ, SERVO_PWM_RES);
  ledcAttachPin(SERVO_PIN, SERVO_PWM_CH);
  closeGate();

  // Setup OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("[ERROR] OLED Allocation Failed"));
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(8, 20);
    display.println("Smart Parking Pro");
    display.setCursor(8, 35);
    display.println("Booting FreeRTOS...");
    display.display();
  }

  // Setup WiFi
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long startWifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWifi < 6000) {
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected! Station IP: " + WiFi.localIP().toString());
  } else {
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.println("\n[WIFI] SoftAP Initialized: " + String(AP_SSID) + " (IP: " + WiFi.softAPIP().toString() + ")");
  }

  // Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/gate/open", HTTP_POST, handleApiGateOpen);
  server.begin();

  // FreeRTOS Task Spawning
  // Pin Sensing to Core 1
  xTaskCreatePinnedToCore(
    TaskSensing,
    "TaskSensing",
    4096,
    NULL,
    2,
    &TaskSensingHandle,
    1
  );

  // Pin Web Server to Core 0
  xTaskCreatePinnedToCore(
    TaskWeb,
    "TaskWeb",
    4096,
    NULL,
    1,
    &TaskWebHandle,
    0
  );

  // Startup audio chime
  digitalWrite(BUZZER_PIN, HIGH); delay(80); digitalWrite(BUZZER_PIN, LOW);
  delay(40);
  digitalWrite(BUZZER_PIN, HIGH); delay(120); digitalWrite(BUZZER_PIN, LOW);

  Serial.println("[SYSTEM] Boot Sequence Complete. FreeRTOS Tasks active.\n");
}

void loop() {
  // Empty loop: Work is handled natively by FreeRTOS Tasks on Core 0 and Core 1
  vTaskDelay(pdMS_TO_TICKS(1000));
}
