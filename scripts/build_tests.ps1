param(
  [string]$BuildDir = "",
  [int]$PerTestTimeoutSec = 20,
  [int]$DiscoveryTimeoutSec = 10,
  [string]$Filter = "",
  [switch]$ListOnly,
  [switch]$StopOnFirstFailure,
  [switch]$Configure,
  [string]$Generator = "",
  [string]$LlvmDir = "C:\msys64\mingw64\lib\cmake\llvm",
  [string]$ResultsDir = ""
)

$runner = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "build_tests_v2.ps1"
& $runner @PSBoundParameters
exit $LASTEXITCODE
