// ============================================================
// plant2.ino FINAL — WWTP Plant 2 RTU
// Discharge Regulation & Monitoring
// ============================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────
// KONEKSI
// ─────────────────────────────────────────────────────────────
WiFiClientSecure wifiClient;
PubSubClient     mqtt(wifiClient);

// ─────────────────────────────────────────────────────────────
// STATE AKTUATOR
// ─────────────────────────────────────────────────────────────
bool valveMainState = false;
bool valveFsState   = false;

// ─────────────────────────────────────────────────────────────
// FLOW SENSOR — interrupt
// ─────────────────────────────────────────────────────────────
volatile uint32_t flowPulseCount  = 0;
float             lastFlowLPM     = 0.0f;
float             lastFlowSent    = -999.0f;
unsigned long     valveMainOpenMs = 0;
bool              flowFaultSent   = false;
bool              overflowSent    = false;

void IRAM_ATTR flowISR() { flowPulseCount++; }

float calculateFlowLPM(float dt_seconds) {
  noInterrupts();
  uint32_t pulses   = flowPulseCount;
  flowPulseCount    = 0;
  interrupts();
  if (dt_seconds <= 0.0f) return 0.0f;
  float freqHz = (float)pulses / dt_seconds;
  float lpm    = freqHz / YFS201_FACTOR;
  Serial.printf("[FLOW] Pulsa=%lu | Freq=%.2fHz | Flow=%.3f L/min\n",
                (unsigned long)pulses, freqHz, lpm);
  return lpm;
}

// ─────────────────────────────────────────────────────────────
// STATE PUBLISH
// ─────────────────────────────────────────────────────────────
float         lastLevel    = -999.0f;
unsigned long lastSendMs   = 0;
unsigned long lastSampleMs = 0;

// ─────────────────────────────────────────────────────────────
// OFFLINE BUFFER
// ─────────────────────────────────────────────────────────────
struct BufEntry { String topic; String json; };
BufEntry offBuf[OFFLINE_BUF_SIZE];
int bufHead = 0, bufTail = 0, bufCount = 0;

// Forward declarations
void connectWiFi();
void connectMQTT();
void onMQTTMessage(char*, byte*, unsigned int);
void publishOrBuffer(String, String);
String buildAlertJson(String, String);
void setValveMain(bool);
void setValveFs(bool);

// =============================================================
// BAGIAN 1: SENSOR LEVEL — LOOKUP TABLE VOLUMETRIK
// =============================================================
float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) {
    Serial.println("[LEVEL] HC-SR04 timeout — tidak ada pantulan");
    return -1.0f;
  }
  float d = dur * 0.0343f / 2.0f;
  Serial.printf("[LEVEL] Jarak: %.2f cm\n", d);
  return d;
}

float distanceToVolume(float distCM) {
  if (distCM < 0) return -1.0f;
  int n = VOL_TABLE_SIZE;
  if (distCM >= VOL_TABLE[0][0])   return VOL_TABLE[0][1];
  if (distCM <= VOL_TABLE[n-1][0]) return VOL_TABLE[n-1][1];
  for (int i = 0; i < n-1; i++) {
    if (distCM <= VOL_TABLE[i][0] && distCM >= VOL_TABLE[i+1][0]) {
      float t = (distCM-VOL_TABLE[i][0])/(VOL_TABLE[i+1][0]-VOL_TABLE[i][0]);
      return VOL_TABLE[i][1] + t*(VOL_TABLE[i+1][1]-VOL_TABLE[i][1]);
    }
  }
  return -1.0f;
}

float volumeToPercent(float volL) {
  float maxVol = VOL_TABLE[VOL_TABLE_SIZE-1][1];
  if (maxVol <= 0) return -1.0f;
  return constrain((volL/maxVol)*100.0f, 0.0f, 100.0f);
}

// =============================================================
// BAGIAN 2: STATUS STRING
// =============================================================
String levelStatus(float pct) {
  if (pct < 0)                  return "Fault";
  if (pct <= LEVEL_MAIN_CLOSE)  return "Critical_Low";
  if (pct < 20.0f)              return "Warning_Low";
  if (pct >= LEVEL_FS_OPEN)     return "Critical_High";
  if (pct >= 70.0f)             return "Warning_High";
  return "Normal";
}

String flowStatus(float lpm) {
  if (lpm < 0)                              return "Fault";
  if (lpm > FLOW_MAX_LPM)                   return "Overflow";
  if (lpm < 0.1f && valveMainState)         return "No_Flow";
  return "Normal";
}

// =============================================================
// BAGIAN 3: AKTUATOR — HIGH-Z RELAY
// =============================================================
void relayOn(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("[RELAY] Pin %d → ON (OUTPUT LOW)\n", pin);
}
void relayOff(int pin) {
  pinMode(pin, INPUT);
  Serial.printf("[RELAY] Pin %d → OFF (High-Z INPUT)\n", pin);
}

void setValveMain(bool open) {
  if (valveMainState == open) return;
  valveMainState = open;
  open ? relayOn(RELAY_MAIN_PIN) : relayOff(RELAY_MAIN_PIN);
  Serial.printf("[VALVE-MAIN] ══► %s\n", open ? "BUKA ✓" : "TUTUP ✓");
  if (open) {
    valveMainOpenMs = millis();
    flowFaultSent   = false;
    Serial.println("[VALVE-MAIN] Timer flow fault dimulai");
  }
}

void setValveFs(bool open) {
  if (valveFsState == open) return;
  valveFsState = open;
  open ? relayOn(RELAY_FS_PIN) : relayOff(RELAY_FS_PIN);
  Serial.printf("[VALVE-FS]   ══► %s\n", open ? "BUKA ✓" : "TUTUP ✓");
}

void updateValveLogic(float pct, bool levelFault) {
  // FAULT — tutup semua
  if (levelFault) {
    if (valveMainState || valveFsState) {
      Serial.println("[FAULT] Level sensor error — tutup semua valve (RTU failsafe)");
      setValveMain(false);
      setValveFs(false);
      publishOrBuffer(TOPIC_ALERT, buildAlertJson(
        "LEVEL_SENSOR_FAULT",
        "Sensor level error — kedua valve ditutup paksa"
      ));
    }
  }
}

// =============================================================
// BAGIAN 5: FLOW ALERT LOGIC
// =============================================================
void updateFlowAlerts(float lpm) {
  unsigned long now = millis();

  // Flow = 0 padahal valve sudah buka > FLOW_FAULT_MS
  if (valveMainState && lpm < 0.1f) {
    if (!flowFaultSent && (now - valveMainOpenMs) >= FLOW_FAULT_MS) {
      flowFaultSent = true;
      Serial.println("[FLOW-ALERT] !! Flow = 0 padahal valve sudah buka > 5 detik");
      publishOrBuffer(TOPIC_ALERT, buildAlertJson(
        "FLOW_FAULT",
        "Flow = 0 L/menit padahal valve utama sudah buka > 5 detik"
      ));
    }
  } else {
    if (flowFaultSent) {
      flowFaultSent = false;
      Serial.println("[FLOW-ALERT] Flow kembali normal — reset fault");
    }
  }

  // Flow overflow
  if (lpm > FLOW_MAX_LPM) {
    if (!overflowSent) {
      overflowSent = true;
      Serial.printf("[FLOW-ALERT] !! Overflow: %.2f L/min > %.0f L/min\n",
                    lpm, FLOW_MAX_LPM);
      publishOrBuffer(TOPIC_ALERT, buildAlertJson(
        "FLOW_OVERFLOW",
        String("Flow=") + String(lpm,1) +
        " L/menit melebihi batas " + String(FLOW_MAX_LPM,0) + " L/menit"
      ));
    }
  } else {
    if (overflowSent) {
      overflowSent = false;
      Serial.println("[FLOW-ALERT] Flow kembali normal — reset overflow");
    }
  }
}

// =============================================================
// BAGIAN 6: JSON BUILDERS
// =============================================================
String buildLevelJson(float distCM, float volL, float pct, String status) {
  JsonDocument doc;
  doc["plant_id"]    = PLANT_ID;
  doc["sensor_type"] = "level";
  doc["value_cm"]    = serialized(String(distCM, 1));
  doc["value_liter"] = serialized(String(volL, 3));
  doc["value_pct"]   = serialized(String(pct, 1));
  doc["unit"]        = "liter";
  doc["status"]      = status;
  String out; serializeJson(doc, out); return out;
}

String buildFlowJson(float lpm, String status) {
  JsonDocument doc;
  doc["plant_id"]    = PLANT_ID;
  doc["sensor_type"] = "flow";
  doc["value"]       = serialized(String(lpm, 2));
  doc["unit"]        = "L/min";
  doc["status"]      = status;
  String out; serializeJson(doc, out); return out;
}

String buildValveJson(String device, bool state) {
  JsonDocument doc;
  doc["plant_id"] = PLANT_ID;
  doc["device"]   = device;
  doc["state"]    = state ? "OPEN" : "CLOSED";
  String out; serializeJson(doc, out); return out;
}

String buildAlertJson(String code, String message) {
  JsonDocument doc;
  doc["plant_id"]   = PLANT_ID;
  doc["code"]       = code;
  doc["message"]    = message;
  doc["valve_main"] = valveMainState ? "OPEN" : "CLOSED";
  doc["valve_fs"]   = valveFsState   ? "OPEN" : "CLOSED";
  String out; serializeJson(doc, out); return out;
}

// =============================================================
// BAGIAN 7: MQTT & BUFFER
// =============================================================
void bufferMsg(String topic, String json) {
  offBuf[bufHead] = {topic, json};
  bufHead = (bufHead+1) % OFFLINE_BUF_SIZE;
  if (bufCount < OFFLINE_BUF_SIZE) bufCount++;
  else bufTail = (bufTail+1) % OFFLINE_BUF_SIZE;
}

void flushBuffer() {
  if (bufCount == 0) return;
  Serial.printf("[BUFFER] Flushing %d entri offline...\n", bufCount);
  while (bufCount > 0 && mqtt.connected()) {
    mqtt.publish(offBuf[bufTail].topic.c_str(),
                 offBuf[bufTail].json.c_str(), false);
    bufTail = (bufTail+1) % OFFLINE_BUF_SIZE;
    bufCount--; delay(50);
  }
  Serial.println("[BUFFER] Flush selesai");
}

void publishOrBuffer(String topic, String json) {
  if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
    flushBuffer();
    mqtt.publish(topic.c_str(), json.c_str(), false);
    Serial.printf("[MQTT→] %s\n       %s\n", topic.c_str(), json.c_str());
  } else {
    bufferMsg(topic, json);
    Serial.printf("[BUFFER] Disimpan offline (%d/%d): %s\n",
                  bufCount, OFFLINE_BUF_SIZE, topic.c_str());
  }
}

void onMQTTMessage(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("\n[MQTT←] Topic: %s\n        Data : %s\n", topic, msg.c_str());

  JsonDocument doc;
  if (deserializeJson(doc, msg) != DeserializationError::Ok) {
    Serial.println("[MQTT←] ERROR: JSON parse gagal");
    return;
  }

  // Command valve utama
  if (String(topic) == TOPIC_CMD_MAIN) {
    Serial.println("[CMD-MAIN] Menerima perintah valve utama dari MTU...");
    if (doc.containsKey("state")) {
      setValveMain(doc["state"].as<String>() == "OPEN");
    }
    publishOrBuffer(TOPIC_VALVE_MAIN,
                    buildValveJson("main_valve", valveMainState));
    return;
  }

  // Command valve failsafe
  if (String(topic) == TOPIC_CMD_FS) {
    Serial.println("[CMD-FS] Menerima perintah valve failsafe dari MTU...");
    if (doc.containsKey("state")) {
      setValveFs(doc["state"].as<String>() == "OPEN");
    }
    publishOrBuffer(TOPIC_VALVE_FS,
                    buildValveJson("failsafe_valve", valveFsState));
    return;
  }

  Serial.printf("[MQTT←] Topic tidak dikenal: %s\n", topic);
}

// =============================================================
// BAGIAN 8: KONEKSI
// =============================================================
void connectWiFi() {
  Serial.printf("\n[WIFI] Menghubungkan ke '%s'", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n[WIFI] ✓ Terhubung | IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n[WIFI] ✗ Gagal — mode offline aktif");
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  int tries = 0;
  while (!mqtt.connected() && tries < 5) {
    Serial.printf("[MQTT] Menghubungkan (percobaan %d/5)...\n", tries+1);
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println("[MQTT] ✓ Terhubung ke broker");
      mqtt.subscribe(TOPIC_CMD_MAIN);
      Serial.printf("[MQTT] Subscribe: %s\n", TOPIC_CMD_MAIN);
      mqtt.subscribe(TOPIC_CMD_FS);
      Serial.printf("[MQTT] Subscribe: %s\n", TOPIC_CMD_FS);
    } else {
      Serial.printf("[MQTT] ✗ Gagal rc=%d — retry dalam 2s\n", mqtt.state());
      delay(2000); tries++;
    }
  }
  if (!mqtt.connected())
    Serial.println("[MQTT] ✗ Tidak berhasil konek — mode offline");
}

// =============================================================
// SETUP & LOOP
// =============================================================
void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("\n╔══════════════════════════════════╗");
  Serial.println("║   Plant 2 RTU — WWTP SCADA      ║");
  Serial.println("║   Discharge Regulation           ║");
  Serial.println("╚══════════════════════════════════╝\n");

  Serial.println("[SETUP] Inisialisasi pin...");
  pinMode(TRIG_PIN,      OUTPUT);
  pinMode(ECHO_PIN,      INPUT);
  pinMode(FLOW_PIN,      INPUT_PULLUP);

  // Relay: semua mulai High-Z (OFF) — failsafe
  pinMode(RELAY_MAIN_PIN, INPUT);
  pinMode(RELAY_FS_PIN,   INPUT);
  valveMainState = false;
  valveFsState   = false;
  Serial.println("[SETUP] Semua relay → High-Z (OFF) ✓");

  // Flow sensor interrupt
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowISR, RISING);
  Serial.println("[SETUP] Flow sensor interrupt terpasang ✓");

  wifiClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMQTTMessage);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);

  Serial.println("[SETUP] Menghubungkan jaringan...");
  connectWiFi();
  connectMQTT();

  Serial.println("\n[SETUP] ✓ Plant 2 RTU siap");
  Serial.println("─────────────────────────────────────────────────────");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    Serial.println("[LOOP] MQTT terputus — reconnect...");
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastSampleMs < SAMPLING_MS) return;

  float dt_seconds = (float)(now - lastSampleMs) / 1000.0f;
  lastSampleMs = now;

  Serial.println("\n─── Sampling ───────────────────────────────────────");

  // 1. Baca flow
  float flowLPM = calculateFlowLPM(dt_seconds);
  lastFlowLPM   = flowLPM;
  String flowSt = flowStatus(flowLPM);

  // 2. Baca level
  float dist     = readDistanceCM();
  bool  lvFault  = (dist < 0);
  float volL     = lvFault ? -1.0f : distanceToVolume(dist);
  float pct      = lvFault ? -1.0f : volumeToPercent(volL);
  String lvSt    = levelStatus(pct);

  // 3. Update logika valve
  Serial.println("[LOGIC] Evaluasi kondisi valve...");
  updateValveLogic(pct, lvFault);

  // 4. Update flow alerts
  updateFlowAlerts(flowLPM);

  // 5. Serial monitor ringkas
  Serial.printf(
    "[STATUS] Level:%.3fL(%.0f%%) %-12s | Flow:%.2fL/min %-8s | Main:%s | FS:%s\n",
    volL, pct, ("("+lvSt+")").c_str(),
    flowLPM, ("("+flowSt+")").c_str(),
    valveMainState ? "BUKA" : "TUTUP",
    valveFsState   ? "BUKA" : "TUTUP"
  );
  Serial.println("─────────────────────────────────────────────────────");

  // 6. RBE publish
  bool heartbeat  = (now - lastSendMs)           >= RBE_HEARTBEAT_MS;
  bool lvChg      = fabs(volL    - lastLevel)    >= RBE_LEVEL_DEADBAND;
  bool flowChg    = fabs(flowLPM - lastFlowSent) >= RBE_FLOW_DEADBAND;

  if (heartbeat || lvChg || flowChg) {
    String reason = heartbeat ? "heartbeat" : (lvChg ? "level_change" : "flow_change");
    Serial.printf("[PUBLISH] Trigger: %s\n", reason.c_str());
    lastLevel    = volL;
    lastFlowSent = flowLPM;
    lastSendMs   = now;
    publishOrBuffer(TOPIC_LEVEL,      buildLevelJson(dist, volL, pct, lvSt));
    publishOrBuffer(TOPIC_FLOW,       buildFlowJson(flowLPM, flowSt));
    publishOrBuffer(TOPIC_VALVE_MAIN, buildValveJson("main_valve",     valveMainState));
    publishOrBuffer(TOPIC_VALVE_FS,   buildValveJson("failsafe_valve", valveFsState));
  }
}