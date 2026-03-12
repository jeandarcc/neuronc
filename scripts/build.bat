@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
for %%I in ("%ROOT%") do set "REPO_NAME=%%~nxI"

set "STATE_ROOT=%LOCALAPPDATA%\NeuronPP\workspaces\%REPO_NAME%"
if "%LOCALAPPDATA%"=="" (
  set "STATE_ROOT=%TEMP%\NeuronPP\workspaces\%REPO_NAME%"
)

set "BUILD_DIR=%STATE_ROOT%\build-mingw"
set "LOG_DIR=%STATE_ROOT%\logs"
set "LOG_FILE=%LOG_DIR%\build_out.txt"
set "ERR_FILE=%LOG_DIR%\build_error.txt"
set "TEST_SCRIPT=%ROOT%\scripts\build_tests.ps1"

set "TOOLCHAIN_BIN="
if not "%NEURON_TOOLCHAIN_BIN%"=="" (
  set "TOOLCHAIN_BIN=%NEURON_TOOLCHAIN_BIN%"
) else if exist "%ROOT%\toolchain\bin\c++.exe" (
  set "TOOLCHAIN_BIN=%ROOT%\toolchain\bin"
) else (
  set "TOOLCHAIN_BIN=C:\msys64\mingw64\bin"
)

set "MSYS_BIN=C:\msys64\usr\bin"
if exist "%ROOT%\toolchain\bin\bash.exe" (
  set "MSYS_BIN=%ROOT%\toolchain\bin"
)

set "GENERATOR=MinGW Makefiles"
if exist "%TOOLCHAIN_BIN%\ninja.exe" (
  set "GENERATOR=Ninja"
)

if not exist "%LOG_DIR%" (
  mkdir "%LOG_DIR%" >nul 2>&1
)

if not exist "%TOOLCHAIN_BIN%\c++.exe" (
  > "%ERR_FILE%" (
    echo [toolchain] c++.exe not found: "%TOOLCHAIN_BIN%\c++.exe"
    echo [toolchain] Set NEURON_TOOLCHAIN_BIN to your MinGW bin folder.
  )
  type "%ERR_FILE%"
  exit /b 1
)

set "PATH=%TOOLCHAIN_BIN%;%MSYS_BIN%;%PATH%"

set "PROBE_CPP=%TEMP%\npp_toolchain_probe_%RANDOM%.cpp"
> "%PROBE_CPP%" echo int main^(^) { return 0; }
"%TOOLCHAIN_BIN%\c++.exe" -x c++ -E "%PROBE_CPP%" -o nul >nul 2>&1
set "PROBE_EXIT=!ERRORLEVEL!"
del /Q "%PROBE_CPP%" >nul 2>&1

if not "!PROBE_EXIT!"=="0" (
  > "%ERR_FILE%" (
    echo [toolchain] failed to start gcc frontend ^(cc1plus^).
    echo [toolchain] If you only see "mingw32-make ... Error 1", this is a common cause.
    echo [toolchain] TOOLCHAIN_BIN=!TOOLCHAIN_BIN!
    echo [toolchain] PATH=!PATH!
  )
  type "%ERR_FILE%"
  exit /b 1
)

if not exist "%BUILD_DIR%\CMakeCache.txt" (
  cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "!GENERATOR!" > "%LOG_FILE%" 2>&1
  if errorlevel 1 (
    copy /Y "%LOG_FILE%" "%ERR_FILE%" >nul
    type "%ERR_FILE%"
    exit /b 1
  )
)

set "JOBS=%NUMBER_OF_PROCESSORS%"
if "%JOBS%"=="" (
  set "JOBS=8"
)

echo [build] cmake --build "%BUILD_DIR%" --config Release --verbose --parallel %JOBS%
cmake --build "%BUILD_DIR%" --config Release --verbose --parallel %JOBS% > "%LOG_FILE%" 2>&1
set "BUILD_EXIT=!ERRORLEVEL!"

if not "!BUILD_EXIT!"=="0" (
  copy /Y "%LOG_FILE%" "%ERR_FILE%" >nul
  echo [build] FAILED. Last 120 lines from "%ERR_FILE%":
  powershell -NoProfile -Command "Get-Content -Path '%ERR_FILE%' -Tail 120"
  echo.
  echo [build] Highlighted diagnostics:
  findstr /N /R /C:": error:" /C:"fatal error:" /C:"undefined reference" "%ERR_FILE%"
  if errorlevel 1 (
    echo [build] No compiler diagnostic lines matched. Full log is in "%ERR_FILE%".
  )
  exit /b !BUILD_EXIT!
)

echo [build] OK. Full log: "%LOG_FILE%"
echo [test] Running full test suite...
powershell -NoProfile -ExecutionPolicy Bypass -File "%TEST_SCRIPT%" -BuildDir "%BUILD_DIR%" -Generator "!GENERATOR!"
set "TEST_EXIT=!ERRORLEVEL!"
if not "!TEST_EXIT!"=="0" (
  echo [test] FAILED with exit code !TEST_EXIT!.
  exit /b !TEST_EXIT!
)

echo [test] OK.
echo [build] Build + test pipeline OK.
exit /b 0
