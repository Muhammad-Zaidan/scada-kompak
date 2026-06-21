-- ============================================
-- SCHEMA SQLite Lokal — Redundansi WWTP SCADA
-- ============================================

CREATE TABLE IF NOT EXISTS sensor_readings (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  plant_id TEXT NOT NULL,
  sensor_type TEXT NOT NULL,
  value REAL NOT NULL,
  unit TEXT,
  status TEXT,
  is_manual INTEGER DEFAULT 0,
  created_at TEXT DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS level_readings (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  plant_id TEXT NOT NULL,
  value_cm REAL,
  value_pct REAL,
  value_liter REAL,
  unit TEXT,
  status TEXT,
  created_at TEXT DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS valve_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  plant_id TEXT NOT NULL,
  device TEXT,
  state TEXT,
  mode TEXT,
  created_at TEXT DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS alert_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  plant_id TEXT NOT NULL,
  code TEXT,
  message TEXT,
  valve TEXT,
  created_at TEXT DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS flow_readings (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  plant_id TEXT NOT NULL,
  value REAL,
  unit TEXT,
  status TEXT,
  created_at TEXT DEFAULT (datetime('now','localtime'))
);

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
);

CREATE TABLE IF NOT EXISTS test (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  message TEXT,
  created_at TEXT DEFAULT (datetime('now','localtime'))
);
