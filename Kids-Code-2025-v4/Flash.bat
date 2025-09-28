@echo off
setlocal enabledelayedexpansion

REM =======================
REM Config — edit as needed
REM =======================
set "COM_PORT=COM12"
set "BAUD=19200"
set "MCU=attiny85"
set "PROGRAMMER=stk500v1"
set "FIRMWARE=2025-Code-KidsBadge.ino.hex"

REM =======================
REM Paths (relative to this .bat)
REM =======================
set "ROOT=%~dp0"
set "AVRDUDE=%ROOT%avrdude\bin\avrdude.exe"
set "AVRCONF=%ROOT%avrdude\etc\avrdude.conf"

if not exist "%AVRDUDE%" (
  echo [ERROR] avrdude not found: "%AVRDUDE%"
  goto :end
)
if not exist "%AVRCONF%" (
  echo [ERROR] avrdude.conf not found: "%AVRCONF%"
  goto :end
)
if not exist "%ROOT%%FIRMWARE%" (
  echo [ERROR] Firmware not found: "%ROOT%%FIRMWARE%"
  goto :end
)

set /a FLASH_COUNT=0
echo Ready on %COM_PORT% @ %BAUD%. Connect device to program.

:loop
echo(
echo === Programming Cycle Start ===

echo [1/2] Burning bootloader (fuses)...
"%AVRDUDE%" -C "%AVRCONF%" -v -v -v -v -p%MCU% -c%PROGRAMMER% -P%COM_PORT% -b%BAUD% -e ^
  -U efuse:w:0xff:m -U hfuse:w:0xdf:m -U lfuse:w:0xe2:m
if errorlevel 1 (
  echo [FAIL] Bootloader/fuse step failed.
  set /p "x=Press ENTER to retry (Ctrl+C to exit)..."
  goto :loop
)

REM 2s pause before flashing
timeout /t 2 /nobreak >nul

echo [2/2] Flashing firmware: %FIRMWARE%
"%AVRDUDE%" -C "%AVRCONF%" -v -V -p%MCU% -c%PROGRAMMER% -P%COM_PORT% -b%BAUD% ^
  -U flash:w:"%ROOT%%FIRMWARE%":i
if errorlevel 1 (
  echo [FAIL] Flash step failed.
  set /p "x=Press ENTER to retry (Ctrl+C to exit)..."
  goto :loop
)

set /a FLASH_COUNT+=1
echo [OK] Device programmed successfully.
echo Total Flashes: !FLASH_COUNT!

set /p "x=Press ENTER to program the next device (Ctrl+C to exit)..."
goto :loop

:end
echo Done.
endlocal
