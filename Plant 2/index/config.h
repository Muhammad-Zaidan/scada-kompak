// ============================================================
// config.h — Plant 2 FINAL
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
#define MQTT_CLIENT_ID          "plant2_esp32_001"

// ─────────────────────────────────────────────────────────────
// TOPIK MQTT
// ─────────────────────────────────────────────────────────────
#define TOPIC_LEVEL             "wwtp/plant2/level"
#define TOPIC_FLOW              "wwtp/plant2/flow"
#define TOPIC_VALVE_MAIN        "wwtp/plant2/valve/main/state"
#define TOPIC_VALVE_FS          "wwtp/plant2/valve/failsafe/state"
#define TOPIC_ALERT             "wwtp/alert"
#define TOPIC_CMD_MAIN          "wwtp/plant2/valve/main/cmd"
#define TOPIC_CMD_FS            "wwtp/plant2/valve/failsafe/cmd"

// ─────────────────────────────────────────────────────────────
// IDENTITAS
// ─────────────────────────────────────────────────────────────
#define PLANT_ID                "plant2"

// ─────────────────────────────────────────────────────────────
// PIN
// ─────────────────────────────────────────────────────────────
#define TRIG_PIN                26
#define ECHO_PIN                27
#define RELAY_MAIN_PIN          25      // Valve utama — High-Z = OFF
#define RELAY_FS_PIN            33      // Valve failsafe — High-Z = OFF
#define FLOW_PIN                32      // YF-S201 signal

// ─────────────────────────────────────────────────────────────
// DIMENSI TANGKI — UPDATE SETELAH KALIBRASI LEVEL
// ─────────────────────────────────────────────────────────────
#define SENSOR_OFFSET_CM        7.88f    // ← UPDATE dari hasil SETFULL
#define TANK_HEIGHT_CM          29.01f   // ← UPDATE dari hasil SETEMPTY - SETFULL
#define TANK_RADIUS_CM          10.0f    // ← ukur fisik (jari-jari tangki)

// Lookup table volumetrik {jarak_cm, volume_liter}
// Urut: jarak BESAR (kosong) → jarak KECIL (penuh)
// ← UPDATE setelah kalibrasi
#define VOL_TABLE_SIZE          2
static const float VOL_TABLE[][2] = {
  {36.89f, 0.000f},  // kosong
  {7.88f,  (3.14159265f * TANK_RADIUS_CM * TANK_RADIUS_CM * TANK_HEIGHT_CM / 1000.0f)}, // penuh
};

// ─────────────────────────────────────────────────────────────
// LOGIKA VALVE — HYSTERESIS
// ─────────────────────────────────────────────────────────────
#define LEVEL_MAIN_OPEN         50.0f   // Valve utama buka jika level >= ini
#define LEVEL_MAIN_CLOSE        10.0f   // Valve utama tutup jika level <= ini
#define LEVEL_FS_OPEN           80.0f   // Failsafe buka jika level >= ini
#define LEVEL_FS_CLOSE          30.0f   // Failsafe tutup jika level <= ini

// ─────────────────────────────────────────────────────────────
// FLOW SENSOR YF-S201
// ← UPDATE YFS201_FACTOR setelah kalibrasi
// ─────────────────────────────────────────────────────────────
#define YFS201_FACTOR           7.5f    // Hz per L/menit — UPDATE dari kalibrasi
#define FLOW_MAX_LPM            10.0f   // Batas debit maksimum
#define FLOW_FAULT_MS           5000    // Alert jika flow=0 saat valve buka > ini

// ─────────────────────────────────────────────────────────────
// TIMING PUBLISH
// ─────────────────────────────────────────────────────────────
#define SAMPLING_MS             1000
#define RBE_LEVEL_DEADBAND      0.05f   // Liter
#define RBE_FLOW_DEADBAND       0.2f    // L/menit
#define RBE_HEARTBEAT_MS        30000

// ─────────────────────────────────────────────────────────────
// OFFLINE BUFFER
// ─────────────────────────────────────────────────────────────
#define OFFLINE_BUF_SIZE        30