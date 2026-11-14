@echo off
rem Run from "x64 Native Tools Command Prompt for VS 2022".
if not exist out mkdir out
cl /nologo /O2 /MT /W4 /EHsc /LD /DUNICODE /D_UNICODE src\dllmain.cpp ^
   /Fe:out\xinput1_3.dll /Fo:out\ /link /DEF:src\exports.def /MACHINE:X64 ole32.lib user32.lib
if errorlevel 1 exit /b 1
echo.
echo Built out\xinput1_3.dll
