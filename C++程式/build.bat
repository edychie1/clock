@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

set SRC=%~dp0
set SRC=%SRC:~0,-1%
set TMP=%TEMP%\aiclock_build.exe
set OUT=%SRC%\執行檔(C++)\AIClock.exe

subst Z: "%SRC%"
if errorlevel 1 (
    echo subst failed!
    pause
    exit /b 1
)

echo Compiling with Zig...
zig c++ -std=c++20 -O2 "-Xlinker" "/subsystem:windows" Z:\main.cpp Z:\config.cpp Z:\clock.rc -o "%TMP%" -lgdi32 -lgdiplus -lshell32 -lcomctl32 -lcomdlg32 -ladvapi32 -lole32 -luuid

set RESULT=%ERRORLEVEL%
subst Z: /d

if %RESULT% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

copy /Y "%TMP%" "%OUT%" >nul
del "%TMP%" 2>nul

echo OK: %OUT%
pause
