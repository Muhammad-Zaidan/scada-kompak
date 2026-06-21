// ============================================================
// WWTP Local Logger — MQTT → SQLite (sql.js, pure JavaScript)
// Jalankan di komputer lokal yang terhubung ke Master ESP32
// ============================================================
const mqtt = require('mqtt');
const initSqlJs = require('sql.js');
const fs = require('fs');
const path = require('path');

// ─── Konfigurasi MQTT (sama persis dengan Web Dashboard) ─────
const BROKER_URL = 'mqtts://1181cbf946a740f4b5a02a311e1d483e.s1.eu.hivemq.cloud:8883';
const MQTT_OPTIONS = {
  username: 'kompak',
  password: 'Kompak2026',
  clientId: `local_logger_${Math.random().toString(16).slice(3)}`,
  clean: true,
  reconnectPeriod: 5000,
};

// ─── Konfigurasi SQLite ──────────────────────────────────────
const DB_PATH = path.join(__dirname, 'wwtp_backup.db');
const SAVE_INTERVAL_MS = 10000; // Auto-save ke disk setiap 10 detik

async function main() {
  // Inisialisasi sql.js
  const SQL = await initSqlJs();

  // Buka database yang sudah ada, atau buat baru
  let db;
  if (fs.existsSync(DB_PATH)) {
    const buffer = fs.readFileSync(DB_PATH);
    db = new SQL.Database(buffer);
    console.log('[DB] Database lama dimuat.');
  } else {
    db = new SQL.Database();
    console.log('[DB] Database baru dibuat.');
  }

  console.log(`\n╔════════════════════════════════════════════════════╗`);
  console.log(`║   WWTP Local Logger — MQTT → SQLite                ║`);
  console.log(`║   File: ${DB_PATH}`);
  console.log(`╚════════════════════════════════════════════════════╝\n`);

  // ─── Buat Tabel ──────────────────────────────────────────
  db.run(`
    CREATE TABLE IF NOT EXISTS sensor_readings (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id TEXT NOT NULL,
      sensor_type TEXT,
      value REAL,
      unit TEXT,
      status TEXT,
      is_manual INTEGER DEFAULT 0,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    )
  `);
  db.run(`
    CREATE TABLE IF NOT EXISTS level_readings (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id TEXT NOT NULL,
      value_cm REAL,
      value_pct REAL,
      value_liter REAL,
      unit TEXT,
      status TEXT,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    )
  `);
  db.run(`
    CREATE TABLE IF NOT EXISTS valve_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id TEXT NOT NULL,
      device TEXT,
      state TEXT,
      mode TEXT,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    )
  `);
  db.run(`
    CREATE TABLE IF NOT EXISTS alert_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id TEXT NOT NULL,
      code TEXT,
      message TEXT,
      valve TEXT,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    )
  `);
  db.run(`
    CREATE TABLE IF NOT EXISTS flow_readings (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id TEXT NOT NULL,
      value REAL,
      unit TEXT,
      status TEXT,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    )
  `);
  db.run(`
    CREATE TABLE IF NOT EXISTS dosing_log (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      plant_id TEXT NOT NULL,
      status TEXT,
      cycle INTEGER DEFAULT 0,
      target_ml REAL DEFAULT 0,
      delivered_ml REAL DEFAULT 0,
      pump_base INTEGER DEFAULT 0,
      pump_acid INTEGER DEFAULT 0,
      stirrer INTEGER DEFAULT 0,
      created_at TEXT DEFAULT (datetime('now','localtime'))
    )
  `);

  console.log('[DB] Tabel SQLite siap.\n');

  // ─── Simpan ke Disk Secara Berkala ─────────────────────────
  let dirty = false;

  function saveToDisk() {
    if (dirty) {
      const data = db.export();
      const buffer = Buffer.from(data);
      fs.writeFileSync(DB_PATH, buffer);
      dirty = false;
    }
  }

  const saveTimer = setInterval(saveToDisk, SAVE_INTERVAL_MS);

  // ─── Topic → Handler Mapping ───────────────────────────────
  const HANDLERS = {
    'wwtp/plant1/ph': (data) => {
      db.run(
        `INSERT INTO sensor_readings (plant_id, sensor_type, value, unit, status, is_manual)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [data.plant_id || 'plant1', data.sensor_type || 'ph', parseFloat(data.value) || 0,
         data.unit || 'pH', data.status || 'Unknown', data.is_manual ? 1 : 0]
      );
      console.log(`  [pH]    ${data.value} ${data.unit} — ${data.status}`);
    },

    'wwtp/plant1/level': (data) => {
      db.run(
        `INSERT INTO level_readings (plant_id, value_cm, value_pct, value_liter, unit, status)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [data.plant_id || 'plant1', parseFloat(data.value_cm) || 0, parseFloat(data.value_pct) || 0,
         parseFloat(data.value_liter) || 0, data.unit || 'cm', data.status || 'Unknown']
      );
      console.log(`  [Lvl1]  ${data.value_pct}% — ${data.value_liter}L`);
    },

    'wwtp/plant1/valve/state': (data) => {
      db.run(
        `INSERT INTO valve_log (plant_id, device, state, mode) VALUES (?, ?, ?, ?)`,
        [data.plant_id || 'plant1', data.device || 'outlet_valve', data.state || 'UNKNOWN', data.mode || 'AUTO']
      );
      console.log(`  [Vlv1]  ${data.device} → ${data.state}`);
    },

    'wwtp/plant2/level': (data) => {
      db.run(
        `INSERT INTO level_readings (plant_id, value_cm, value_pct, value_liter, unit, status)
         VALUES (?, ?, ?, ?, ?, ?)`,
        [data.plant_id || 'plant2', parseFloat(data.value_cm) || 0, parseFloat(data.value_pct) || 0,
         parseFloat(data.value_liter) || 0, data.unit || 'cm', data.status || 'Unknown']
      );
      console.log(`  [Lvl2]  ${data.value_pct}% — ${data.value_liter}L`);
    },

    'wwtp/plant2/flow': (data) => {
      db.run(
        `INSERT INTO flow_readings (plant_id, value, unit, status) VALUES (?, ?, ?, ?)`,
        [data.plant_id || 'plant2', parseFloat(data.value) || 0, data.unit || 'L/min', data.status || 'Unknown']
      );
      console.log(`  [Flow]  ${data.value} ${data.unit}`);
    },

    'wwtp/plant2/valve/main/state': (data) => {
      db.run(
        `INSERT INTO valve_log (plant_id, device, state, mode) VALUES (?, ?, ?, ?)`,
        [data.plant_id || 'plant2', data.device || 'main_valve', data.state || 'UNKNOWN', data.mode || 'AUTO']
      );
      console.log(`  [Vlv2M] ${data.state}`);
    },

    'wwtp/plant2/valve/failsafe/state': (data) => {
      db.run(
        `INSERT INTO valve_log (plant_id, device, state, mode) VALUES (?, ?, ?, ?)`,
        [data.plant_id || 'plant2', data.device || 'failsafe_valve', data.state || 'UNKNOWN', data.mode || 'AUTO']
      );
      console.log(`  [Vlv2F] ${data.state}`);
    },

    'wwtp/alert': (data) => {
      db.run(
        `INSERT INTO alert_log (plant_id, code, message, valve) VALUES (?, ?, ?, ?)`,
        [data.plant_id || 'unknown', data.code || 'UNKNOWN', data.message || '', data.valve || 'UNKNOWN']
      );
      console.log(`  [ALM]   ${data.code}: ${data.message}`);
    },

    'wwtp/mtu/dosing/state': (data) => {
      db.run(
        `INSERT INTO dosing_log (plant_id, status, cycle, target_ml, delivered_ml, pump_base, pump_acid, stirrer)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
        [data.plant_id || 'mtu', data.status || 'UNKNOWN', parseInt(data.cycle) || 0,
         parseFloat(data.target_ml) || 0, parseFloat(data.delivered_ml) || 0,
         data.pump_base ? 1 : 0, data.pump_acid ? 1 : 0, data.stirrer ? 1 : 0]
      );
      console.log(`  [DOSE]  ${data.status} — siklus ${data.cycle}`);
    },
  };

  // ─── Koneksi MQTT ──────────────────────────────────────────
  const client = mqtt.connect(BROKER_URL, MQTT_OPTIONS);
  let msgCount = 0;

  client.on('connect', () => {
    console.log('[MQTT] ✓ Terhubung ke broker HiveMQ Cloud');

    const topics = Object.keys(HANDLERS);
    client.subscribe(topics, { qos: 1 }, (err) => {
      if (err) {
        console.error('[MQTT] ✗ Gagal subscribe:', err);
      } else {
        console.log(`[MQTT] Subscribe ke ${topics.length} topik:\n`);
        topics.forEach(t => console.log(`       → ${t}`));
        console.log(`\n[READY] Menunggu data dari ESP32...\n`);
      }
    });
  });

  client.on('message', (topic, message) => {
    try {
      const data = JSON.parse(message.toString());
      const handler = HANDLERS[topic];
      if (handler) {
        handler(data);
        dirty = true;
        msgCount++;
        if (msgCount % 50 === 0) {
          saveToDisk();
          console.log(`\n--- ${msgCount} pesan tersimpan di SQLite ---\n`);
        }
      }
    } catch (err) {
      console.warn(`[WARN] Gagal parse dari ${topic}:`, err.message);
    }
  });

  client.on('error', (err) => console.error('[MQTT] Error:', err.message));
  client.on('reconnect', () => console.log('[MQTT] Reconnecting...'));
  client.on('offline', () => console.log('[MQTT] Offline — data terakhir tetap aman di SQLite'));

  // ─── Graceful Shutdown ─────────────────────────────────────
  process.on('SIGINT', () => {
    console.log('\n[EXIT] Menyimpan database terakhir...');
    clearInterval(saveTimer);
    saveToDisk();
    client.end();
    db.close();
    console.log(`[EXIT] Total ${msgCount} pesan tersimpan. Database aman.`);
    process.exit(0);
  });
}

main().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
