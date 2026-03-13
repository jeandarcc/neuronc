@echo off
setlocal enabledelayedexpansion

:: Neuron Repository Run/Test Shortcut Script
:: This script compiles an .nr file, runs it, and then cleans up the exe/obj files.
:: It does not require neuron.toml and does not affect the project state.

set "TARGET_FILE=%~1"

if "!TARGET_FILE!"=="" (
    echo Usage: run.bat [file.nr]
    exit /b 1
)

if not exist "!TARGET_FILE!" (
    echo Error: File not found: !TARGET_FILE!
    exit /b 1
)

set "BIN_DIR=%LOCALAPPDATA%\Neuron\workspaces\Neuron\build-mingw\bin"
set "NEURON_EXE=!BIN_DIR!\neuron.exe"

if not exist "!NEURON_EXE!" (
    echo Error: neuron.exe not found. Please build the project.
    exit /b 1
)

:: Extract filename and source directory
for %%i in ("!TARGET_FILE!") do (
    set "FILE_STEM=%%~ni"
    set "SOURCE_DIR=%%~dpi"
)

:: Compile (Uses --bypass-rules by default)
echo [Neuron] Compiling: !TARGET_FILE!
"!NEURON_EXE!" compile "!TARGET_FILE!" --bypass-rules

if %ERRORLEVEL% NEQ 0 (
    echo [Neuron] Compilation failed.
    exit /b %ERRORLEVEL%
)

:: Generated binary names (some compiler paths emit near source, some in CWD)
set "EXE_OUTPUT_CWD=!FILE_STEM!.exe"
set "OBJ_OUTPUT_CWD=!FILE_STEM!.obj"
set "LL_OUTPUT_CWD=!FILE_STEM!.ll"
set "EXE_OUTPUT_SRC=!SOURCE_DIR!!FILE_STEM!.exe"
set "OBJ_OUTPUT_SRC=!SOURCE_DIR!!FILE_STEM!.obj"
set "LL_OUTPUT_SRC=!SOURCE_DIR!!FILE_STEM!.ll"
set "EXE_OUTPUT="

if exist "!EXE_OUTPUT_SRC!" set "EXE_OUTPUT=!EXE_OUTPUT_SRC!"
if "!EXE_OUTPUT!"=="" if exist "!EXE_OUTPUT_CWD!" set "EXE_OUTPUT=!EXE_OUTPUT_CWD!"

if "!EXE_OUTPUT!"=="" (
    echo [Neuron] Error: Executable was not produced.
    echo [Neuron] Checked: "!EXE_OUTPUT_SRC!" and "!EXE_OUTPUT_CWD!"
    exit /b 1
)

echo [Neuron] Running: !EXE_OUTPUT!
echo --------------------------------------------------
"!EXE_OUTPUT!"
set "RUN_RET=%ERRORLEVEL%"
echo --------------------------------------------------

:: Cleanup (Delete temporary files only)
for %%f in ("!EXE_OUTPUT_SRC!" "!OBJ_OUTPUT_SRC!" "!LL_OUTPUT_SRC!" "!EXE_OUTPUT_CWD!" "!OBJ_OUTPUT_CWD!" "!LL_OUTPUT_CWD!") do (
    if exist "%%~f" del "%%~f"
)

exit /b !RUN_RET!
