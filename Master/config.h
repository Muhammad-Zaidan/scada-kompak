// ============================================================
// config.h — MTU FINAL (Updated)
// ============================================================
#pragma once

// ─────────────────────────────────────────────────────────────
// Wi-Fi
// ─────────────────────────────────────────────────────────────
#define WIFI_SSID               "Redmi Note 14"
#define WIFI_PASSWORD           "marleskazir0"

// ─────────────────────────────────────────────────────────────
// Static IP MTU (wajib statis untuk Modbus TCP dari InTouch)
// ─────────────────────────────────────────────────────────────
#define MTU_STATIC_IP           "192.168.1.100"
#define MTU_GATEWAY             "192.168.1.1"
#define MTU_SUBNET              "255.255.255.0"
#define MTU_DNS                 "8.8.8.8"

// ─────────────────────────────────────────────────────────────
// MQTT BROKER
// ─────────────────────────────────────────────────────────────
#define MQTT_BROKER             "1181cbf946a740f4b5a02a311e1d483e.s1.eu.hivemq.cloud"
#define MQTT_PORT               8883
#define MQTT_USER               "kompak"
#define MQTT_PASS               "Kompak2026"
#define MQTT_CLIENT_ID          "wwtp_mtu_001"

// ─────────────────────────────────────────────────────────────
// TOPIK SUBSCRIBE — data dari RTU
// ─────────────────────────────────────────────────────────────
#define T_P1_PH                 "wwtp/plant1/ph"
#define T_P1_LEVEL              "wwtp/plant1/level"
#define T_P1_VALVE_STATE        "wwtp/plant1/valve/state"
#define T_P2_LEVEL              "wwtp/plant2/level"
#define T_P2_FLOW               "wwtp/plant2/flow"
#define T_P2_VALVE_MAIN_STATE   "wwtp/plant2/valve/main/state"
#define T_P2_VALVE_FS_STATE     "wwtp/plant2/valve/failsafe/state"

// ─────────────────────────────────────────────────────────────
// TOPIK PUBLISH — perintah ke RTU Plant 1 (per aktuator)
// Arsitektur: MTU kontrol individual, RTU = dumb executor
// ─────────────────────────────────────────────────────────────
#define T_P1_VALVE_CMD          "wwtp/plant1/valve/cmd"
#define T_P1_DOSING_ACID_CMD    "wwtp/plant1/dosing/acid/cmd"
#define T_P1_DOSING_BASE_CMD    "wwtp/plant1/dosing/base/cmd"
#define T_P1_MIXER_CMD          "wwtp/plant1/mixer/cmd"

// ─────────────────────────────────────────────────────────────
// TOPIK PUBLISH — perintah ke RTU Plant 2
// ─────────────────────────────────────────────────────────────
#define T_P2_MAIN_CMD           "wwtp/plant2/valve/main/cmd"
#define T_P2_FS_CMD             "wwtp/plant2/valve/failsafe/cmd"

// ─────────────────────────────────────────────────────────────
// TOPIK HISTORY & ALERT
// ─────────────────────────────────────────────────────────────
#define T_MTU_HISTORY           "wwtp/mtu/history"
#define T_MTU_DOSING_STATE      "wwtp/mtu/dosing/state"
#define T_ALERT                 "wwtp/alert"

// ─────────────────────────────────────────────────────────────
// IDENTITAS
// ─────────────────────────────────────────────────────────────
#define MTU_ID                  "mtu"

// ─────────────────────────────────────────────────────────────
// MODBUS TCP
// ─────────────────────────────────────────────────────────────
#define MODBUS_PORT             502

// Holding Register (float IEEE754 = 2 register per nilai)
#define HR_PH_P1                0   // 40001-40002
#define HR_LEVEL_P1             2   // 40003-40004
#define HR_LEVEL_P2             4   // 40005-40006
#define HR_FLOW_P2              6   // 40007-40008
#define HR_TOTAL                8

// Coil
#define COIL_ALARM_PH           0
#define COIL_ALARM_LEVEL        1
#define COIL_ALARM_FLOW         2
#define COIL_ACK_PH             3
#define COIL_ACK_LEVEL          4
#define COIL_ACK_FLOW           5
#define COIL_RESET              6
#define COIL_MAINTENANCE        7
#define COIL_VALVE_P1           8
#define COIL_VALVE_P2_MAIN      9
#define COIL_VALVE_P2_FS        10
#define COIL_TOTAL              11

// ─────────────────────────────────────────────────────────────
// PARAMETER pH Plant 1 (digunakan MTU untuk logika dosing)
// ─────────────────────────────────────────────────────────────
#define PH_DOSE_TARGET          7.0f    // Target pH dosing
#define PH_SAFE_MIN             6.8f    // Batas bawah aman — mulai dosing basa
#define PH_SAFE_MAX             7.2f    // Batas atas aman — mulai dosing asam
#define PH_FAULT_LOW            5.5f    // Fault ekstrem bawah → PH_FAULT
#define PH_FAULT_HIGH           8.5f    // Fault ekstrem atas → PH_FAULT

// Hysteresis stop — berhenti dosing di titik ini (bukan tepat 7.0)
// Diatur agar dosing berhenti begitu pH MASUK zona aman, bukan mengejar 7.0
// Ini menghemat cairan dosing secara signifikan.
#define PH_DOSE_BASE_STOP       6.85f   // Stop dosing basa jika pH >= ini (cukup masuk zona aman)
#define PH_DOSE_ACID_STOP       7.15f   // Stop dosing asam jika pH <= ini (cukup masuk zona aman)

// ─────────────────────────────────────────────────────────────
// PARAMETER DOSING PROPORSIONAL PULSA (dijalankan di MTU)
//
// Alur: DOSE_PULSE → DOSE_DELAY → MIXING → SETTLING → cek pH
// ─────────────────────────────────────────────────────────────
#define PULSE_PER_ML            4.63f   // Dari kalibrasi: 500 pulsa = 108 mL
#define KP_DOSING_ML_PER_PH     10.0f   // Dosis mL cairan per 1.0 selisih pH (dinaikkan dari 5.0 agar lebih efektif)
#define DOSE_PULSE_MAX_MS       15000UL // Timeout maksimal pompa nyala per siklus (jaga-jaga pulsa macet)
#define DOSE_TO_MIXER_MS        2000UL  // Delay setelah pompa mati sebelum mixer nyala
#define MIXER_RUN_MS            30000UL // Durasi mixer mengaduk (30 detik)
#define SETTLE_MS               300000UL// Durasi settling setelah mixer mati (5 menit = 300.000 ms)
#define MAX_DOSE_CYCLES         10      // Maksimum siklus dosing per batch (dinaikkan karena dosis dicicil)

// ─────────────────────────────────────────────────────────────
// VOLUME Plant 1 (untuk kalkulasi logika)
// ← UPDATE setelah kalibrasi level sensor Plant 1
// ─────────────────────────────────────────────────────────────
#define P1_TANK_HEIGHT_CM       21.61f
#define P1_TANK_RADIUS_CM       10.0f    // ← ukur fisik (jari-jari tangki)
#define P1_MAX_VOL_L            (3.14159265f * P1_TANK_RADIUS_CM * P1_TANK_RADIUS_CM * P1_TANK_HEIGHT_CM / 1000.0f)

// ─────────────────────────────────────────────────────────────
// LOGIKA VALVE Plant 1
// ─────────────────────────────────────────────────────────────
#define P1_LEVEL_OPEN_PCT       80.0f
#define P1_LEVEL_CLOSE_PCT      10.0f
#define P1_MIN_DOSING_PCT       80.0f   // Dosing & mixing hanya aktif jika level >= ini

// ─────────────────────────────────────────────────────────────
// LOGIKA VALVE Plant 2
// ─────────────────────────────────────────────────────────────
#define P2_TANK_HEIGHT_CM       29.01f
#define P2_TANK_RADIUS_CM       10.0f    // ← ukur fisik (jari-jari tangki)
#define P2_MAX_VOL_L            (3.14159265f * P2_TANK_RADIUS_CM * P2_TANK_RADIUS_CM * P2_TANK_HEIGHT_CM / 1000.0f)
#define P2_MAIN_OPEN_PCT        50.0f
#define P2_MAIN_CLOSE_PCT       10.0f
#define P2_FS_OPEN_PCT          80.0f
#define P2_FS_CLOSE_PCT         30.0f
#define P2_INTERLOCK_PCT        95.0f   // Tutup valve P1 jika P2 >= ini

// ─────────────────────────────────────────────────────────────
// TIMING SISTEM
// ─────────────────────────────────────────────────────────────
#define DATA_TIMEOUT_MS         45000UL
#define ALARM_FLIP_MS           500UL
#define WIFI_RECONNECT_MS       10000UL
#define MQTT_RECONNECT_MS       5000UL
#define SNAPSHOT_INTERVAL_MS    30000UL
#define INITIAL_WAIT_MS         180000UL// Waktu tunggu awal saat mesin baru nyala (3 menit)

// ─────────────────────────────────────────────────────────────
// OFFLINE BUFFER
// 60 entri × 30 detik = 30 menit data offline
// ─────────────────────────────────────────────────────────────
#define OFFLINE_BUF_SIZE        60

// ─────────────────────────────────────────────────────────────
// ENUM & STRUCT
// Definisi di config.h agar tersedia sebelum prototype Arduino
// ─────────────────────────────────────────────────────────────

// State machine pH — dijalankan di MTU
// Alur: MONITORING → DOSE_PULSE → DOSE_DELAY → MIXING → SETTLING
//       → cek pH → jika belum OK → DOSE_PULSE lagi (max 5 siklus)
//       → jika OK → PH_OK
enum class pHState {
  INITIAL_WAIT, // Menunggu sensor stabil saat MTU baru dinyalakan
  MONITORING,   // Cek pH + stabilitas level, putuskan apakah perlu dosing
  DOSE_PULSE,   // Pompa dosing ON selama DOSE_PULSE_MS
  DOSE_DELAY,   // Delay sebelum mixer nyala
  MIXING,       // Mixer ON selama MIXER_RUN_MS
  SETTLING,     // Settling time — tunggu sensor pH stabil
  PH_OK,        // pH dalam range, tidak perlu dosing
  PH_FAULT      // pH fault ekstrem atau max siklus tercapai
};

enum class AlarmState { NORMAL, FAULT, ACKED, CLEARED };

struct Alarm {
  AlarmState    state       = AlarmState::NORMAL;
  bool          flipFlop    = false;
  unsigned long lastFlipMs  = 0;
  bool          prevAckCoil = false;
};

struct SensorSnapshot {
  unsigned long capturedMs;
  float  ph;
  float  levelP1Pct;
  float  levelP2Pct;
  float  flowP2;
  bool   valveP1;
  bool   valveP2Main;
  bool   valveP2Fs;
  uint8_t phStateIdx;
  bool   valid;
};