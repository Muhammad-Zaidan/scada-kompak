@echo off
color 0A
title WWTP Serial Logger
echo ========================================================
echo Memulai WWTP Serial Logger...
echo Pastikan Arduino IDE / Serial Monitor sedang DITUTUP!
echo ========================================================

:: Menjalankan script PowerShell
powershell.exe -ExecutionPolicy Bypass -File "%~dp0SerialLogger.ps1" -PortName "COM3" -BaudRate 115200 -LogFile "%~dp0wwtp_data.jsonl"

pause
