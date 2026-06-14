╔══════════════════════════════════════════════════════╗
║                  ESP32 Dev Board                     ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║  LEVEL SENSOR (HC-SR04)                              ║
║  GPIO26 ──────────────── HC-SR04 TRIG                ║
║  GPIO27 ── 1kΩ ─────────── HC-SR04 ECHO             ║
║            └── 2kΩ ── GND (voltage divider)          ║
║  VIN/5V ──────────────── HC-SR04 VCC                 ║
║  GND ─────────────────── HC-SR04 GND                 ║
║                                                      ║
║  FLOW SENSOR YF-S201                                 ║
║  GPIO32 ──────────────── YF-S201 Signal (kuning)     ║
║  VIN/5V ──────────────── YF-S201 VCC (merah)         ║
║  GND ─────────────────── YF-S201 GND (hitam)         ║
║                                                      ║
║  RELAY VALVE UTAMA                                   ║
║  GPIO25 ──────────────── Relay-1 IN                  ║
║  VIN/5V ──────────────── Relay-1 VCC                 ║
║  GND ─────────────────── Relay-1 GND                 ║
║                                                      ║
║  RELAY VALVE FAILSAFE                                ║
║  GPIO33 ──────────────── Relay-2 IN                  ║
║  VIN/5V ──────────────── Relay-2 VCC                 ║
║  GND ─────────────────── Relay-2 GND                 ║
║                                                      ║
╠══════════════════════════════════════════════════════╣
║  PSU 12V 3.5A                                        ║
║  12V + ───────────────── ESP32 VIN                   ║
║  GND ─────────────────── ESP32 GND                   ║
╠══════════════════════════════════════════════════════╣
║  JALUR 220VAC ⚠️  (dalam terminal box tertutup)      ║
║                                                      ║
║  PLN Fasa (coklat) ─── MCB ─┬─ Relay-1 COM          ║
║                              └─ Relay-2 COM          ║
║                                                      ║
║  Relay-1 NO ──────────────── Solenoid Utama L1       ║
║  Relay-2 NO ──────────────── Solenoid Failsafe L1    ║
║                                                      ║
║  PLN Netral (biru) ───────┬─ Solenoid Utama L2       ║
║                            └─ Solenoid Failsafe L2   ║
║                                                      ║
║  PLN Ground (kuning-hijau) ── Bodi panel/box         ║
╚══════════════════════════════════════════════════════╝