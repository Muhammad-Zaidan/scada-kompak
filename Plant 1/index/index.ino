// ============================================================
// plant1.ino FINAL — WWTP Plant 1 RTU (Dumb Executor)
// Arsitektur: RTU hanya sensor + eksekusi perintah dari MTU
//             Tidak ada logika dosing/state machine lokal
// ============================================================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>       // v7
#include <math.h>
#include "config.h"

// ─────────────────────────────────────────────────────────────
// KONEKSI
// ─────────────────────────────────────────────────────────────
WiFiClientSecure wifiClient;
PubSubClient     mqtt(wifiClient);

// ─────────────────────────────────────────────────────────────
// STATE SENSOR
// ─────────────────────────────────────────────────────────────
float ema_pH   = 0.0f;
bool  ema_init = false;

// ─────────────────────────────────────────────────────────────
// STATE AKTUATOR
// ─────────────────────────────────────────────────────────────
bool valveState       = false;
bool dosingBaseActive = false;
bool dosingAcidActive = false;
bool stirrerActive    = false;

// ─────────────────────────────────────────────────────────────
// STATE LEVEL STABILITY
// ─────────────────────────────────────────────────────────────
float         volPrev      = -999.0f;
float         volStableRef = -999.0f;
unsigned long volStableMs  = 0;
bool          levelIsStable = false;

// ─────────────────────────────────────────────────────────────
// STATE PUBLISH
// ─────────────────────────────────────────────────────────────
float         lastPH     = -999.0f;
float         lastVolume = -999.0f;
unsigned long lastSendMs = 0;
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
void setValve(bool);
void setStirrer(bool);
void setPumpAcid(bool);
void setPumpBase(bool);

// =============================================================
// BAGIAN 1: SENSOR pH
// =============================================================
struct CalPoint { float volt; float pH; };
CalPoint calPoints[] = {
  {PH_CAL_V1, PH_CAL_PH1},
  {PH_CAL_V2, PH_CAL_PH2},
  {PH_CAL_V3, PH_CAL_PH3}
};
const int NUM_CAL = 3;

float piecewisePH(float volt) {
  if (volt >= calPoints[0].volt) {
    float dV  = calPoints[1].volt - calPoints[0].volt;
    float dPH = calPoints[1].pH   - calPoints[0].pH;
    return calPoints[0].pH + (volt - calPoints[0].volt) * (dPH / dV);
  }
  if (volt <= calPoints[NUM_CAL-1].volt) {
    int l = NUM_CAL - 1;
    float dV  = calPoints[l].volt - calPoints[l-1].volt;
    float dPH = calPoints[l].pH   - calPoints[l-1].pH;
    return calPoints[l-1].pH + (volt - calPoints[l-1].volt) * (dPH / dV);
  }
  for (int i = 0; i < NUM_CAL - 1; i++) {
    if (volt <= calPoints[i].volt && volt >= calPoints[i+1].volt) {
      float dV  = calPoints[i+1].volt - calPoints[i].volt;
      float dPH = calPoints[i+1].pH   - calPoints[i].pH;
      return calPoints[i].pH + (volt - calPoints[i].volt) * (dPH / dV);
    }
  }
  return -1.0f;
}

float readPH() {
  Serial.println("[pH] Membaca sensor pH...");
  int samples[PH_SAMPLES];
  for (int i = 0; i < PH_SAMPLES; i++) {
    samples[i] = analogRead(PH_PIN); delay(3);
  }
  // Bubble sort
  for (int i = 0; i < PH_SAMPLES-1; i++)
    for (int j = i+1; j < PH_SAMPLES; j++)
      if (samples[i] > samples[j]) { int t=samples[i]; samples[i]=samples[j]; samples[j]=t; }
  // Trimmed mean
  long sum = 0;
  for (int i = PH_TRIM; i < PH_SAMPLES-PH_TRIM; i++) sum += samples[i];
  float volt  = (float)sum/(PH_SAMPLES-2*PH_TRIM) * 3.3f/4095.0f;
  float rawPH = piecewisePH(volt);
  // EMA
  if (!ema_init) { ema_pH = rawPH; ema_init = true; }
  else ema_pH = EMA_ALPHA * rawPH + (1.0f-EMA_ALPHA) * ema_pH;
  Serial.printf("[pH] Volt=%.4fV Raw=%.3f EMA=%.3f\n", volt, rawPH, ema_pH);
  return ema_pH;
}

// =============================================================
// BAGIAN 2: SENSOR LEVEL — LOOKUP TABLE VOLUMETRIK
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
// BAGIAN 3: DETEKSI STABILITAS LEVEL
// Level dianggap stabil jika volume tidak berubah > 0.05L
// selama 1 menit penuh (LEVEL_STABLE_MS = 60000)
// =============================================================
void updateLevelStability(float volL) {
  if (volL < 0) {
    Serial.println("[STABILITY] Volume tidak valid — reset stability");
    levelIsStable = false; volStableRef = -999.0f; return;
  }
  bool rising = (volPrev >= 0 && (volL - volPrev) > LEVEL_STABLE_DELTA_L);
  if (rising) {
    levelIsStable = false;
    volStableRef  = volL;
    volStableMs   = millis();
    Serial.printf("[STABILITY] Level NAIK: %.3f → %.3f L — timer reset\n", volPrev, volL);
  } else {
    if (!levelIsStable) {
      if (volStableRef < 0) {
        volStableRef = volL;
        volStableMs  = millis();
        Serial.printf("[STABILITY] Mulai hitung stabilitas dari %.3f L\n", volL);
      } else if (fabs(volL - volStableRef) <= LEVEL_STABLE_DELTA_L) {
        unsigned long elapsed = millis() - volStableMs;
        if (elapsed >= LEVEL_STABLE_MS) {
          levelIsStable = true;
          Serial.printf("[STABILITY] ✓ STABIL di %.3f L (sudah %lums = 1 menit)\n",
                        volL, elapsed);
        } else {
          Serial.printf("[STABILITY] Menunggu stabil... %lums / %dms\n",
                        elapsed, LEVEL_STABLE_MS);
        }
      } else {
        volStableRef = volL;
        volStableMs  = millis();
        Serial.printf("[STABILITY] Bergerak — reset ke %.3f L\n", volL);
      }
    }
  }
  volPrev = volL;
}

// =============================================================
// BAGIAN 4: STATUS STRING
// =============================================================
String phStatus(float ph) {
  if (ph < 0)                                    return "Fault";
  if (ph < PH_FAULT_LOW || ph > PH_FAULT_HIGH)  return "Critical";
  return "Normal";
}

String levelStatus(float pct) {
  if (pct < 0)                return "Fault";
  if (pct <= LEVEL_CLOSE_PCT) return "Critical_Low";
  if (pct < 20.0f)            return "Warning_Low";
  if (pct > 99.0f)            return "Critical_High";
  if (pct > 90.0f)            return "Warning_High";
  return "Normal";
}

// =============================================================
// BAGIAN 5: AKTUATOR — HIGH-Z RELAY
// Metode: ON = OUTPUT + LOW, OFF = INPUT (High-Z / floating)
// Sama persis dengan kode kalibrasi yang sudah terbukti benar
// =============================================================
void relayOn(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  Serial.printf("[RELAY] Pin %d → ON (OUTPUT LOW)\n", pin);
}
void relayOff(int pin) {
  pinMode(pin, INPUT);        // High-Z — relay module pull-up → OFF
  Serial.printf("[RELAY] Pin %d → OFF (INPUT High-Z)\n", pin);
}

void setValve(bool open) {
  if (valveState == open) return;
  valveState = open;
  open ? relayOn(RELAY_VALVE_PIN) : relayOff(RELAY_VALVE_PIN);
  Serial.printf("[VALVE] ══► %s\n", open ? "BUKA ✓" : "TUTUP ✓");
  publishOrBuffer(TOPIC_VALVE, buildValveJson(valveState));
}

void setPumpAcid(bool on) {
  if (dosingAcidActive == on) return;
  dosingAcidActive = on;
  on ? relayOn(RELAY_PUMP_ACID_PIN) : relayOff(RELAY_PUMP_ACID_PIN);
  Serial.printf("[PUMP-ACID/HNO3] ══► %s\n", on ? "ON ✓" : "OFF ✓");
  publishOrBuffer(TOPIC_ACID_STATE, buildActuatorJson(dosingAcidActive));
}

void setPumpBase(bool on) {
  if (dosingBaseActive == on) return;
  dosingBaseActive = on;
  on ? relayOn(RELAY_PUMP_BASE_PIN) : relayOff(RELAY_PUMP_BASE_PIN);
  Serial.printf("[PUMP-BASE/KOH]  ══► %s\n", on ? "ON ✓" : "OFF ✓");
  publishOrBuffer(TOPIC_BASE_STATE, buildActuatorJson(dosingBaseActive));
}

void setStirrer(bool on) {
  if (stirrerActive == on) return;
  stirrerActive = on;
  if (on) {
    Serial.printf("[STIRRER] Soft start → target speed=%d...\n", STIRRER_SPEED);
    digitalWrite(STIRRER_EN_PIN, HIGH);
    for (int s = 0; s <= STIRRER_SPEED; s += STIRRER_RAMP_STEP) {
      analogWrite(STIRRER_PWM_PIN, s);
      delay(STIRRER_RAMP_DELAY_MS);
    }
    Serial.printf("[STIRRER] ══► ON ✓ (speed=%d/255)\n", STIRRER_SPEED);
  } else {
    analogWrite(STIRRER_PWM_PIN, 0);
    digitalWrite(STIRRER_EN_PIN, LOW);
    Serial.println("[STIRRER] ══► OFF ✓");
  }
  publishOrBuffer(TOPIC_MIXER_STATE, buildActuatorJson(stirrerActive));
}

void stopAllActuators() {
  Serial.println("[ACTUATOR] !! STOP ALL — mematikan semua aktuator");
  setPumpAcid(false);
  setPumpBase(false);
  setStirrer(false);
  setValve(false);
  Serial.println("[ACTUATOR] Semua aktuator OFF ✓");
}

// =============================================================
// BAGIAN 6: JSON BUILDERS (ArduinoJson v7)
// =============================================================
String buildPHJson(float ph, String status) {
  JsonDocument doc;
  doc["plant_id"]    = PLANT_ID;
  doc["sensor_type"] = "ph";
  doc["value"]       = serialized(String(ph, 2));
  doc["unit"]        = "pH";
  doc["status"]      = status;
  String out; serializeJson(doc, out); return out;
}

String buildLevelJson(float distCM, float volL, float pct, String status) {
  JsonDocument doc;
  doc["plant_id"]    = PLANT_ID;
  doc["sensor_type"] = "level";
  doc["value_cm"]    = serialized(String(distCM, 1));
  doc["value_liter"] = serialized(String(volL, 3));
  doc["value_pct"]   = serialized(String(pct, 1));
  doc["unit"]        = "liter";
  doc["status"]      = status;
  doc["is_stable"]   = levelIsStable;
  String out; serializeJson(doc, out); return out;
}

String buildValveJson(bool state) {
  JsonDocument doc;
  doc["plant_id"] = PLANT_ID;
  doc["device"]   = "outlet_valve";
  doc["state"]    = state ? "OPEN" : "CLOSED";
  String out; serializeJson(doc, out); return out;
}

String buildActuatorJson(bool state) {
  JsonDocument doc;
  doc["plant_id"] = PLANT_ID;
  doc["state"]    = state ? "ON" : "OFF";
  String out; serializeJson(doc, out); return out;
}

String buildAlertJson(String code, String message) {
  JsonDocument doc;
  doc["plant_id"] = PLANT_ID;
  doc["code"]     = code;
  doc["message"]  = message;
  doc["valve"]    = valveState ? "OPEN" : "CLOSED";
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

// =============================================================
// BAGIAN 8: MQTT CALLBACK — Perintah Aktuator dari MTU
// Arsitektur dumb executor: setiap aktuator punya topic sendiri
// Payload: {"cmd": "ON"} atau {"cmd": "OFF"}
//          {"state": "OPEN"/"CLOSED", "mode": "AUTO"}  (valve)
// =============================================================
void onMQTTMessage(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("\n[MQTT←] Topic: %s\n        Data : %s\n", topic, msg.c_str());

  JsonDocument doc;
  if (deserializeJson(doc, msg) != DeserializationError::Ok) {
    Serial.println("[MQTT←] ERROR: JSON parse gagal");
    return;
  }

  String t = String(topic);

  // ── Perintah pompa asam dari MTU ──────────────────────────
  if (t == TOPIC_ACID_CMD) {
    bool on = (doc["cmd"].as<String>() == "ON");
    Serial.printf("[CMD] Pompa Asam → %s\n", on ? "ON" : "OFF");
    setPumpAcid(on);
    return;
  }

  // ── Perintah pompa basa dari MTU ──────────────────────────
  if (t == TOPIC_BASE_CMD) {
    bool on = (doc["cmd"].as<String>() == "ON");
    Serial.printf("[CMD] Pompa Basa → %s\n", on ? "ON" : "OFF");
    setPumpBase(on);
    return;
  }

  // ── Perintah mixer/stirrer dari MTU ───────────────────────
  if (t == TOPIC_MIXER_CMD) {
    bool on = (doc["cmd"].as<String>() == "ON");
    Serial.printf("[CMD] Mixer → %s\n", on ? "ON" : "OFF");
    setStirrer(on);
    return;
  }

  // ── Perintah valve dari MTU ───────────────────────────────
  if (t == TOPIC_VALVE_CMD) {
    Serial.println("[CMD] Menerima perintah valve...");
    if (doc.containsKey("state")) {
      bool open = (doc["state"].as<String>() == "OPEN");
      setValve(open);
    }
    publishOrBuffer(TOPIC_VALVE, buildValveJson(valveState));
    return;
  }

  Serial.printf("[MQTT←] Topic tidak dikenal: %s\n", topic);
}

// =============================================================
// BAGIAN 9: KONEKSI
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
    Serial.printf("[MQTT] Menghubungkan ke broker (percobaan %d/5)...\n", tries+1);
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println("[MQTT] ✓ Terhubung ke broker");
      // Subscribe ke semua topic perintah aktuator
      mqtt.subscribe(TOPIC_VALVE_CMD);
      mqtt.subscribe(TOPIC_ACID_CMD);
      mqtt.subscribe(TOPIC_BASE_CMD);
      mqtt.subscribe(TOPIC_MIXER_CMD);
      Serial.println("[MQTT] Subscribe: valve/cmd, acid/cmd, base/cmd, mixer/cmd");
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
  Serial.println("║   Plant 1 RTU — WWTP SCADA      ║");
  Serial.println("║   Dumb Executor + Sensor         ║");
  Serial.println("╚══════════════════════════════════╝\n");

  Serial.println("[SETUP] Inisialisasi pin...");
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  pinMode(TRIG_PIN,        OUTPUT);
  pinMode(ECHO_PIN,        INPUT);
  pinMode(STIRRER_EN_PIN,  OUTPUT);
  pinMode(STIRRER_PWM_PIN, OUTPUT);
  digitalWrite(STIRRER_EN_PIN, LOW);
  analogWrite(STIRRER_PWM_PIN, 0);

  // Relay: Set INPUT (High-Z) — sama seperti kode kalibrasi
  // Relay module punya pull-up sendiri → relay OFF saat pin floating
  pinMode(RELAY_VALVE_PIN,     INPUT);
  pinMode(RELAY_PUMP_ACID_PIN, INPUT);
  pinMode(RELAY_PUMP_BASE_PIN, INPUT);
  valveState = dosingAcidActive = dosingBaseActive = false;
  Serial.println("[SETUP] Semua relay → OFF (INPUT High-Z) ✓");

  // MQTT
  wifiClient.setInsecure();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMQTTMessage);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);

  Serial.println("[SETUP] Menghubungkan jaringan...");
  connectWiFi();
  connectMQTT();

  Serial.println("\n[SETUP] ✓ Plant 1 RTU siap (dumb executor)");
  Serial.println("[SETUP] Mengirim data sensor, menunggu perintah dari MTU...\n");
  Serial.println("─────────────────────────────────────────────────────");
}

void loop() {
  // Jaga koneksi MQTT
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    Serial.println("[LOOP] MQTT terputus — reconnect...");
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastSampleMs < SAMPLING_MS) return;
  lastSampleMs = now;

  Serial.println("\n─── Sampling ───────────────────────────────────────");

  // 1. Baca sensor
  float ph    = readPH();
  float dist  = readDistanceCM();
  bool  phF   = (ph < 0);
  bool  lvF   = (dist < 0);
  float volL  = lvF ? -1.0f : distanceToVolume(dist);
  float pct   = lvF ? -1.0f : volumeToPercent(volL);
  String phSt = phStatus(ph);
  String lvSt = levelStatus(pct);

  // 2. Update stabilitas level
  updateLevelStability(volL);

  // 3. Failsafe lokal: tutup valve jika sensor fault
  if (phF || lvF) {
    if (valveState) {
      Serial.println("[FAILSAFE] Sensor fault — tutup valve");
      setValve(false);
      publishOrBuffer(TOPIC_ALERT, buildAlertJson(
        phF ? "PH_SENSOR_FAULT" : "LEVEL_SENSOR_FAULT",
        "Sensor error — valve ditutup (failsafe RTU)"
      ));
    }
  }

  // 4. Failsafe lokal: tutup valve jika level kritis
  if (!lvF && valveState && pct >= 0 && pct <= LEVEL_CLOSE_PCT) {
    Serial.printf("[FAILSAFE] Level kritis (%.1f%% ≤ %.0f%%) — tutup valve\n",
                  pct, LEVEL_CLOSE_PCT);
    setValve(false);
    publishOrBuffer(TOPIC_ALERT, buildAlertJson(
      "LEVEL_CRITICAL_LOW",
      String("Level=") + String(pct,1) + "% ≤ 10% — valve ditutup RTU"
    ));
  }

  // 5. Serial monitor ringkas
  Serial.printf("[STATUS] pH:%.2f %-8s | Vol:%.3fL(%.0f%%) | Stable:%s | Valve:%s | Acid:%s | Base:%s | Mixer:%s\n",
    ph, ("("+phSt+")").c_str(),
    volL, pct,
    levelIsStable ? "✓" : "✗",
    valveState ? "BUKA" : "TUTUP",
    dosingAcidActive ? "ON" : "OFF",
    dosingBaseActive ? "ON" : "OFF",
    stirrerActive    ? "ON" : "OFF"
  );
  Serial.println("─────────────────────────────────────────────────────");

  // 6. RBE publish
  bool heartbeat = (now - lastSendMs)        >= RBE_HEARTBEAT_MS;
  bool phChg     = fabs(ph   - lastPH)       >= RBE_PH_DEADBAND;
  bool lvChg     = fabs(volL - lastVolume)    >= RBE_LEVEL_DEADBAND;

  if (heartbeat || phChg || lvChg) {
    String reason = heartbeat ? "heartbeat" : (phChg ? "pH_change" : "level_change");
    Serial.printf("[PUBLISH] Trigger: %s\n", reason.c_str());
    lastPH     = ph;
    lastVolume = volL;
    lastSendMs = now;
    publishOrBuffer(TOPIC_PH,    buildPHJson(ph, phSt));
    publishOrBuffer(TOPIC_LEVEL, buildLevelJson(dist, volL, pct, lvSt));
    // Publish semua status aktuator saat periodic publish
    publishOrBuffer(TOPIC_VALVE, buildValveJson(valveState));
    publishOrBuffer(TOPIC_ACID_STATE,  buildActuatorJson(dosingAcidActive));
    publishOrBuffer(TOPIC_BASE_STATE,  buildActuatorJson(dosingBaseActive));
    publishOrBuffer(TOPIC_MIXER_STATE, buildActuatorJson(stirrerActive));
  }
}