// ============================================================
// mtu_main.ino — WWTP MTU (Master Terminal Unit)
// Platform   : ESP32 WEMOS D1 R32
// Role       : Otak sistem WWTP
//              - Subscribe data sensor Plant 1 & Plant 2
//              - Jalankan logika kontrol terpusat (pH SM + valve)
//              - Publish perintah aktuator ke kedua RTU
//              - Modbus TCP Server untuk HMI AVEVA InTouch
//              - Offline ring buffer → flush ke Supabase via MQTT
// ============================================================
// DEPENDENSI (Arduino Library Manager):
//   - PubSubClient     by Nick O'Leary     v2.8
//   - ArduinoJson      by Benoit Blanchon  v7.x
//   - ModbusIP_ESP8266 by Andre Sarmento   (kompatibel ESP32)
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>       // v7 — pakai JsonDocument tanpa parameter
#include <ModbusIP_ESP8266.h>  // Modbus TCP Slave
#include "config.h"

// ─────────────────────────────────────────────────────────────
// OBJEK KONEKSI
// ─────────────────────────────────────────────────────────────
WiFiClientSecure wifiClient;
PubSubClient     mqtt(wifiClient);
ModbusIP         modbus;

// ─────────────────────────────────────────────────────────────
// HELPER: nama state pH untuk Serial monitor
// ─────────────────────────────────────────────────────────────
const char* pHStateStr(pHState s) {
  switch (s) {
    case pHState::INITIAL_WAIT: return "INITIAL_WAIT";
    case pHState::MONITORING: return "MONITORING";
    case pHState::DOSE_PULSE: return "DOSE_PULSE";
    case pHState::DOSE_DELAY: return "DOSE_DELAY";
    case pHState::MIXING:     return "MIXING";
    case pHState::SETTLING:   return "SETTLING";
    case pHState::PH_OK:      return "PH_OK";
    case pHState::PH_FAULT:   return "PH_FAULT";
    default:                  return "UNKNOWN";
  }
}

// ─────────────────────────────────────────────────────────────
// STRUKTUR DATA SENSOR TERKINI
// ─────────────────────────────────────────────────────────────
struct Plant1Data {
  float ph          = 7.0f;
  float levelLiter  = 0.0f;
  float levelPct    = 0.0f;
  float levelLogPct = 0.0f;
  bool  valveOpen   = false;
  bool  isStable    = false;   // Level stabil (1 menit tidak berubah)
  bool  phValid     = false;
  bool  levelValid  = false;
  unsigned long lastPhMs    = 0;
  unsigned long lastLevelMs = 0;
} p1;

struct Plant2Data {
  float levelLiter    = 0.0f;
  float levelPct      = 0.0f;
  float levelLogPct   = 0.0f;
  float flowLPM       = 0.0f;
  bool  valveMainOpen = false;
  bool  valveFsOpen   = false;
  bool  levelValid    = false;
  bool  flowValid     = false;
  unsigned long lastLevelMs = 0;
  unsigned long lastFlowMs  = 0;
} p2;

// ─────────────────────────────────────────────────────────────
// STATE AKTUATOR — tracking perintah terakhir ke RTU
// ─────────────────────────────────────────────────────────────
bool cmdP1ValveOpen      = false;
bool cmdP1AcidPumpOn     = false;
bool cmdP1BasePumpOn     = false;
bool cmdP1MixerOn        = false;
bool cmdP2ValveMainOpen = false;
bool cmdP2ValveFsOpen   = false;

// ─────────────────────────────────────────────────────────────
// STATE MACHINE & ALARM VARIABLES
// ─────────────────────────────────────────────────────────────
pHState        phCtrlState  = pHState::INITIAL_WAIT;
bool           dosingBase   = false;
int            doseCycles   = 0;
unsigned long stateStartMs = 0;

Alarm alarmPH;
Alarm alarmLevel;
Alarm alarmFlow;

bool maintenanceOn = false;
bool prevResetCoil = false;

// [FIX 2] systemReady — flag agar alarm tidak menyala saat boot
// sebelum data pertama dari kedua plant tiba.
// Disetel true hanya setelah P1 (ph+level) dan P2 (level) valid.
bool systemReady = false;

// ─────────────────────────────────────────────────────────────
// OFFLINE RING BUFFER
//
// Menyimpan snapshot sensor saat WiFi/MQTT terputus.
// Struktur ring buffer: kepala (head) selalu menunjuk slot
// berikutnya yang akan diisi. Jika penuh, data terlama
// ditimpa (oldest-first overwrite).
//
// Saat WiFi reconnect → MQTT reconnect → flushOfflineBuffer()
// dipanggil, mengirim semua entri valid ke topic T_MTU_HISTORY.
// Node-RED subscribe topic ini dan menulis ke Supabase.
// ─────────────────────────────────────────────────────────────
SensorSnapshot offlineBuf[OFFLINE_BUF_SIZE];
uint8_t        bufHead   = 0;    // slot berikutnya yang akan ditulis
uint8_t        bufCount  = 0;    // jumlah entri yang belum ter-flush
bool           needFlush = false; // disetel true saat WiFi baru reconnect

unsigned long lastSnapshotMs = 0; // timestamp snapshot terakhir

void buzzer() {
  static bool buzzerInitialized = false;
  if (!buzzerInitialized) {
    pinMode(25, OUTPUT);    
    digitalWrite(25, LOW);
    buzzerInitialized = true;
  }

  // Indikator apakah data dari RTU benar-benar hilang/kabel dicabut
  bool kabelPutusP1 = false;
  bool kabelPutusP2 = false;

  // 1. Validasi Plant 1 (Hanya cek jika systemReady sudah aktif setelah koneksi awal sukses)
  if (systemReady) {
    // Jika data tidak fresh (stale) ATAU nilainya anjlok di bawah batas operasional (di bawah 10%, mendekati 0)
    if (!isP1DataFresh() || p1.levelLogPct < 0.5f) {
      kabelPutusP1 = true;
    }
  }

  // 2. Validasi Plant 2
  if (systemReady) {
    // Jika data tidak fresh (stale) ATAU nilainya anjlok di bawah batas operasional (di bawah 10%, mendekati 0)
    if (!isP2DataFresh() || p2.levelLogPct < 0.5f) {
      kabelPutusP2 = true;
    }
  }

  // 3. Eksekusi Buzzer GPIO 25
  if (systemReady && (kabelPutusP1 || kabelPutusP2)) {
    digitalWrite(25, HIGH); // BUNYI (Murni karena hardware fault / kabel dicabut)
  } else {
    digitalWrite(25, LOW);  // MATI (Normal operasional, karena level selalu >= 10%)
  }
}
void failsafePompa() {
  if (phCtrlState == pHState::DOSE_DELAY || 
      phCtrlState == pHState::MIXING     || 
      phCtrlState == pHState::SETTLING) {
    if (cmdP1AcidPumpOn) cmdP1AcidPumpOn = false;
    if (cmdP1BasePumpOn) cmdP1BasePumpOn = false;
  }
}

// Simpan satu snapshot ke ring buffer
void pushSnapshot() {
  buzzer();
  failsafePompa();

  SensorSnapshot s;
  s.capturedMs  = millis();
  s.ph          = p1.ph;
  s.levelP1Pct  = p1.levelLogPct;
  s.levelP2Pct  = p2.levelLogPct;
  s.flowP2      = p2.flowLPM;
  s.valveP1     = cmdP1ValveOpen;
  s.valveP2Main = cmdP2ValveMainOpen;
  s.valveP2Fs   = cmdP2ValveFsOpen;
  s.phStateIdx  = (uint8_t)phCtrlState;
  s.valid       = true;

  offlineBuf[bufHead] = s;
  bufHead = (bufHead + 1) % OFFLINE_BUF_SIZE;
  if (bufCount < OFFLINE_BUF_SIZE) bufCount++;
}

// Kirim semua entri buffer ke Node-RED via MQTT topic T_MTU_HISTORY
// Node-RED subscribe topic ini dan tulis ke Supabase per entri.
// Dipanggil setelah MQTT reconnect berhasil.
void flushOfflineBuffer() {
  if (bufCount == 0) return;

  // Hitung indeks mulai (entri terlama)
  uint8_t startIdx = (bufHead - bufCount + OFFLINE_BUF_SIZE) % OFFLINE_BUF_SIZE;
  uint8_t flushed  = 0;

  Serial.printf("[BUF] Flushing %d entri offline ke Supabase via MQTT...\n",
                bufCount);

  for (uint8_t i = 0; i < bufCount; i++) {
    uint8_t idx = (startIdx + i) % OFFLINE_BUF_SIZE;
    SensorSnapshot& s = offlineBuf[idx];
    if (!s.valid) continue;

    // Bangun payload JSON per entri
    JsonDocument doc;
    doc["source"]     = MTU_ID;
    doc["offset_ms"]  = s.capturedMs;     // offset millis dari boot
    doc["ph"]         = serialized(String(s.ph, 2));
    doc["level_p1"]   = serialized(String(s.levelP1Pct, 1));
    doc["level_p2"]   = serialized(String(s.levelP2Pct, 1));
    doc["flow_p2"]    = serialized(String(s.flowP2, 2));
    doc["valve_p1"]   = s.valveP1     ? "OPEN" : "CLOSED";
    doc["valve_main"] = s.valveP2Main ? "OPEN" : "CLOSED";
    doc["valve_fs"]   = s.valveP2Fs   ? "OPEN" : "CLOSED";
    doc["ph_state"]   = pHStateStr((pHState)s.phStateIdx);
    doc["buffered"]   = true;   // tandai sebagai data offline (untuk Node-RED)

    char buf[256];
    serializeJson(doc, buf);

    if (mqtt.publish(T_MTU_HISTORY, buf)) {
      s.valid = false;
      flushed++;
    }
    // Beri jeda antar publish agar broker tidak overwhelmed
    delay(50);
  }

  uint8_t totalToFlush = bufCount;  // simpan sebelum direset
  bufCount = 0;
  bufHead  = 0;
  needFlush = false;
  Serial.printf("[BUF] Flush selesai: %d/%d entri terkirim\n",
                flushed, totalToFlush);
}

// Snapshot periodik — dipanggil dari loop() setiap SNAPSHOT_INTERVAL_MS
// Berjalan terus meski WiFi mati; data tersimpan di RAM hingga reconnect.
void runSnapshotTask() {
  if (!systemReady) return;
  unsigned long now = millis();
  if (now - lastSnapshotMs < SNAPSHOT_INTERVAL_MS) return;
  lastSnapshotMs = now;
  pushSnapshot();
  Serial.printf("[BUF] Snapshot disimpan. Buffer: %d/%d entri\n",
                bufCount, OFFLINE_BUF_SIZE);
}

// ─────────────────────────────────────────────────────────────
// HELPER: Konversi liter fisik → persen logical
// ─────────────────────────────────────────────────────────────
float toLogicalPct(float liter, float maxOpL) {
  float pct = (liter / maxOpL) * 100.0f;
  if (pct < 0.0f)   pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

// ─────────────────────────────────────────────────────────────
// HELPER: Cek kesegaran data sensor
// ─────────────────────────────────────────────────────────────
bool isP1DataFresh() {
  unsigned long now = millis();
  bool actuatorOn = (cmdP1AcidPumpOn || cmdP1BasePumpOn || cmdP1MixerOn || cmdP1ValveOpen);
  unsigned long timeout = actuatorOn ? 60000UL : DATA_TIMEOUT_MS;
  
  if (cmdP1ValveOpen) {
    // Saat kuras, sensor pH menggantung di udara. Hiraukan validitas & timeout pH!
    return p1.levelValid && (now - p1.lastLevelMs < timeout);
  }

  return p1.phValid && p1.levelValid &&
         (now - p1.lastPhMs    < timeout) &&
         (now - p1.lastLevelMs < timeout);
}

bool isP2DataFresh() {
  return p2.levelValid &&
         (millis() - p2.lastLevelMs < DATA_TIMEOUT_MS);
}

// ─────────────────────────────────────────────────────────────
// HELPER: Tulis float IEEE754 ke 2 Holding Register Big-Endian
// ─────────────────────────────────────────────────────────────
void setFloatReg(uint16_t startAddr, float value) {
  uint32_t raw;
  memcpy(&raw, &value, sizeof(raw));
  modbus.Hreg(startAddr,     (uint16_t)(raw >> 16));
  modbus.Hreg(startAddr + 1, (uint16_t)(raw & 0xFFFF));
}

// ─────────────────────────────────────────────────────────────
// PUBLISH HELPER — state tracking + retain flag
//
// retain=true: broker menyimpan perintah terakhir per topic.
// RTU yang reconnect langsung dapat perintah terakhir tanpa
// menunggu perubahan state baru dari MTU.
// ─────────────────────────────────────────────────────────────
void sendActuatorCmd(const char* topic, bool on, bool& stateTracker, unsigned long target_pulse = 0) {
  if (stateTracker == on && target_pulse == 0) return; // Mengizinkan pengiriman ulang jika ada pulsa baru
  stateTracker = on;
  JsonDocument doc;
  doc["cmd"] = on ? "ON" : "OFF";
  if (on && target_pulse > 0) {
    doc["target_pulse"] = target_pulse;
  }
  char buf[96];
  serializeJson(doc, buf);
  mqtt.publish(topic, buf, false);   // retain=false (hindari command hantu)
  Serial.printf("[CMD→] %s : %s\n", topic, buf);
}

void sendValveCmd(const char* topic, bool open, bool& stateTracker) {
  if (stateTracker == open) return;
  stateTracker = open;
  JsonDocument doc;
  doc["state"] = open ? "OPEN" : "CLOSED";
  doc["mode"]  = "AUTO";
  char buf[96];
  serializeJson(doc, buf);
  mqtt.publish(topic, buf, false);   // retain=false
  Serial.printf("[CMD→] %s : %s\n", topic, buf);
}

// ─────────────────────────────────────────────────────────────
// WRAPPER KONTROL AKTUATOR
// ─────────────────────────────────────────────────────────────
void setP1Valve(bool open)     { sendValveCmd(T_P1_VALVE_CMD,          open, cmdP1ValveOpen);     }
void setP1AcidPump(bool on, unsigned long pulse=0) { sendActuatorCmd(T_P1_DOSING_ACID_CMD, on, cmdP1AcidPumpOn, pulse); }
void setP1BasePump(bool on, unsigned long pulse=0) { sendActuatorCmd(T_P1_DOSING_BASE_CMD, on, cmdP1BasePumpOn, pulse); }
void setP1Mixer(bool on)       { sendActuatorCmd(T_P1_MIXER_CMD,       on,   cmdP1MixerOn);       }
void setP2ValveMain(bool open) { sendValveCmd(T_P2_MAIN_CMD,           open, cmdP2ValveMainOpen); }
void setP2ValveFs(bool open)   { sendValveCmd(T_P2_FS_CMD,             open, cmdP2ValveFsOpen);   }

void stopAllDosing() {
  setP1AcidPump(false);
  setP1BasePump(false);
  setP1Mixer(false);
}

// [FIX 6] Reset semua state tracker aktuator
// Dipanggil saat MQTT reconnect agar MTU republish perintah
// terakhir ke broker (broker mungkin restart & retained hilang).
void resetActuatorTrackers() {
  cmdP1ValveOpen     = !cmdP1ValveOpen;   // paksa beda agar trigger kirim ulang
  cmdP1AcidPumpOn    = !cmdP1AcidPumpOn;
  cmdP1BasePumpOn    = !cmdP1BasePumpOn;
  cmdP1MixerOn       = !cmdP1MixerOn;
  cmdP2ValveMainOpen = !cmdP2ValveMainOpen;
  cmdP2ValveFsOpen   = !cmdP2ValveFsOpen;
}

// ─────────────────────────────────────────────────────────────
// PUBLISH DOSING LOG
// ─────────────────────────────────────────────────────────────
void publishDosingState(const char* statusStr) {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["plant_id"]     = "mtu";
  doc["status"]       = statusStr;
  doc["cycle"]        = doseCycles;
  doc["target_ml"]    = 0;
  doc["delivered_ml"] = 0;
  doc["pump_base"]    = cmdP1BasePumpOn;
  doc["pump_acid"]    = cmdP1AcidPumpOn;
  doc["stirrer"]      = cmdP1MixerOn;

  char buf[256];
  serializeJson(doc, buf);
  mqtt.publish(T_MTU_DOSING_STATE, buf, false);
  Serial.printf("[LOG→] %s : %s\n", T_MTU_DOSING_STATE, buf);
}

// ─────────────────────────────────────────────────────────────
// KALKULASI DOSIS PROPORSIONAL PULSA
// ─────────────────────────────────────────────────────────────
unsigned long currentTargetPulse = 0; 

unsigned long calcDosePulse(float currentPh) {
  float error = fabs(currentPh - 7.0f); 
  float doseMl = error * KP_DOSING_ML_PER_PH;
  unsigned long pulses = (unsigned long)(doseMl * PULSE_PER_ML);
  if (pulses < 1) pulses = 1; 
  return pulses;
}

// ─────────────────────────────────────────────────────────────
// STATE MACHINE pH Plant 1 (non-blocking, millis-based)
// ─────────────────────────────────────────────────────────────
void runPhStateMachine() {
  unsigned long now = millis();
  float ph = p1.ph;

  switch (phCtrlState) {

    case pHState::INITIAL_WAIT: {
      if (now - stateStartMs >= INITIAL_WAIT_MS) {
        phCtrlState = pHState::MONITORING;
        Serial.println("[pH-SM] Waktu tunggu awal (3 menit) selesai → MONITORING");
      }
      break;
    }

    case pHState::MONITORING: {
      static unsigned long phFaultStartMs = 0;
      // Hiraukan ekstrem pH jika sedang proses kuras (sensor menggantung)
      if (!cmdP1ValveOpen && (ph < PH_FAULT_LOW || ph > PH_FAULT_HIGH)) {
        if (phFaultStartMs == 0) phFaultStartMs = millis();
        if (millis() - phFaultStartMs > 5000) { // Debounce 5 detik menahan lonjakan sensor pH
          stopAllDosing();
          setP1Valve(false);
          phCtrlState = pHState::PH_FAULT;
          publishDosingState("PH_FAULT");
          Serial.printf("[pH-SM] ⚠ FAULT ekstrem — pH=%.2f\n", ph);
          phFaultStartMs = 0;
          break;
        }
      } else {
        phFaultStartMs = 0;
      }

      if (!p1.isStable) {
        static unsigned long lastUnstableWarn = 0;
        if (millis() - lastUnstableWarn > 5000) {
          Serial.println("[pH-SM] Menunggu level stabil (1 menit)...");
          lastUnstableWarn = millis();
        }
        break;  
      }
      
      if (p1.levelLogPct < P1_MIN_DOSING_PCT) {
        static unsigned long lastLowWarn = 0;
        if (millis() - lastLowWarn > 5000) {
          Serial.printf("[pH-SM] Level terlalu rendah (%.1f%% < %.0f%%) — Dosing ditunda\n", 
                        p1.levelLogPct, P1_MIN_DOSING_PCT);
          lastLowWarn = millis();
        }
        break;
      }
      if (ph < PH_SAFE_MIN) {
        dosingBase   = true;
        doseCycles   = 1;
        setP1Valve(false);
        currentTargetPulse = calcDosePulse(ph);
        setP1BasePump(true, currentTargetPulse);
        stateStartMs = now;
        phCtrlState  = pHState::DOSE_PULSE;
        publishDosingState("DOSING_BASE");
        Serial.printf("[pH-SM] pH=%.2f < %.1f → DOSE_PULSE [BASE] #1 (%lu pulsa)\n",
                      ph, PH_SAFE_MIN, currentTargetPulse);
        break;
      }
      if (ph > PH_SAFE_MAX) {
        dosingBase   = false;
        doseCycles   = 1;
        setP1Valve(false);
        currentTargetPulse = calcDosePulse(ph);
        setP1AcidPump(true, currentTargetPulse);
        stateStartMs = now;
        phCtrlState  = pHState::DOSE_PULSE;
        publishDosingState("DOSING_ACID");
        Serial.printf("[pH-SM] pH=%.2f > %.1f → DOSE_PULSE [ACID] #1 (%lu pulsa)\n",
                      ph, PH_SAFE_MAX, currentTargetPulse);
        break;
      } else {
        phCtrlState = pHState::PH_OK;
        doseCycles  = 0;
        Serial.println("[pH-SM] pH aman → Standby di PH_OK");
      }
      break;
    }

    case pHState::DOSE_PULSE: {
      if (now - stateStartMs >= DOSE_PULSE_MAX_MS) {
        if (dosingBase) setP1BasePump(false);
        else            setP1AcidPump(false);
        doseCycles++;
        stateStartMs = now;
        phCtrlState  = pHState::DOSE_DELAY;
        Serial.printf("[pH-SM] Timeout pulsa %lu ms tercapai → DOSE_DELAY\n", DOSE_PULSE_MAX_MS);
      }
      break;
    }

    case pHState::DOSE_DELAY: {
      if (now - stateStartMs >= DOSE_TO_MIXER_MS) {
        setP1Mixer(true);
        stateStartMs = now;
        phCtrlState  = pHState::MIXING;
        publishDosingState("MIXING");
        Serial.println("[pH-SM] Delay selesai → MIXING ON");
      }
      break;
    }

    case pHState::MIXING: {
      if (now - stateStartMs >= MIXER_RUN_MS) {
        setP1Mixer(false);
        stateStartMs = now;
        phCtrlState  = pHState::SETTLING;
        publishDosingState("SETTLING");
        Serial.println("[pH-SM] Mixer OFF → SETTLING");
      }
      break;
    }

    case pHState::SETTLING: {
      if (now - stateStartMs >= SETTLE_MS) {
        Serial.printf("[pH-SM] Settling selesai. pH=%.2f siklus %d/%d\n",
                      ph, doseCycles, MAX_DOSE_CYCLES);
        // Jika pH sudah di dalam zona aman (6.8–7.2), langsung selesai
        if (ph >= PH_SAFE_MIN && ph <= PH_SAFE_MAX) {
          phCtrlState = pHState::PH_OK;
          doseCycles  = 0;
          Serial.printf("[pH-SM] ✓ pH=%.2f dalam zona aman (%.1f–%.1f) → PH_OK\n",
                        ph, PH_SAFE_MIN, PH_SAFE_MAX);
          break;
        }
        if (doseCycles >= MAX_DOSE_CYCLES) {
          stopAllDosing();
          setP1Valve(false);
          phCtrlState = pHState::PH_FAULT;
          Serial.println("[pH-SM] ✗ Max siklus dosing → PH_FAULT");
          break;
        }
        
        currentTargetPulse = calcDosePulse(ph);
        if (dosingBase) setP1BasePump(true, currentTargetPulse);
        else            setP1AcidPump(true, currentTargetPulse);
        
        stateStartMs = now;
        phCtrlState  = pHState::DOSE_PULSE;
        Serial.printf("[pH-SM] Belum tercapai → DOSE_PULSE #%d (%lu pulsa)\n",
                      doseCycles + 1, currentTargetPulse);
      }
      break;
    }

    case pHState::PH_OK: {
      if (!cmdP1ValveOpen && (ph < PH_SAFE_MIN || ph > PH_SAFE_MAX)) {
        phCtrlState = pHState::MONITORING;
        Serial.printf("[pH-SM] pH drift (%.2f) → MONITORING\n", ph);
      }
      break;
    }

    case pHState::PH_FAULT: {
      stopAllDosing();
      setP1Valve(false);
      if (ph >= PH_SAFE_MIN && ph <= PH_SAFE_MAX) {
        doseCycles  = 0;
        phCtrlState = pHState::MONITORING;
        Serial.println("[pH-SM] pH kembali normal → MONITORING");
      } else if (ph > PH_FAULT_LOW && ph < PH_FAULT_HIGH && doseCycles < MAX_DOSE_CYCLES) {
        phCtrlState = pHState::MONITORING;
        Serial.println("[pH-SM] Data sensor pulih → lanjut MONITORING");
      }
      break;
    }
  }
}

void runValveControlP1() {
  // ====================================================================
  // KONDISI A: JIKA POMPA/VALVE SEDANG MENYALA (PROSES DISTRIBUSI AKTIF)
  // ====================================================================
  if (cmdP1ValveOpen) {
    // REVISI: Pompa hanya BOLEH MATI jika level air menyentuh batas bawah 10%
    static unsigned long lowLevelStart = 0;
    if (p1.levelLogPct <= 10.0f) { 
      if (lowLevelStart == 0) lowLevelStart = millis();
      if (millis() - lowLevelStart > 5000) { // Debounce 5 detik menahan lonjakan sensor
        setP1Valve(false);
        Serial.printf("[VALVE-P1] Batas Kuras Tercapai (10%%). TUTUP Aman — P1=%.1f%%\n", p1.levelLogPct);
        lowLevelStart = 0;
        return;
      }
    } else {
      lowLevelStart = 0;
    }

    // INTERLOCK KHUSUS PLANT 2: Tetap matikan jika Plant 2 penuh (Sesuai tujuan Anda)
    if (isP2DataFresh()) {
      if (p2.levelLogPct >= P2_INTERLOCK_PCT) {
        setP1Valve(false);
        Serial.printf("[INTERLOCK] P2 Penuh (%.1f%%) → Pompa P1 dipaksa TUTUP\n", p2.levelLogPct);
        return;
      }
    } else {
      setP1Valve(false);
      Serial.println("[INTERLOCK] Data P2 Hilang saat distribusi → Pompa P1 TUTUP (failsafe)");
      return;
    }

    // BYPASS TOTAL: Selama air masih di atas 10% dan Plant 2 belum penuh, keluar.
    // Fluktuasi sensor naik-turun (seperti lompatan ke 87.8% tadi) DIABAIKAN TOTAL!
    return; 
  }

  // ====================================================================
  // KONDISI B: JIKA POMPA SEDANG MATI MENURUT MTU (MENUNGGU SYARAT START)
  // ====================================================================
  
  // Tentukan apakah SEMUA syarat ideal untuk memulai kuras terpenuhi
  bool shouldBeOpen = (phCtrlState == pHState::PH_OK) && 
                      isP1DataFresh() &&
                      (!isP2DataFresh() || p2.levelLogPct < P2_INTERLOCK_PCT) &&
                      (p1.levelLogPct >= P1_LEVEL_OPEN_PCT);

  if (!shouldBeOpen) {
    // Jika syarat TIDAK terpenuhi, cek apakah pompa fisik ternyata malah nyala
    if (p1.valveOpen) {
      // Pompa fisik menyala (karena manual dari web atau hantu pesan retain MQTT),
      // sedangkan aturan MTU melarangnya. Paksa TUTUP sekarang juga!
      cmdP1ValveOpen = true; // Tipu stateTracker sesaat agar pengiriman perintah tidak dicegah
      setP1Valve(false);
      Serial.println("[VALVE-P1] ⚠ Pompa menyala di luar aturan MTU. Memaksa TUTUP!");
    }
    return;
  }

  // JIKA SEMUA SYARAT AMAN DAN LEVEL BENAR-BENAR PENUH (>80%), BARULAH START
  setP1Valve(true);
  Serial.printf("[VALVE-P1] START DISTRIBUSI — pH=%.2f P1=%.1f%% P2=%.1f%%\n", 
                p1.ph, p1.levelLogPct, p2.levelLogPct);
}
//─────────────────────────────────────────────────────────
// LOGIKA VALVE Plant 2
// ─────────────────────────────────────────────────────────────
void runValveControlP2() {
  static bool p2StaleWarned = false;
  if (!isP2DataFresh()) {
    setP2ValveMain(false);
    setP2ValveFs(false);
    if (!p2StaleWarned) {
      Serial.println("[VALVE-P2] P2 stale → TUTUP semua");
      p2StaleWarned = true;
    }
    return;
  }
  p2StaleWarned = false;

  float p2Log = p2.levelLogPct;
  if (!cmdP2ValveMainOpen && p2Log >= P2_MAIN_OPEN_PCT) {
    setP2ValveMain(true);
    Serial.printf("[VALVE-P2-MAIN] BUKA — %.1f%%\n", p2Log);
  } else if (cmdP2ValveMainOpen && p2Log <= P2_MAIN_CLOSE_PCT) {
    setP2ValveMain(false);
    Serial.printf("[VALVE-P2-MAIN] TUTUP — %.1f%%\n", p2Log);
  }
  if (!cmdP2ValveFsOpen && p2Log >= P2_FS_OPEN_PCT) {
    setP2ValveFs(true);
    Serial.printf("[VALVE-P2-FS] ⚠ BUKA — %.1f%%\n", p2Log);
  } else if (cmdP2ValveFsOpen && p2Log <= P2_FS_CLOSE_PCT) {
    setP2ValveFs(false);
    Serial.printf("[VALVE-P2-FS] TUTUP — %.1f%%\n", p2Log);
  }
}

// ─────────────────────────────────────────────────────────────
// KONDISI FAULT per kategori alarm
// ─────────────────────────────────────────────────────────────
bool isFaultPH() {
  if (!p1.phValid) return true;
  return (p1.ph < PH_FAULT_LOW || p1.ph > PH_FAULT_HIGH ||
          phCtrlState == pHState::PH_FAULT);
}

bool isFaultLevel() {
  if (!systemReady) return false;
  if (!p1.levelValid || !p2.levelValid) return true;
  if (cmdP1ValveOpen && p1.levelLogPct <= P1_LEVEL_CLOSE_PCT) return true;
  if (p2.levelLogPct >= P2_INTERLOCK_PCT) return true;
  return false;
}

bool isFaultFlow() {
  if (!systemReady) return false;
  if (!p2.flowValid) return false;
  if (cmdP2ValveMainOpen && p2.flowLPM < 0.1f) return true;  
  if (p2.flowLPM > 10.0f) return true;                       
  return false;
}

// ─────────────────────────────────────────────────────────────
// ALARM ENGINE 
// ─────────────────────────────────────────────────────────────
void updateOneAlarm(Alarm& alarm, uint16_t coilAddr,
                    uint16_t ackCoilAddr, bool faultActive) {
  unsigned long now = millis();

  switch (alarm.state) {

    case AlarmState::NORMAL: {
      if (faultActive) {
        alarm.state      = AlarmState::FAULT;
        alarm.flipFlop   = true;
        alarm.lastFlipMs = now;
        modbus.Coil(coilAddr, true);
        Serial.printf("[ALARM] Coil %d FAULT\n", coilAddr);
      }
      break;
    }

    case AlarmState::FAULT: {
      if (now - alarm.lastFlipMs >= ALARM_FLIP_MS) {
        alarm.lastFlipMs = now;
        alarm.flipFlop   = !alarm.flipFlop;
        modbus.Coil(coilAddr, alarm.flipFlop);
      }
      bool curAck = modbus.Coil(ackCoilAddr);
      if (curAck && !alarm.prevAckCoil) {
        alarm.state = AlarmState::ACKED;
        modbus.Coil(coilAddr,    true);   
        modbus.Coil(ackCoilAddr, false);  
        Serial.printf("[ALARM] Coil %d ACKED\n", coilAddr);
      }
      alarm.prevAckCoil = curAck;
      if (!faultActive) {
        alarm.state = AlarmState::NORMAL;
        modbus.Coil(coilAddr, false);
      }
      break;
    }

    case AlarmState::ACKED: {
      if (faultActive) {
        alarm.state      = AlarmState::FAULT;
        alarm.flipFlop   = true;
        alarm.lastFlipMs = now;
        modbus.Coil(coilAddr, true);
        Serial.printf("[ALARM] Coil %d rekuren → FAULT\n", coilAddr);
      }
      break;
    }

    case AlarmState::CLEARED: {
      break;
    }
  }
}

void updateAlarms() {
  if (!systemReady) return;
  updateOneAlarm(alarmPH,    COIL_ALARM_PH,    COIL_ACK_PH,    isFaultPH());
  updateOneAlarm(alarmLevel, COIL_ALARM_LEVEL, COIL_ACK_LEVEL, isFaultLevel());
  updateOneAlarm(alarmFlow,  COIL_ALARM_FLOW,  COIL_ACK_FLOW,  isFaultFlow());
}

// ─────────────────────────────────────────────────────────────
// HANDLER RESET SWITCH
// ─────────────────────────────────────────────────────────────
void handleResetSwitch() {
  bool curReset = modbus.Coil(COIL_RESET);

  if (curReset && !prevResetCoil) {
    Serial.println("[RESET] Reset ke-1 → clear alarm, Maintenance ON");
    auto clearAlarm = [](Alarm& a, uint16_t coilAddr) {
      if (a.state == AlarmState::ACKED) {
        a.state = AlarmState::CLEARED;
        modbus.Coil(coilAddr, false);
      }
    };
    clearAlarm(alarmPH,    COIL_ALARM_PH);
    clearAlarm(alarmLevel, COIL_ALARM_LEVEL);
    clearAlarm(alarmFlow,  COIL_ALARM_FLOW);
    maintenanceOn = true;
    modbus.Coil(COIL_MAINTENANCE, true);
  }

  if (!curReset && prevResetCoil) {
    Serial.println("[RESET] Reset ke-2 → Maintenance OFF, semua normal");
    auto normalAlarm = [](Alarm& a) {
      if (a.state == AlarmState::CLEARED) a.state = AlarmState::NORMAL;
    };
    normalAlarm(alarmPH);
    normalAlarm(alarmLevel);
    normalAlarm(alarmFlow);
    maintenanceOn = false;
    modbus.Coil(COIL_MAINTENANCE, false);
  }

  prevResetCoil = curReset;
}

// ─────────────────────────────────────────────────────────────
// SINKRONISASI DATA → MODBUS REGISTER
// ─────────────────────────────────────────────────────────────
void updateModbusData() {
  setFloatReg(HR_PH_P1,    p1.ph);
  setFloatReg(HR_LEVEL_P1, p1.levelLogPct);
  setFloatReg(HR_LEVEL_P2, p2.levelLogPct);
  setFloatReg(HR_FLOW_P2,  p2.flowLPM);
  modbus.Coil(COIL_VALVE_P1,      p1.valveOpen);
  modbus.Coil(COIL_VALVE_P2_MAIN, p2.valveMainOpen);
  modbus.Coil(COIL_VALVE_P2_FS,   p2.valveFsOpen);
}

// ─────────────────────────────────────────────────────────────
// MQTT CALLBACK 
// ─────────────────────────────────────────────────────────────
void onMQTTMessage(char* topic, byte* payload, unsigned int len) {
  String t   = String(topic);
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("[MQTT←] %s : %s\n", topic, msg.c_str());

  JsonDocument doc;
  if (deserializeJson(doc, msg) != DeserializationError::Ok) {
    Serial.println("[ERR] JSON parse gagal"); return;
  }

  // JIKA pompa/mixer sedang menyala, bypass pembaruan nilai pH dari sensor untuk hindari noise induksi listrik / kimia sesaat
  if (t == T_P1_PH) {
    if (cmdP1AcidPumpOn || cmdP1BasePumpOn || cmdP1MixerOn) {
      // Pembacaan pH dikunci pada nilai terakhir agar tidak melompat karena induksi
      p1.phValid = true; 
      p1.lastPhMs = millis();
    } else {
      float val = doc["value"].as<float>();
      if (val > 0.0f) {
        p1.ph = val; p1.phValid = true; p1.lastPhMs = millis();
      } else { p1.phValid = false; }
    }
  }
  else if (t == T_P1_LEVEL) {
    float lit = doc["value_liter"].as<float>();
    float pct = doc["value_pct"].as<float>();
    if (lit >= 0.0f) {
      p1.levelLiter  = lit;
      p1.levelPct    = pct;
      p1.levelLogPct = toLogicalPct(lit, P1_MAX_VOL_L);
      p1.isStable    = doc["is_stable"] | false;  
      p1.levelValid  = true;
      p1.lastLevelMs = millis();
    } else { p1.levelValid = false; p1.isStable = false; }
  }
  else if (t == T_P1_VALVE_STATE) {
    p1.valveOpen = (doc["state"].as<String>() == "OPEN");
  }
  else if (t == T_P2_LEVEL) {
    float lit = doc["value_liter"].as<float>();
    float pct = doc["value_pct"].as<float>();
    if (lit >= 0.0f) {
      p2.levelLiter  = lit;
      p2.levelPct    = pct;
      p2.levelLogPct = toLogicalPct(lit, P2_MAX_VOL_L);
      p2.levelValid  = true;
      p2.lastLevelMs = millis();
    } else { p2.levelValid = false; }
  }
  else if (t == T_P2_FLOW) {
    float lpm  = doc["value"].as<float>();
    p2.flowLPM    = (lpm >= 0.0f) ? lpm : 0.0f;
    p2.flowValid  = true;
    p2.lastFlowMs = millis();
  }
  else if (t == T_P2_VALVE_MAIN_STATE) {
    p2.valveMainOpen = (doc["state"].as<String>() == "OPEN");
  }
  else if (t == T_P2_VALVE_FS_STATE) {
    p2.valveFsOpen = (doc["state"].as<String>() == "OPEN");
  }

  if (!systemReady && p1.phValid && p1.levelValid && p2.levelValid) {
    systemReady = true;
    Serial.println("[SYS] ✓ System READY — data P1 & P2 telah diterima");
  }

  if (t == "wwtp/mtu/cmd") {
    if (doc.containsKey("state")) {
      String st = doc["state"].as<String>();
      if (st == "INITIAL_WAIT") {
        phCtrlState = pHState::INITIAL_WAIT;
        stateStartMs = millis();
      }
      else if (st == "MONITORING") phCtrlState = pHState::MONITORING;
      else if (st == "DOSE_PULSE") phCtrlState = pHState::DOSE_PULSE;
      else if (st == "DOSE_DELAY") phCtrlState = pHState::DOSE_DELAY;
      else if (st == "MIXING") phCtrlState = pHState::MIXING;
      else if (st == "SETTLING") phCtrlState = pHState::SETTLING;
      else if (st == "PH_OK") phCtrlState = pHState::PH_OK;
      Serial.printf("[MTU] ⚠ State diganti secara manual via Web ke: %s\n", st.c_str());
    }
  }
}

// ─────────────────────────────────────────────────────────────
// KONEKSI WiFi
// ─────────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.printf("[WIFI] Connecting ke %s via DHCP...", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\n[WIFI] OK — IP: %s\n", WiFi.localIP().toString().c_str());
  else
    Serial.println("\n[WIFI] Gagal — akan retry non-blocking di loop()");
}

unsigned long lastWifiRetryMs = 0;
bool          wifiReconnecting = false;

void tryReconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) { wifiReconnecting = false; return; }

  unsigned long now = millis();
  if (!wifiReconnecting) {
    wifiReconnecting = true;
    lastWifiRetryMs  = now;
    Serial.printf("[WIFI] Terputus — retry dalam %lus...\n",
                  WIFI_RECONNECT_MS / 1000);
    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    return;
  }
  if (now - lastWifiRetryMs >= WIFI_RECONNECT_MS) {
    lastWifiRetryMs = now;
    if (WiFi.status() == WL_CONNECTED) {
      wifiReconnecting = false;
      Serial.printf("[WIFI] Reconnect OK — IP: %s\n",
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WIFI] Masih gagal, retry...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// ─────────────────────────────────────────────────────────────
// KONEKSI MQTT (Ditambahkan agar wificlient/mqtt di setup valid)
// ─────────────────────────────────────────────────────────────
void subscribeAllTopics() {
  mqtt.subscribe(T_P1_PH);
  mqtt.subscribe(T_P1_LEVEL);
  mqtt.subscribe(T_P1_VALVE_STATE);
  mqtt.subscribe(T_P2_LEVEL);
  mqtt.subscribe(T_P2_FLOW);
  mqtt.subscribe(T_P2_VALVE_MAIN_STATE);
  mqtt.subscribe(T_P2_VALVE_FS_STATE);
  mqtt.subscribe("wwtp/mtu/cmd");
  Serial.println("[MQTT] Subscribe: P1(ph,level,valve) | P2(level,flow,valve_main,valve_fs) | MTU(cmd)");
}

void connectMQTT() {
  Serial.print("[MQTT] Menghubungkan ke broker...");
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    Serial.println(" Terhubung!");
    subscribeAllTopics();
  } else {
    Serial.printf(" Gagal, rc=%d. Mencoba lagi via non-blocking loop.\n", mqtt.state());
  }
}

unsigned long lastMqttRetryMs = 0;

void tryReconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttRetryMs < MQTT_RECONNECT_MS) return;
  lastMqttRetryMs = now;

  Serial.printf("[MQTT] Reconnecting id:%s...\n", MQTT_CLIENT_ID);
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    Serial.println("[MQTT] Reconnect OK");
    subscribeAllTopics();
    resetActuatorTrackers();

    if (bufCount > 0) {
      needFlush = true;
      Serial.printf("[BUF] %d entri offline siap di-flush\n", bufCount);
    }
  } else {
    Serial.printf("[MQTT] Reconnect gagal rc=%d\n", mqtt.state());
  }
}

// ─────────────────────────────────────────────────────────────
// INISIALISASI MODBUS
// ─────────────────────────────────────────────────────────────
void initModbus() {
  modbus.server(MODBUS_PORT);
  for (uint16_t i = 0; i < HR_TOTAL;   i++) modbus.addHreg(i, 0);
  for (uint16_t i = 0; i < COIL_TOTAL; i++) modbus.addCoil(i, false);
  Serial.printf("[MODBUS] Slave aktif — port %d | IP: %s\n",
                MODBUS_PORT, MTU_STATIC_IP);
  Serial.printf("[MODBUS] %d Holding Register | %d Coil\n",
                HR_TOTAL, COIL_TOTAL);
}

// ─────────────────────────────────────────────────────────────
// STATUS MONITOR
// ─────────────────────────────────────────────────────────────
const char* alarmStateStr(AlarmState st) {
  switch (st) {
    case AlarmState::FAULT:   return "FAULT";
    case AlarmState::ACKED:   return "ACKED";
    case AlarmState::CLEARED: return "CLEARED";
    default:                  return "NORMAL";
  }
}

unsigned long lastPrintMs = 0;
void printStatus() {
  if (millis() - lastPrintMs < 5000) return;
  lastPrintMs = millis();

  Serial.println("\n┌──────────────────────────────────────────────────────┐");
  Serial.printf( "│ Ready:%s | MQTT:%s | WiFi:%s | Buf:%d/%d\n",
    systemReady ? "Y" : "N",
    mqtt.connected()              ? "OK"   : "DISC",
    WiFi.status() == WL_CONNECTED ? "OK"   : "DISC",
    bufCount, OFFLINE_BUF_SIZE);
  Serial.println("│──────────────────────────────────────────────────────│");
  Serial.printf( "│ [P1] pH:%.2f %-10s Siklus:%d/%d Fresh:%s Stable:%s\n",
    p1.ph, pHStateStr(phCtrlState), doseCycles, MAX_DOSE_CYCLES,
    isP1DataFresh() ? "Y" : "N",
    p1.isStable     ? "Y" : "N");
  Serial.printf( "│      Lvl:%.1f%%(log) %.2fL\n", p1.levelLogPct, p1.levelLiter);
  Serial.printf( "│      Valve:%-5s Acid:%-3s Base:%-3s Mixer:%-3s\n",
    cmdP1ValveOpen  ? "BUKA" : "TUTUP",
    cmdP1AcidPumpOn ? "ON"   : "OFF",
    cmdP1BasePumpOn ? "ON"   : "OFF",
    cmdP1MixerOn    ? "ON"   : "OFF");
  Serial.println("│──────────────────────────────────────────────────────│");
  Serial.printf( "│ [P2] Lvl:%.1f%%(log) %.2fL Flow:%.2fL/min Fresh:%s\n",
    p2.levelLogPct, p2.levelLiter, p2.flowLPM, isP2DataFresh() ? "Y" : "N");
  Serial.printf( "│      VMain:%-5s VFS:%-5s\n",
    cmdP2ValveMainOpen ? "BUKA" : "TUTUP",
    cmdP2ValveFsOpen   ? "BUKA" : "TUTUP");
  Serial.println("│──────────────────────────────────────────────────────│");
  Serial.printf( "│ [ALM] pH:%-8s Lvl:%-8s Flow:%-8s Maint:%s\n",
    alarmStateStr(alarmPH.state), alarmStateStr(alarmLevel.state),
    alarmStateStr(alarmFlow.state), maintenanceOn ? "ON" : "OFF");
  Serial.println("└──────────────────────────────────────────────────────┘\n");
}

// ─────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n╔════════════════════════════════════════════════════╗");
  Serial.println("║   WWTP MTU — Sistem Kontrol Terpusat               ║");
  Serial.println("║   ESP32 WEMOS D1 R32                               ║");
  Serial.println("╚════════════════════════════════════════════════════╝\n");
  Serial.printf("[CFG] Static IP      : %s\n",    MTU_STATIC_IP);
  Serial.printf("[CFG] DNS Server     : %s\n",    MTU_DNS);
  Serial.printf("[CFG] Broker         : %s:%d\n", MQTT_BROKER, MQTT_PORT);
  Serial.printf("[CFG] pH Aman        : %.1f – %.1f\n",  PH_SAFE_MIN, PH_SAFE_MAX);
  Serial.printf("[CFG] Hysteresis     : basa ≥%.1f | asam ≤%.1f\n",
                PH_DOSE_BASE_STOP, PH_DOSE_ACID_STOP);
  Serial.printf("[CFG] P1 Vol Maks    : %.1fL (80%% dari 15L)\n", P1_MAX_VOL_L);
  Serial.printf("[CFG] P2 Vol Maks    : %.1fL (80%% dari 19L)\n", P2_MAX_VOL_L);
  Serial.printf("[CFG] P2 Interlock   : ≥ %.0f%% logical\n",       P2_INTERLOCK_PCT);
  Serial.printf("[CFG] Offline Buffer : %d slot × %lus = ~%lu menit\n\n",
                OFFLINE_BUF_SIZE,
                SNAPSHOT_INTERVAL_MS / 1000,
                (unsigned long)OFFLINE_BUF_SIZE * SNAPSHOT_INTERVAL_MS / 60000);

  memset(offlineBuf, 0, sizeof(offlineBuf));

  wifiClient.setInsecure(); 
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMQTTMessage);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);

  connectWiFi();
  initModbus();
  connectMQTT(); // Fungsi di atas sekarang valid dipanggil

  Serial.println("[SYS] MTU siap. Menunggu data dari Plant 1 & Plant 2...\n");
  stateStartMs = millis();
}

// ─────────────────────────────────────────────────────────────
// LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
  modbus.task();
  tryReconnectWiFi();
  tryReconnectMQTT();
  if (mqtt.connected()) mqtt.loop();

  if (isP1DataFresh()) {
    runPhStateMachine();
  } else if (systemReady) {
    if (phCtrlState != pHState::PH_FAULT && phCtrlState != pHState::INITIAL_WAIT) {
      stopAllDosing();
      setP1Valve(false);
      phCtrlState = pHState::PH_FAULT;
      Serial.println("[SYS] P1 data stale → PH_FAULT failsafe");
    }
  }

  runValveControlP1();
  runValveControlP2();
  updateAlarms();
  handleResetSwitch();
  updateModbusData();
  runSnapshotTask();

  if (needFlush && mqtt.connected()) {
    flushOfflineBuffer();
  }

  printStatus();
  yield();
}