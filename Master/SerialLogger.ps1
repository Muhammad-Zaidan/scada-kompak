param(
    [string]$PortName = "COM3",
    [int]$BaudRate = 115200,
    [string]$LogFile = "wwtp_data.jsonl"
)

# Pastikan file log ada
if (-not (Test-Path $LogFile)) {
    New-Item -Path $LogFile -ItemType File | Out-Null
}

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host " WWTP Serial Data Logger" -ForegroundColor Cyan
Write-Host " Port : $PortName"
Write-Host " Baud : $BaudRate"
Write-Host " File : $LogFile"
Write-Host " (Mengekstrak pesan JSON dari serial monitor)"
Write-Host " Tekan Ctrl+C untuk berhenti"
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host ""

try {
    $port = New-Object System.IO.Ports.SerialPort $PortName, $BaudRate, None, 8, One
    $port.ReadTimeout = 2000
    $port.Open()
    Write-Host "[OK] Port $PortName berhasil dibuka. Menunggu data..." -ForegroundColor Green
    
    while ($port.IsOpen) {
        try {
            $line = $port.ReadLine().Trim()
            
            # Tampilkan semua baris di layar (opsional, agar terlihat seperti Serial Monitor biasa)
            Write-Host $line
            
            # Ekstrak HANYA string JSON dari baris log [MQTT←]
            # Contoh baris: [MQTT←] wwtp/plant1/ph : {"plant_id":"plant1",...}
            if ($line -match '\[MQTT←\].*? : (\{.*\})') {
                $jsonData = $matches[1]
                Add-Content -Path $LogFile -Value $jsonData
            }
        }
        catch [TimeoutException] {
            # Timeout biasa karena tidak ada data masuk, biarkan loop lanjut
        }
    }
}
catch {
    Write-Host "[ERROR] Gagal membuka port $PortName. Pastikan Serial Monitor di Arduino IDE DITUTUP sebelum menjalankan script ini!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
}
finally {
    if ($port -ne $null -and $port.IsOpen) {
        $port.Close()
        Write-Host "[OK] Port ditutup." -ForegroundColor Yellow
    }
}
