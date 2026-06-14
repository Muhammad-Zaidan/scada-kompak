// ============================================================
// config.h — Plant 1 FINAL (Dumb Executor)
// Arsitektur: RTU hanya eksekusi perintah ON/OFF dari MTU
// ============================================================
#pragma once

// ─────────────────────────────────────────────────────────────
// JARINGAN
// ─────────────────────────────────────────────────────────────
#define WIFI_SSID               "Polban"
#define WIFI_PASSWORD           "12polban34"

// ─────────────────────────────────────────────────────────────
// MQTT BROKER
// ─────────────────────────────────────────────────────────────
#define MQTT_BROKER             "1181cbf946a740f4b5a02a311e1d483e.s1.eu.hivemq.cloud"
#define MQTT_PORT               8883
#define MQTT_USER               "kompak1"
#define MQTT_PASS               "Kompak2000"
#define MQTT_CLIENT_ID          "plant1_esp32_001"

// ─────────────────────────────────────────────────────────────
// TOPIK MQTT — PUBLISH (data sensor ke MTU)
// ─────────────────────────────────────────────────────────────
#define TOPIC_PH                "wwtp/plant1/ph"
#define TOPIC_LEVEL             "wwtp/plant1/level"
#define TOPIC_VALVE             "wwtp/plant1/valve/state"
#define TOPIC_ALERT             "wwtp/alert"

// ─────────────────────────────────────────────────────────────
// TOPIK MQTT — SUBSCRIBE (perintah aktuator dari MTU)
// Arsitektur: satu topic per aktuator, payload {cmd: "ON"/"OFF"}
// ─────────────────────────────────────────────────────────────
#define TOPIC_VALVE_CMD         "wwtp/plant1/valve/cmd"
#define TOPIC_ACID_CMD          "wwtp/plant1/dosing/acid/cmd"
#define TOPIC_BASE_CMD          "wwtp/plant1/dosing/base/cmd"
#define TOPIC_MIXER_CMD         "wwtp/plant1/mixer/cmd"

// ─────────────────────────────────────────────────────────────
// IDENTITAS
// ─────────────────────────────────────────────────────────────
#define PLANT_ID                "plant1"

// ─────────────────────────────────────────────────────────────
// PIN
// ─────────────────────────────────────────────────────────────
#define PH_PIN                  34
#define TRIG_PIN                26
#define ECHO_PIN                27
#define RELAY_VALVE_PIN         25      // Outlet valve
#define RELAY_PUMP_ACID_PIN     32      // Pompa asam HNO3 10%
#define RELAY_PUMP_BASE_PIN     23      // Pompa basa KOH 10% (dipindah dari 33)
#define STIRRER_PWM_PIN         14      // BTS7960 RPWM
#define STIRRER_EN_PIN          12      // BTS7960 R_EN + L_EN

// ─────────────────────────────────────────────────────────────
// DIMENSI TANGKI — UPDATE SETELAH KALIBRASI LEVEL
// ─────────────────────────────────────────────────────────────
#define SENSOR_OFFSET_CM        6.11f    // ← UPDATE dari hasil SETFULL
#define TANK_HEIGHT_CM          21.61f   // ← UPDATE dari hasil SETEMPTY - SETFULL
#define TANK_RADIUS_CM          10.0f    // ← ukur fisik (jari-jari tangki)

// Lookup table volumetrik {jarak_cm, volume_liter}
// Urut: jarak BESAR (kosong) → jarak KECIL (penuh)
// ← UPDATE seluruh tabel dan VOL_TABLE_SIZE setelah kalibrasi
#define VOL_TABLE_SIZE          2
static const float VOL_TABLE[][2] = {
  {27.72f, 0.000f},   // kosong
  {6.11f,  (3.14159265f * TANK_RADIUS_CM * TANK_RADIUS_CM * TANK_HEIGHT_CM / 1000.0f)},  // penuh
};

// ─────────────────────────────────────────────────────────────
// KALIBRASI pH
// CATATAN: valid untuk supply 3.3V ke PH-4502C
// ─────────────────────────────────────────────────────────────
#define PH_CAL_V1               3.2228f
#define PH_CAL_PH1              4.01f
#define PH_CAL_V2               2.5833f
#define PH_CAL_PH2              6.84f
#define PH_CAL_V3               2.2005f
#define PH_CAL_PH3              9.14f

// ─────────────────────────────────────────────────────────────
// FILTER pH
// ─────────────────────────────────────────────────────────────
#define PH_SAMPLES              30
#define PH_TRIM                 6
#define EMA_ALPHA               0.15f

// ─────────────────────────────────────────────────────────────
// SETPOINT pH (untuk deteksi fault lokal — failsafe RTU)
// ─────────────────────────────────────────────────────────────
#define PH_FAULT_LOW            5.5f
#define PH_FAULT_HIGH           9.5f

// ─────────────────────────────────────────────────────────────
// LOGIKA VALVE OUTLET — failsafe lokal RTU
// ─────────────────────────────────────────────────────────────
#define LEVEL_CLOSE_PCT         10.0f   // Tutup valve jika level <= ini (RTU failsafe)

// ─────────────────────────────────────────────────────────────
// DETEKSI STABILITAS LEVEL
// Level dianggap stabil jika tidak berubah > 0.05L selama 1 menit
// ─────────────────────────────────────────────────────────────
#define LEVEL_STABLE_DELTA_L    0.05f   // Perubahan volume (L) dianggap naik
#define LEVEL_STABLE_MS         60000   // 1 menit — sesuai permintaan

// ─────────────────────────────────────────────────────────────
// STIRRER — kecepatan rendah untuk simple mixing
// ─────────────────────────────────────────────────────────────
#define STIRRER_SPEED           15      // 0-255, putaran pelan
#define STIRRER_RAMP_STEP       1       // Naik 1 per langkah
#define STIRRER_RAMP_DELAY_MS   30      // ms per langkah (~450ms total ramp)

// ─────────────────────────────────────────────────────────────
// TIMING PUBLISH
// ─────────────────────────────────────────────────────────────
#define SAMPLING_MS             1000
#define RBE_PH_DEADBAND         0.05f
#define RBE_LEVEL_DEADBAND      0.05f
#define RBE_HEARTBEAT_MS        30000

// ─────────────────────────────────────────────────────────────
// OFFLINE BUFFER
// ─────────────────────────────────────────────────────────────
#define OFFLINE_BUF_SIZE        30