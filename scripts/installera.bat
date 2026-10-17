@echo off
rem Installerar R-744 på Windows. Dubbelklicka på filen.
rem Plugin-mappen för VST3 kräver administratör, så filen ber om det själv.

net session >nul 2>&1
if errorlevel 1 (
  powershell -NoProfile -Command "Start-Process -Verb RunAs -FilePath '%~f0'"
  exit /b
)

cd /d "%~dp0"
set "DEST=%CommonProgramFiles%\VST3"
if not exist "%DEST%" mkdir "%DEST%"
if exist "%DEST%\R-744.vst3" rmdir /s /q "%DEST%\R-744.vst3"
xcopy "R-744.vst3" "%DEST%\R-744.vst3\" /e /i /y /q >nul

if errorlevel 1 (
  echo.
  echo Nagot gick fel. Packade du upp zip-filen forst? Hogerklicka pa zip-filen och valj "Extrahera alla".
  pause
  exit /b 1
)

echo.
echo Klart! R-744 ligger nu i:
echo   %DEST%
echo Starta om Bitwig, eller tryck Rescan under Settings ^> Locations ^> Plug-ins.
echo.
pause
