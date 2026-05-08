# Paint 3D Installer (Sideload)
# Run this script as Administrator in PowerShell

Write-Host "Installing Paint 3D (v6.2305.16087.0)..." -ForegroundColor Cyan

$appxPath = Join-Path $PSScriptRoot "Paint3D_x64.appx"

if (-Not (Test-Path $appxPath)) {
    Write-Host "ERROR: Paint3D_x64.appx not found next to this script." -ForegroundColor Red
    Write-Host "Make sure both files are in the same folder." -ForegroundColor Yellow
    pause
    exit 1
}

try {
    Add-AppxPackage -Path $appxPath
    Write-Host ""
    Write-Host "Paint 3D installed successfully!" -ForegroundColor Green
    Write-Host "You can find it in the Start Menu." -ForegroundColor Green
} catch {
    Write-Host ""
    Write-Host "Installation failed: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "If you get a signature error, run this first to allow sideloading:" -ForegroundColor Yellow
    Write-Host '  Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser' -ForegroundColor White
    Write-Host "Then try running this script again." -ForegroundColor Yellow
}

pause
