@echo off
setlocal enabledelayedexpansion
echo ============================================
echo  Paint 3D Installer
echo ============================================
echo.
echo Step 1: Downloading required dependency (Microsoft.UI.Xaml 2.0)...
echo This may take a moment...
echo.

set "DEP_URL=https://www.nuget.org/api/v2/package/Microsoft.UI.Xaml/2.0.181018003"
set "DEP_ZIP=%TEMP%\microsoft.ui.xaml.2.0.zip"
set "DEP_DIR=%TEMP%\microsoft.ui.xaml.2.0"
set "DEP_APPX=%DEP_DIR%\tools\AppX\x64\Release\Microsoft.UI.Xaml.2.0.appx"

powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri '%DEP_URL%' -OutFile '%DEP_ZIP%' -UseBasicParsing"

if not exist "%DEP_ZIP%" (
    echo ERROR: Failed to download the dependency. Check your internet connection.
    pause
    exit /b 1
)

echo Extracting dependency...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -Path '%DEP_ZIP%' -DestinationPath '%DEP_DIR%' -Force"

if not exist "%DEP_APPX%" (
    echo ERROR: Could not find appx inside downloaded package.
    echo Expected: %DEP_APPX%
    pause
    exit /b 1
)

echo Installing Microsoft.UI.Xaml 2.0 dependency...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path '%DEP_APPX%'"

if %errorlevel% neq 0 (
    echo WARNING: Dependency install returned an error - it may already be installed. Continuing...
)

echo.
echo Step 2: Installing Paint 3D...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-AppxPackage -Path '%~dp0Paint3D_x64.appx'"

if %errorlevel% == 0 (
    echo.
    echo ============================================
    echo  SUCCESS! Paint 3D is installed.
    echo  Open it from the Start Menu.
    echo ============================================
) else (
    echo.
    echo Paint 3D install failed. Error code: %errorlevel%
    echo Make sure Paint3D_x64.appx is in the same folder as this .bat file.
)

echo.
pause
