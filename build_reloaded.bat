@echo off
rem Run from "x64 Native Tools Command Prompt for VS 2022".
rem Produces a Reloaded-II mod folder: out\reloaded\heerelli.smtvv.dualsense.haptic\
set MODDIR=out\reloaded\heerelli.smtvv.dualsense.haptic
if not exist %MODDIR% mkdir %MODDIR%
if not exist out\obj_reloaded mkdir out\obj_reloaded
cl /nologo /O2 /MT /W4 /EHsc /LD /DUNICODE /D_UNICODE /DSMT5V_RELOADED src\dllmain.cpp ^
   /Fe:%MODDIR%\SMTVVDualSenseHaptic.dll /Fo:out\obj_reloaded\ /link /MACHINE:X64 ole32.lib user32.lib
if errorlevel 1 exit /b 1
copy /y reloaded\ModConfig.json %MODDIR%\ModConfig.json >nul
del /q %MODDIR%\*.lib %MODDIR%\*.exp 2>nul
echo.
echo Built %MODDIR%
echo Copy that folder into Reloaded-II\Mods\ (or zip it and drag it onto Reloaded-II).
