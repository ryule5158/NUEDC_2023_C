@echo off
rem Double-click entry: build the bitstream and package the QSPI image.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-QspiBoot.ps1"
if errorlevel 1 (
  echo.
  echo Build failed. Review the message above.
) else (
  echo.
  echo Build and BOOT image generation completed successfully.
)
pause
