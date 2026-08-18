# sign_exe.ps1
# สคริปต์นี้ทำ 3 อย่าง:
#   1) สร้าง self-signed code-signing certificate
#   2) ติดตั้ง cert นั้นเข้า "Trusted Root" และ "Trusted Publishers" ของเครื่องตัวเอง
#      (ถ้าจะใช้หลายเครื่องในองค์กร ต้อง export cert แล้ว deploy ผ่าน GPO ไปยังเครื่องอื่นด้วย)
#   3) เซ็น MachineInfoGUI.exe ด้วย cert ที่สร้าง
#
# วิธีรัน: เปิด PowerShell แบบ "Run as Administrator" แล้วรัน
#   Set-ExecutionPolicy -Scope Process Bypass -Force
#   .\sign_exe.ps1

$ErrorActionPreference = "Stop"

$certSubject = "CN=LBS"
$exePath = ".\MachineInfoGUI.exe"

Write-Host "1) กำลังสร้าง self-signed certificate..." -ForegroundColor Cyan
$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject $certSubject `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(5)

Write-Host "   สร้างสำเร็จ: Thumbprint = $($cert.Thumbprint)" -ForegroundColor Green

Write-Host "2) กำลังติดตั้ง cert เข้า Trusted Root และ Trusted Publishers..." -ForegroundColor Cyan
$rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "CurrentUser")
$rootStore.Open("ReadWrite")
$rootStore.Add($cert)
$rootStore.Close()

$pubStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("TrustedPublisher", "CurrentUser")
$pubStore.Open("ReadWrite")
$pubStore.Add($cert)
$pubStore.Close()

Write-Host "   ติดตั้งสำเร็จ" -ForegroundColor Green

Write-Host "3) กำลังเซ็นไฟล์ $exePath ..." -ForegroundColor Cyan
if (-Not (Test-Path $exePath)) {
    Write-Host "   ไม่พบไฟล์ $exePath ในโฟลเดอร์นี้ กรุณาย้ายสคริปต์ไปไว้โฟลเดอร์เดียวกับ .exe" -ForegroundColor Red
    exit 1
}

# หา signtool.exe จาก Windows SDK (ปกติติดมากับ Visual Studio หรือ Windows SDK)
$signtool = Get-ChildItem -Path "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "x64" } | Select-Object -First 1 -ExpandProperty FullName

if (-not $signtool) {
    Write-Host "   ไม่พบ signtool.exe — ต้องติดตั้ง Windows SDK ก่อน (ดาวน์โหลดฟรีจาก Microsoft)" -ForegroundColor Yellow
    Write-Host "   ลิงก์: https://developer.microsoft.com/windows/downloads/windows-sdk/" -ForegroundColor Yellow
    exit 1
}

& $signtool sign /sha1 $cert.Thumbprint /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 $exePath

Write-Host "เซ็นไฟล์สำเร็จแล้ว!" -ForegroundColor Green
Write-Host "หมายเหตุ: cert นี้ใช้ได้เฉพาะเครื่องที่รันสคริปต์นี้ (self-signed)" -ForegroundColor Yellow
Write-Host "ถ้าจะแจกไปเครื่องอื่นในองค์กร ต้อง export cert (.cer) แล้ว deploy ผ่าน Group Policy ไปยังเครื่องอื่นด้วย" -ForegroundColor Yellow
Write-Host "คำสั่ง export: Export-Certificate -Cert $($cert.PSPath) -FilePath sshitsupport.cer" -ForegroundColor Yellow
