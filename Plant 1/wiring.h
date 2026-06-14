╔══════════════════════════════════════════════════════════════╗
║                    ESP32 Dev Board                           ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  SENSOR pH (PH-4502C)                                        ║
║  GPIO34 ────────────────── PH-4502C PO (output analog)       ║
║  VIN/5V ────────────────── PH-4502C VCC                      ║
║  GND ───────────────────── PH-4502C GND                      ║
// ─────────────────────────────────────────────────────────────
// SENSOR pH (PH-4502C)
// CATATAN: Modul ini dioperasikan di 3.3V (bukan 5V spesifikasi).
// Nilai kalibrasi PH_CAL_V1/V2/V3 di bawah valid HANYA untuk
// supply 3.3V ini. Jika suatu saat diganti ke 5V, kalibrasi
// harus diulang dari awal.
// ─────────────────────────────────────────────────────────────
║                                                              ║
║  LEVEL SENSOR (HC-SR04) *echo wajib voltage divider!         ║
║  GPIO26 ────────────────── HC-SR04 TRIG                      ║
║  GPIO27 ──── 1kΩ ─────────── HC-SR04 ECHO                   ║
║             └── 2kΩ ── GND  (voltage divider 5V→3.3V)        ║
║  VIN/5V ────────────────── HC-SR04 VCC                       ║
║  GND ───────────────────── HC-SR04 GND                       ║
║                                                              ║
║  OUTLET VALVE (Relay 1-ch)                                   ║
║  GPIO25 ────────────────── Relay IN                          ║
║  VIN/5V ────────────────── Relay VCC                         ║
║  GND ───────────────────── Relay GND                         ║
║  Relay NO ──────────────── Solenoid valve +                  ║
║  Relay COM ─────────────── Sumber 12V solenoid               ║
║                                                              ║
║  POMPA DOSING ASAM HNO3 (Relay 2-ch, port A)                 ║
║  GPIO32 ────────────────── Relay-A IN                        ║
║  Relay-A NO ────────────── Motor pompa asam + (12V)          ║
║  Relay-A COM ───────────── Sumber 12V pompa                  ║
║                                                              ║
║  POMPA DOSING BASA KOH (Relay 2-ch, port B)                  ║
║  GPIO23 ────────────────── Relay-B IN                        ║
║  Relay-B NO ────────────── Motor pompa basa + (12V)          ║
║  Relay-B COM ───────────── Sumber 12V pompa                  ║
║                                                              ║
║  FLOW SENSOR POMPA ASAM (HAL built-in)                       ║
║  GPIO18 ──── 10kΩ ── 3.3V  (pull-up WAJIB!)                 ║
║  GPIO18 ────────────────── Flow sensor ASAM OUT              ║
║  5V ────────────────────── Flow sensor ASAM 5V               ║
║  GND ───────────────────── Flow sensor ASAM GND              ║
║                                                              ║
║  FLOW SENSOR POMPA BASA (HAL built-in)                       ║
║  GPIO19 ──── 10kΩ ── 3.3V  (pull-up WAJIB!)                 ║
║  GPIO19 ────────────────── Flow sensor BASA OUT              ║
║  5V ────────────────────── Flow sensor BASA 5V               ║
║  GND ───────────────────── Flow sensor BASA GND              ║
║                                                              ║
║  STIRRER MOTOR RS775 (BTS7960 Driver)                        ║
║  GPIO14 ────────────────── BTS7960 RPWM                      ║
║  GPIO12 ────────────────── BTS7960 R_EN + L_EN (bridge)      ║
║  GND ───────────────────── BTS7960 GND sinyal                ║
║  3.3V ──────────────────── BTS7960 VCC sinyal                ║
║                                                              ║
║  BTS7960 Power (terpisah dari ESP32!)                        ║
║  12V PSU + ─────────────── BTS7960 B+ & M+                   ║
║  12V PSU - ─────────────── BTS7960 B- & GND power            ║
║  BTS7960 Motor Out ─────── RS775 motor terminal              ║
║                                                              ║
║  LPWM → GND (hardcode LOW, satu arah saja)                   ║
╚══════════════════════════════════════════════════════════════╝

CATATAN POWER:
  - ESP32 + sensor 3.3V  → dari USB / regulator board
  - HC-SR04, PH-4502C    → 5V dari pin VIN ESP32
  - Pompa dosing, RS775  → PSU 12V TERPISAH
  - GND ESP32 & PSU 12V harus di-bridge (common ground)