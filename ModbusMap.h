// ================================================================
// modbus_map.h — Peta Tag Modbus MTU untuk AVEVA InTouch
// WWTP SCADA — ESP32 WEMOS D1 R32
// FILE INI MURNI DOKUMENTASI — TIDAK DIINCLUDE KE PROGRAM MANAPUN
// ================================================================


// ================================================================
// BAGIAN 1 — HOLDING REGISTER
// Modbus Function Code : FC03 (Read Holding Registers)
// InTouch Access Name  : Modbus TCP, alamat IP 192.168.1.100, Port 502
// ================================================================
//
//  Tag Name InTouch       | Tipe Tag  | Alamat Modbus | Offset | Satuan | Min   | Max   | Keterangan
// -----------------------|-----------|---------------|--------|--------|-------|-------|-------------------------
//  MTU_PH_P1             | I/O Real  | 400001        | HR = 0 | —      | 0.0   | 14.0  | pH reaktor Plant 1
//  MTU_LEVEL_P1          | I/O Real  | 400003        | HR = 2 | %      | 0.0   | 100.0 | Level logical Plant 1
//  MTU_LEVEL_P2          | I/O Real  | 400005        | HR = 4 | %      | 0.0   | 100.0 | Level logical Plant 2
//  MTU_FLOW_P2           | I/O Real  | 400007        | HR = 6 | L/min  | 0.0   | 10.0  | Flow keluar Plant 2
// -----------------------|-----------|---------------|--------|--------|-------|-------|-------------------------
//
// Catatan konfigurasi driver Modbus di InTouch:
//   Data Type  : 32-Bit Float
//   Byte Order : Big-Endian (ABCD) — High Word di register pertama
//   Contoh     : MTU_PH_P1 membaca register 400001 + 400002 sekaligus
// ================================================================


// ================================================================
// BAGIAN 2 — COIL
// Modbus Function Code  : FC01 (Read Coils) / FC05 (Write Single Coil)
// InTouch Access Name   : sama seperti Holding Register di atas
// ================================================================
//
//  Tag Name InTouch       | Tipe Tag     | Akses | Alamat Modbus | Offset | Keterangan
// -----------------------|--------------|-------|---------------|--------|------------------------------------------
//  MTU_ALARM_PH          | I/O Discrete | R     | 000001        | C = 0  | 1 = Alarm pH aktif (berkedip saat FAULT)
//  MTU_ALARM_LEVEL       | I/O Discrete | R     | 000002        | C = 1  | 1 = Alarm level aktif
//  MTU_ALARM_FLOW        | I/O Discrete | R     | 000003        | C = 2  | 1 = Alarm flow aktif
//  MTU_ACK_PH            | I/O Discrete | R/W   | 000004        | C = 3  | Tulis 1 dari HMI → acknowledge alarm pH
//  MTU_ACK_LEVEL         | I/O Discrete | R/W   | 000005        | C = 4  | Tulis 1 dari HMI → acknowledge alarm level
//  MTU_ACK_FLOW          | I/O Discrete | R/W   | 000006        | C = 5  | Tulis 1 dari HMI → acknowledge alarm flow
//  MTU_RESET             | I/O Discrete | R/W   | 000007        | C = 6  | 0→1: Reset ke-1 (clear+maint ON), 1→0: Reset ke-2 (maint OFF)
//  MTU_MAINTENANCE       | I/O Discrete | R     | 000008        | C = 7  | 1 = Mode maintenance sedang aktif
//  MTU_VALVE_P1          | I/O Discrete | R     | 000009        | C = 8  | 1 = Valve Plant 1 OPEN
//  MTU_VALVE_P2_MAIN     | I/O Discrete | R     | 000010        | C = 9  | 1 = Valve utama Plant 2 OPEN
//  MTU_VALVE_P2_FS       | I/O Discrete | R     | 000011        | C = 10 | 1 = Valve failsafe Plant 2 OPEN
// -----------------------|--------------|-------|---------------|--------|------------------------------------------
// ================================================================


// ================================================================
// BAGIAN 3 — SETPOINT & THRESHOLD (Referensi Alarm InTouch)
// ================================================================
//
//  Tag Name InTouch  | Tipe Tag    | Nilai  | Satuan | Keterangan
// ------------------|-------------|--------|--------|-----------------------------------
//  MTU_PH_P1        | Alarm Hi    | 8.5    | —      | pH terlalu basa → dosing asam
//  MTU_PH_P1        | Alarm Lo    | 6.5    | —      | pH terlalu asam → dosing basa
//  MTU_PH_P1        | Alarm HiHi  | 10.0   | —      | pH fault ekstrem atas → PH_FAULT
//  MTU_PH_P1        | Alarm LoLo  | 5.0    | —      | pH fault ekstrem bawah → PH_FAULT
// ------------------|-------------|--------|--------|-----------------------------------
//  MTU_LEVEL_P1     | Alarm Hi    | 85.0   | %      | Level tinggi → valve P1 buka
//  MTU_LEVEL_P1     | Alarm Lo    | 10.0   | %      | Level rendah → valve P1 tutup
// ------------------|-------------|--------|--------|-----------------------------------
//  MTU_LEVEL_P2     | Alarm HiHi  | 95.0   | %      | Interlock → valve P1 dipaksa tutup
//  MTU_LEVEL_P2     | Alarm Hi    | 80.0   | %      | Level tinggi → valve failsafe buka
//  MTU_LEVEL_P2     | Alarm Lo    | 10.0   | %      | Level rendah → valve main tutup
// ------------------|-------------|--------|--------|-----------------------------------
//  MTU_FLOW_P2      | Alarm Hi    | 10.0   | L/min  | Flow overflow
//  MTU_FLOW_P2      | Alarm Lo    | 0.1    | L/min  | Flow tersumbat (saat valve main OPEN)
// ================================================================
