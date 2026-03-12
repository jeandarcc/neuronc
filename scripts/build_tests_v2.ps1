param(
  [string]$BuildDir = "",
  [int]$PerTestTimeoutSec = 20,
  [int]$DiscoveryTimeoutSec = 10,
  [string]$Filter = "",
  [switch]$ListOnly,
  [switch]$StopOnFirstFailure,
  [switch]$Configure,
  [switch]$CleanConfigure,
  [string]$Generator = "",
  [string]$LlvmDir = "C:\msys64\mingw64\lib\cmake\llvm",
  [string]$ResultsDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
  $PSNativeCommandUseErrorActionPreference = $false
}

function Get-WorkspaceStateRoot {
  param([string]$RepoPath)

  $baseRoot = $env:LOCALAPPDATA
  if ([string]::IsNullOrWhiteSpace($baseRoot)) {
    $baseRoot = $env:TEMP
  }
  if ([string]::IsNullOrWhiteSpace($baseRoot)) {
    throw "LOCALAPPDATA and TEMP are both unavailable."
  }

  $repoName = Split-Path -Path $RepoPath -Leaf
  return [System.IO.Path]::GetFullPath((Join-Path $baseRoot ("NeuronPP\workspaces\" + $repoName)))
}

function Resolve-PathFromRepo {
  param(
    [string]$PathValue,
    [string]$RepoPath
  )

  if ([System.IO.Path]::IsPathRooted($PathValue)) {
    return [System.IO.Path]::GetFullPath($PathValue)
  }

  return [System.IO.Path]::GetFullPath((Join-Path $RepoPath $PathValue))
}

function Ensure-Directory {
  param([string]$Path)
  if (-not (Test-Path $Path)) {
    New-Item -ItemType Directory -Path $Path | Out-Null
  }
}

function Get-SafeFileName {
  param([string]$Name)
  $safe = $Name -replace '[^A-Za-z0-9._-]', '_'
  if ([string]::IsNullOrWhiteSpace($safe)) { return 'unnamed' }
  return $safe
}

function Join-QuotedArguments {
  param([string[]]$Arguments)
  return (($Arguments | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' }) -join ' ')
}

function Invoke-ProcessWithTimeout {
  param(
    [string]$FilePath,
    [string[]]$Arguments,
    [int]$TimeoutSec,
    [string]$LogPath
  )

  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $FilePath
  $psi.Arguments = Join-QuotedArguments -Arguments $Arguments
  $psi.UseShellExecute = $false
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.CreateNoWindow = $true

  $process = New-Object System.Diagnostics.Process
  $process.StartInfo = $psi
  $null = $process.Start()

  $stdoutTask = $process.StandardOutput.ReadToEndAsync()
  $stderrTask = $process.StandardError.ReadToEndAsync()

  if (-not $process.WaitForExit($TimeoutSec * 1000)) {
    try { $process.Kill() } catch {}
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    ($stdout + $stderr) | Set-Content -Path $LogPath -Encoding UTF8
    return [pscustomobject]@{ TimedOut = $true; ExitCode = 124; StdOut = $stdout; StdErr = $stderr }
  }

  $stdout = $stdoutTask.GetAwaiter().GetResult()
  $stderr = $stderrTask.GetAwaiter().GetResult()
  ($stdout + $stderr) | Set-Content -Path $LogPath -Encoding UTF8
  return [pscustomobject]@{ TimedOut = $false; ExitCode = $process.ExitCode; StdOut = $stdout; StdErr = $stderr }
}

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$WorkspaceStateRoot = Get-WorkspaceStateRoot -RepoPath $RepoRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
  $BuildDir = Join-Path $WorkspaceStateRoot "build-mingw"
}
$BuildPath = Resolve-PathFromRepo -PathValue $BuildDir -RepoPath $RepoRoot
if ([string]::IsNullOrWhiteSpace($ResultsDir)) {
  $ResultsDir = Join-Path $BuildPath "test-results"
} else {
  $ResultsDir = Resolve-PathFromRepo -PathValue $ResultsDir -RepoPath $RepoRoot
}

$ToolchainBin = if (-not [string]::IsNullOrWhiteSpace($env:NEURON_TOOLCHAIN_BIN)) {
  $env:NEURON_TOOLCHAIN_BIN
} else {
  "C:\msys64\mingw64\bin"
}
if ([string]::IsNullOrWhiteSpace($Generator)) {
  if (Test-Path (Join-Path $ToolchainBin "ninja.exe")) {
    $Generator = "Ninja"
  } else {
    $Generator = "MinGW Makefiles"
  }
}

$TestExe = Join-Path $BuildPath "bin\neuron_tests.exe"
$env:Path = "$ToolchainBin;C:\msys64\usr\bin;" + $env:Path
$env:LLVM_DIR = $LlvmDir

function Get-TestNames {
  param([string]$ExePath)
  $discoveryLog = Join-Path $ResultsDir 'discovery.log'
  $result = Invoke-ProcessWithTimeout -FilePath $ExePath -Arguments @('--list-tests') -TimeoutSec $DiscoveryTimeoutSec -LogPath $discoveryLog
  if ($result.TimedOut) { throw "Test discovery timed out after ${DiscoveryTimeoutSec}s. See $discoveryLog" }
  if ($result.ExitCode -ne 0) { throw "Test discovery failed with exit code $($result.ExitCode). See $discoveryLog" }
  return @($result.StdOut -split "`r?`n" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Get-TestTimeoutSec {
  param([string]$TestName)
  if ($TestName -match 'Graphics|Gpu|Canvas') { return [Math]::Max($PerTestTimeoutSec, 30) }
  return $PerTestTimeoutSec
}

function Write-Summary {
  param([object[]]$Results)
  $summaryPath = Join-Path $ResultsDir 'summary.txt'
  $jsonPath = Join-Path $ResultsDir 'summary.json'
  $lines = New-Object System.Collections.Generic.List[string]
  $lines.Add('Neuron++ test summary')
  $lines.Add("Results directory: $ResultsDir")
  $lines.Add('')
  foreach ($result in $Results) {
    $lines.Add(("{0} :: {1} :: {2}ms :: {3}" -f $result.Status, $result.Name, $result.DurationMs, $result.LogPath))
  }
  $lines | Set-Content -Path $summaryPath -Encoding UTF8
  ($Results | ConvertTo-Json -Depth 5) | Set-Content -Path $jsonPath -Encoding UTF8
}

Push-Location $RepoRoot
try {
  if ($CleanConfigure -and (Test-Path $BuildPath)) {
    Remove-Item -Recurse -Force $BuildPath
  }

  Ensure-Directory $ResultsDir

  if ($Configure -or $CleanConfigure) {
    Write-Host "[CONFIGURE] $BuildPath"
    & cmake -S $RepoRoot -B $BuildPath -G $Generator -DLLVM_DIR=$LlvmDir
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE" }
  }

  Write-Host '[BUILD] neuron_tests'
  & cmake --build $BuildPath --target neuron_tests
  if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
  if (-not (Test-Path $TestExe)) { throw "Test executable not found: $TestExe" }

  $tests = Get-TestNames -ExePath $TestExe
  if (-not [string]::IsNullOrWhiteSpace($Filter)) {
    $tests = @($tests | Where-Object { $_ -like $Filter })
  }
  if ($tests.Count -eq 0) { throw "No tests matched filter '$Filter'." }

  if ($ListOnly) {
    $tests | ForEach-Object { Write-Host $_ }
    exit 0
  }

  $results = New-Object System.Collections.Generic.List[object]
  foreach ($testName in $tests) {
    $timeoutSec = Get-TestTimeoutSec -TestName $testName
    $safeName = Get-SafeFileName $testName
    $logPath = Join-Path $ResultsDir ($safeName + '.log')
    Write-Host "[TEST] $testName (timeout ${timeoutSec}s)"
    $start = Get-Date
    $result = Invoke-ProcessWithTimeout -FilePath $TestExe -Arguments @('--filter', $testName) -TimeoutSec $timeoutSec -LogPath $logPath
    $durationMs = [int]((Get-Date) - $start).TotalMilliseconds

    $status = 'PASS'
    if ($result.TimedOut) { $status = 'TIMEOUT' }
    elseif ($result.ExitCode -ne 0) { $status = 'FAIL' }

    $entry = [pscustomobject]@{
      Name = $testName
      Status = $status
      ExitCode = $result.ExitCode
      DurationMs = $durationMs
      TimeoutSec = $timeoutSec
      LogPath = $logPath
    }
    $results.Add($entry)

    if ($status -eq 'PASS') {
      Write-Host "  -> PASS (${durationMs}ms)"
    } else {
      if ($status -eq 'TIMEOUT') {
        Write-Host "  -> TIMEOUT after ${timeoutSec}s" -ForegroundColor Red
      } else {
        Write-Host "  -> FAIL (exit $($result.ExitCode), ${durationMs}ms)" -ForegroundColor Red
      }
      $tail = Get-Content -Path $logPath -Tail 20 -ErrorAction SilentlyContinue
      if ($tail) {
        Write-Host '  Last output:'
        $tail | ForEach-Object { Write-Host "    $_" }
      }
      if ($StopOnFirstFailure) { break }
    }
  }

  Write-Summary -Results $results
  $passed = @($results | Where-Object { $_.Status -eq 'PASS' }).Count
  $failed = @($results | Where-Object { $_.Status -eq 'FAIL' }).Count
  $timedOut = @($results | Where-Object { $_.Status -eq 'TIMEOUT' }).Count

  Write-Host ''
  Write-Host "Summary: $passed pass, $failed fail, $timedOut timeout, $($results.Count) total"
  Write-Host "Summary file: $(Join-Path $ResultsDir 'summary.txt')"

  if ($failed -gt 0 -or $timedOut -gt 0) { exit 1 }
  exit 0
}
finally {
  Pop-Location
}
