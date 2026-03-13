@echo off
setlocal enabledelayedexpansion

:: dev-neuron.bat â€” Shortcut to the locally built neuron.exe
:: Usage: dev-neuron.bat [args...]
:: Example: dev-neuron.bat help
::          dev-neuron.bat compile foo.nr

set "BIN_DIR=%LOCALAPPDATA%\Neuron\workspaces\Neuron\build-mingw\bin"
set "NEURON_EXE=!BIN_DIR!\neuron.exe"

if not exist "!NEURON_EXE!" (
    echo Error: neuron.exe not found at "!NEURON_EXE!"
    echo Please build the project first.
    exit /b 1
)

"!NEURON_EXE!" %*
exit /b %ERRORLEVEL%
