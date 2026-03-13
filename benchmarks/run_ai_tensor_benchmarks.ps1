param(
  [int]$Trials = 3,
  [string[]]$ThreadCounts = @(),
  [int]$TensorSize = 1024,
  [int]$TfInterOpThreads = 1,
  [string]$TensorFlowPython = "",
  [switch]$DisableTensorFlow
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
  $PSNativeCommandUseErrorActionPreference = $false
}

function Get-Median {
  param([double[]]$Values)
  $sorted = @($Values | Sort-Object)
  $count = $sorted.Length
  if ($count -eq 0) {
    throw "Median requested for empty array."
  }
  if (($count % 2) -eq 1) {
    return [double]$sorted[[int]($count / 2)]
  }
  $left = [double]$sorted[($count / 2) - 1]
  $right = [double]$sorted[$count / 2]
  return ($left + $right) / 2.0
}

function Parse-PureMetrics {
  param(
    [object[]]$OutputLines,
    [string]$Prefix
  )

  $metrics = @{}
  for ($i = 0; $i -lt $OutputLines.Count - 1; $i++) {
    $line = [string]$OutputLines[$i]
    $next = [string]$OutputLines[$i + 1]
    if ($line.Trim() -eq "${Prefix}_FMA_MS") {
      $metrics["fma"] = [double]$next.Trim()
    }
    if ($line.Trim() -eq "${Prefix}_MATMUL_MS") {
      $metrics["matmul"] = [double]$next.Trim()
    }
  }

  if (-not $metrics.ContainsKey("fma") -or -not $metrics.ContainsKey("matmul")) {
    throw "Could not parse pure metrics for prefix '${Prefix}'. Output: $($OutputLines -join ' | ')"
  }
  return $metrics
}

function Parse-SingleMetric {
  param(
    [object[]]$OutputLines,
    [string]$MetricLabel
  )

  for ($i = 0; $i -lt $OutputLines.Count - 1; $i++) {
    $line = [string]$OutputLines[$i]
    $next = [string]$OutputLines[$i + 1]
    if ($line.Trim() -eq $MetricLabel) {
      return [double]$next.Trim()
    }
  }
  throw "Could not parse metric '$MetricLabel'. Output: $($OutputLines -join ' | ')"
}

function Parse-StructuredMetrics {
  param(
    [object[]]$OutputLines,
    [string]$Prefix
  )

  $labelToKey = @{
    "${Prefix}_CIRC_MATMUL_MS" = "circ_matmul"
    "${Prefix}_TOEP_MATMUL_MS" = "toep_matmul"
    "${Prefix}_HYBRID_MATMUL_MS" = "hybrid_matmul"
    "${Prefix}_CIRC_FUSED_MS" = "circ_fused"
    "${Prefix}_TOEP_FUSED_MS" = "toep_fused"
    "${Prefix}_HYBRID_FUSED_MS" = "hybrid_fused"
  }

  $metrics = @{}
  for ($i = 0; $i -lt $OutputLines.Count - 1; $i++) {
    $line = [string]$OutputLines[$i]
    $next = [string]$OutputLines[$i + 1]
    if ($labelToKey.ContainsKey($line.Trim())) {
      $metrics[$labelToKey[$line.Trim()]] = [double]$next.Trim()
    }
  }

  foreach ($key in $labelToKey.Values) {
    if (-not $metrics.ContainsKey($key)) {
      throw "Could not parse structured metric '$key' for prefix '${Prefix}'. Output: $($OutputLines -join ' | ')"
    }
  }
  return $metrics
}

function Compile-CppBenchmark {
  param(
    [string]$Name,
    [string]$Source,
    [string]$Output,
    [string[]]$ExtraArgs = @()
  )

  Write-Host "Compiling $Name ..."
  & g++ -O3 -ffast-math -march=native -std=c++17 $Source @ExtraArgs -o $Output
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $Output)) {
    throw "Failed to compile $Name benchmark."
  }
}

function Try-CompileBlasBenchmark {
  param(
    [string]$Source,
    [string]$Output
  )

  $candidateLibs = @(
    @("-lopenblas"),
    @("-lblas")
  )

  $cmdExe = Join-Path $env:WINDIR "System32\cmd.exe"
  foreach ($libArgs in $candidateLibs) {
    $libFlags = $libArgs -join " "
    $quotedSource = '"' + $Source + '"'
    $quotedOutput = '"' + $Output + '"'
    $cmd = "g++ -O3 -ffast-math -march=native -std=c++17 $quotedSource $libFlags -o $quotedOutput >nul 2>nul"
    & $cmdExe /c $cmd | Out-Null
    if ($LASTEXITCODE -eq 0 -and (Test-Path $Output)) {
      return ($libArgs -join " ")
    }
  }

  return $null
}

function Compile-NppBenchmark {
  param(
    [string]$NeuronExe,
    [string]$RepoRoot,
    [string]$SourcePath,
    [string]$ExpectedExe
  )
  Push-Location $RepoRoot
  & $NeuronExe compile $SourcePath | Out-Host
  Pop-Location
  if (-not (Test-Path $ExpectedExe)) {
    throw "Compiled Neuron benchmark executable not found at $ExpectedExe"
  }
}

function Format-Speedup {
  param(
    [double]$Base,
    [double]$Npp
  )
  if ($Npp -le 0.0) {
    return "n/a"
  }
  return ("{0:N2}x" -f ($Base / $Npp))
}

function Resolve-PythonCommand {
  $python = Get-Command python -ErrorAction SilentlyContinue
  if ($null -ne $python) {
    return @{
      Exe = $python.Source
      BaseArgs = @()
    }
  }

  $py = Get-Command py -ErrorAction SilentlyContinue
  if ($null -ne $py) {
    return @{
      Exe = $py.Source
      BaseArgs = @("-3")
    }
  }

  return $null
}

function Test-TensorFlowAvailable {
  param(
    [string]$PythonExe,
    [object[]]$PythonBaseArgs
  )

  $checkArgs = @($PythonBaseArgs + @("-c", "import tensorflow as tf; print(tf.__version__)"))
  $result = Invoke-ExternalCommand -Exe $PythonExe -CommandArgs $checkArgs
  if ($result.ExitCode -eq 0) {
    $version = ([string[]]$result.Combined | Where-Object { $_ -and $_.Trim().Length -gt 0 } | Select-Object -Last 1).Trim()
    if (-not [string]::IsNullOrWhiteSpace($version)) {
      Write-Host "TensorFlow benchmark enabled (version $version)."
    } else {
      Write-Host "TensorFlow benchmark enabled."
    }
    return $true
  }

  Write-Host "TensorFlow not available. TensorFlow tier skipped."
  return $false
}

function Set-BenchmarkThreadEnv {
  param(
    [int]$Threads,
    [int]$InterOpThreads
  )

  $threadStr = [string]$Threads
  $env:NEURON_TENSOR_THREADS = $threadStr
  $env:OPENBLAS_NUM_THREADS = $threadStr
  $env:OMP_NUM_THREADS = $threadStr
  $env:MKL_NUM_THREADS = $threadStr
  $env:BLIS_NUM_THREADS = $threadStr
  $env:TF_NUM_INTRAOP_THREADS = $threadStr
  $env:TF_NUM_INTEROP_THREADS = [string]$InterOpThreads
  $env:TF_CPP_MIN_LOG_LEVEL = "2"
}

function Normalize-ThreadCounts {
  param(
    [string[]]$InputThreadCounts
  )

  if ($InputThreadCounts.Count -eq 0) {
    $maxThreads = [Math]::Max([Environment]::ProcessorCount, 1)
    $defaultSet = @(1, 2, 4, 8, $maxThreads)
    return @($defaultSet | Where-Object { $_ -gt 0 -and $_ -le $maxThreads } | Sort-Object -Unique)
  }

  $parsedValues = New-Object System.Collections.Generic.List[int]
  foreach ($entry in $InputThreadCounts) {
    if ($null -eq $entry) { continue }
    $parts = ([string]$entry).Split(@(",", ";", " "), [System.StringSplitOptions]::RemoveEmptyEntries)
    foreach ($part in $parts) {
      $value = 0
      if (-not [int]::TryParse($part, [ref]$value)) {
        throw "Invalid thread count value '$part'. Use positive integers."
      }
      if ($value -le 0) {
        throw "Thread count must be positive. Got '$value'."
      }
      $parsedValues.Add($value)
    }
  }

  $normalized = @($parsedValues | Sort-Object -Unique)
  if ($normalized.Count -eq 0) {
    throw "ThreadCounts must include at least one positive integer."
  }
  return $normalized
}

function Invoke-Profile {
  param([hashtable]$Profile)

  $profileArgs = @()
  if ($Profile.ContainsKey("Args") -and $null -ne $Profile["Args"]) {
    $profileArgs = @($Profile["Args"])
  }

  $result = Invoke-ExternalCommand -Exe ([string]$Profile["Exe"]) -CommandArgs $profileArgs
  if ($result.ExitCode -ne 0) {
    throw "Benchmark profile '$($Profile["Name"])' failed (exit code $($result.ExitCode)). Output: $($result.Combined -join ' | ')"
  }
  return $result.Combined
}

function Invoke-ExternalCommand {
  param(
    [string]$Exe,
    [object[]]$CommandArgs
  )

  $savedErrorActionPreference = $ErrorActionPreference
  try {
    $ErrorActionPreference = "Continue"
    $argumentList = @()
    if ($null -ne $CommandArgs) {
      $argumentList = @($CommandArgs | Where-Object { $null -ne $_ } | ForEach-Object { [string]$_ })
    }
    $output = & $Exe @argumentList 2>&1
    $combined = @()
    foreach ($entry in @($output)) {
      if ($entry -is [System.Management.Automation.ErrorRecord]) {
        $combined += [string]$entry.ToString()
      } else {
        $combined += [string]$entry
      }
    }
    return @{
      ExitCode = [int]$LASTEXITCODE
      StdOut = $combined
      StdErr = @()
      Combined = $combined
    }
  } catch {
    return @{
      ExitCode = 1
      StdOut = @()
      StdErr = @([string]$_.ToString())
      Combined = @([string]$_.ToString())
    }
  } finally {
    $ErrorActionPreference = $savedErrorActionPreference
  }
}

function Get-NullablePureMetric {
  param(
    [hashtable]$MedianMap,
    [string]$Prefix,
    [string]$Metric
  )

  if ($MedianMap.ContainsKey($Prefix)) {
    return [double]$MedianMap[$Prefix][$Metric]
  }
  return $null
}

function Get-NullableFusedMetric {
  param(
    [hashtable]$MedianMap,
    [string]$Prefix
  )

  if ($MedianMap.ContainsKey($Prefix)) {
    return [double]$MedianMap[$Prefix]
  }
  return $null
}

function Get-NullableStructuredMetric {
  param(
    [hashtable]$MedianMap,
    [string]$Prefix,
    [string]$Metric
  )

  if ($MedianMap.ContainsKey($Prefix)) {
    return [double]$MedianMap[$Prefix][$Metric]
  }
  return $null
}

function Format-Nullable {
  param($Value)
  if ($null -eq $Value) {
    return "n/a"
  }
  return ("{0:N2}" -f [double]$Value)
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$neuronExe = Join-Path $buildDir "bin/neuron.exe"
$tfBenchScript = Join-Path $repoRoot "benchmarks/python/tf_tensor_bench.py"

$mingwBin = "C:\msys64\mingw64\bin"
$msysUsrBin = "C:\msys64\usr\bin"
if (Test-Path $mingwBin) {
  if (Test-Path $msysUsrBin) {
    $env:PATH = "$mingwBin;$msysUsrBin;$env:PATH"
  } else {
    $env:PATH = "$mingwBin;$env:PATH"
  }
}

if (-not (Test-Path $buildDir)) {
  cmake -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release
}
cmake --build $buildDir --target neuron --config Release
if (-not (Test-Path $neuronExe)) {
  throw "neuron executable not found at $neuronExe"
}

$pureNppSource = Join-Path $repoRoot "benchmarks/AiTensorBench.nr"
$pureNppExe = Join-Path $repoRoot "benchmarks/AiTensorBench.exe"
$fusedNppSource = Join-Path $repoRoot "benchmarks/AiFusedBench.nr"
$fusedNppExe = Join-Path $repoRoot "benchmarks/AiFusedBench.exe"

Compile-NppBenchmark -NeuronExe $neuronExe -RepoRoot $repoRoot `
  -SourcePath $pureNppSource -ExpectedExe $pureNppExe
Compile-NppBenchmark -NeuronExe $neuronExe -RepoRoot $repoRoot `
  -SourcePath $fusedNppSource -ExpectedExe $fusedNppExe

$cppOutDir = Join-Path $repoRoot "benchmarks/bin"
New-Item -ItemType Directory -Path $cppOutDir -Force | Out-Null

$cppNaiveSource = Join-Path $repoRoot "benchmarks/cpp/ai_tensor_bench.cpp"
$cppOptimizedSource = Join-Path $repoRoot "benchmarks/cpp/ai_tensor_bench_optimized.cpp"
$cppBlasSource = Join-Path $repoRoot "benchmarks/cpp/ai_tensor_bench_blas.cpp"
$cppFusedOptSource = Join-Path $repoRoot "benchmarks/cpp/ai_fused_bench_optimized.cpp"
$cppFusedBlasSource = Join-Path $repoRoot "benchmarks/cpp/ai_fused_bench_blas.cpp"
$cppStructuredRuntimeSource = Join-Path $repoRoot "benchmarks/cpp/ai_structured_runtime_bench.cpp"
$cppStructuredBlasSource = Join-Path $repoRoot "benchmarks/cpp/ai_structured_blas_bench.cpp"

$cppNaiveExe = Join-Path $cppOutDir "ai_tensor_cpp_naive.exe"
$cppOptimizedExe = Join-Path $cppOutDir "ai_tensor_cpp_optimized.exe"
$cppBlasExe = Join-Path $cppOutDir "ai_tensor_cpp_blas.exe"
$cppFusedOptExe = Join-Path $cppOutDir "ai_fused_cpp_optimized.exe"
$cppFusedBlasExe = Join-Path $cppOutDir "ai_fused_cpp_blas.exe"
$cppStructuredRuntimeExe = Join-Path $cppOutDir "ai_structured_runtime.exe"
$cppStructuredBlasExe = Join-Path $cppOutDir "ai_structured_blas.exe"

Compile-CppBenchmark -Name "C++ naive" -Source $cppNaiveSource -Output $cppNaiveExe
Compile-CppBenchmark -Name "C++ optimized" -Source $cppOptimizedSource -Output $cppOptimizedExe
Compile-CppBenchmark -Name "C++ fused optimized" -Source $cppFusedOptSource -Output $cppFusedOptExe

$runtimeObjects = @(
  (Join-Path $repoRoot "runtime/runtime.o"),
  (Join-Path $repoRoot "runtime/tensor.o"),
  (Join-Path $repoRoot "runtime/nn.o"),
  (Join-Path $repoRoot "runtime/io.o"),
  (Join-Path $repoRoot "runtime/gpu.o"),
  "-lm",
  "-fopenmp"
)
$hasStructuredRuntime = $true
foreach ($obj in $runtimeObjects) {
  if ($obj -is [string] -and $obj -notmatch "^-") {
    if (-not (Test-Path $obj)) {
      Write-Host "Structured runtime benchmark skipped (missing runtime object: $obj)."
      $hasStructuredRuntime = $false
      break
    }
  }
}
if ($hasStructuredRuntime) {
  Compile-CppBenchmark -Name "C++ structured runtime" -Source $cppStructuredRuntimeSource `
    -Output $cppStructuredRuntimeExe -ExtraArgs $runtimeObjects
}

$blasLinkArgPure = Try-CompileBlasBenchmark -Source $cppBlasSource -Output $cppBlasExe
$blasLinkArgFused = Try-CompileBlasBenchmark -Source $cppFusedBlasSource -Output $cppFusedBlasExe
$blasLinkArgStructured = Try-CompileBlasBenchmark -Source $cppStructuredBlasSource -Output $cppStructuredBlasExe
$hasBlasPure = $null -ne $blasLinkArgPure
$hasBlasFused = $null -ne $blasLinkArgFused
$hasBlasStructured = $null -ne $blasLinkArgStructured

if ($hasBlasPure) {
  Write-Host "BLAS pure benchmark enabled ($blasLinkArgPure)."
} else {
  Write-Host "BLAS pure benchmark not found. Pure BLAS tier skipped."
}
if ($hasBlasFused) {
  Write-Host "BLAS fused benchmark enabled ($blasLinkArgFused)."
} else {
  Write-Host "BLAS fused benchmark not found. Fused BLAS tier skipped."
}
if ($hasBlasStructured) {
  Write-Host "BLAS structured benchmark enabled ($blasLinkArgStructured)."
} else {
  Write-Host "BLAS structured benchmark not found. Structured BLAS tier skipped."
}

$pythonCommand = $null
$hasTensorFlow = $false
$pythonExe = ""
$pythonBaseArgs = @()
if ($DisableTensorFlow) {
  Write-Host "TensorFlow benchmark disabled by parameter."
} elseif (-not (Test-Path $tfBenchScript)) {
  Write-Host "TensorFlow benchmark script not found at $tfBenchScript. TensorFlow tier skipped."
} else {
  if (-not [string]::IsNullOrWhiteSpace($TensorFlowPython)) {
    if (-not (Test-Path $TensorFlowPython)) {
      throw "TensorFlowPython executable not found: $TensorFlowPython"
    }
    $pythonCommand = @{
      Exe = (Resolve-Path $TensorFlowPython).Path
      BaseArgs = @()
    }
  } else {
    $pythonCommand = Resolve-PythonCommand
  }

  if ($null -eq $pythonCommand) {
    Write-Host "Python not found. TensorFlow tier skipped."
  } else {
    $pythonExe = [string]$pythonCommand["Exe"]
    $pythonBaseArgs = @($pythonCommand["BaseArgs"])
    $hasTensorFlow = Test-TensorFlowAvailable -PythonExe $pythonExe -PythonBaseArgs $pythonBaseArgs
  }
}

$normalizedThreadCounts = Normalize-ThreadCounts -InputThreadCounts $ThreadCounts
Write-Host ("Thread sweep: {0}" -f ($normalizedThreadCounts -join ", "))
Write-Host "Thread equality is enforced for Neuron, BLAS and TensorFlow through environment variables."

$threadRunSummaries = @()
$rows = @()
$now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"

foreach ($threadCount in $normalizedThreadCounts) {
  Set-BenchmarkThreadEnv -Threads $threadCount -InterOpThreads $TfInterOpThreads
  Write-Host ""
  Write-Host ("=== Thread Count: {0} ===" -f $threadCount)

  $pureProfiles = @(
    @{ Name = "Neuron runtime"; Prefix = "Neuron"; Exe = $pureNppExe; Args = @() },
    @{ Name = "C++ naive"; Prefix = "NAIVE"; Exe = $cppNaiveExe; Args = @() },
    @{ Name = "C++ optimized"; Prefix = "OPT"; Exe = $cppOptimizedExe; Args = @() }
  )
  if ($hasBlasPure) {
    $pureProfiles += @{ Name = "C++ BLAS"; Prefix = "BLAS"; Exe = $cppBlasExe; Args = @() }
  }
  if ($hasTensorFlow) {
    $tfPureArgs = @($pythonBaseArgs + @($tfBenchScript, "--mode", "pure", "--size", "$TensorSize", "--threads", "$threadCount", "--inter-op-threads", "$TfInterOpThreads"))
    $pureProfiles += @{ Name = "TensorFlow CPU"; Prefix = "TF"; Exe = $pythonExe; Args = $tfPureArgs }
  }

  $fusedProfiles = @(
    @{ Name = "Neuron fused"; Prefix = "Neuron"; Exe = $fusedNppExe; Args = @(); Label = "Neuron_FUSED_MS" },
    @{ Name = "C++ fused opt"; Prefix = "OPT"; Exe = $cppFusedOptExe; Args = @(); Label = "OPT_FUSED_MS" }
  )
  if ($hasBlasFused) {
    $fusedProfiles += @{ Name = "C++ fused BLAS"; Prefix = "BLAS"; Exe = $cppFusedBlasExe; Args = @(); Label = "BLAS_FUSED_MS" }
  }
  if ($hasTensorFlow) {
    $tfFusedArgs = @($pythonBaseArgs + @($tfBenchScript, "--mode", "fused", "--size", "$TensorSize", "--threads", "$threadCount", "--inter-op-threads", "$TfInterOpThreads"))
    $fusedProfiles += @{ Name = "TensorFlow fused"; Prefix = "TF"; Exe = $pythonExe; Args = $tfFusedArgs; Label = "TF_FUSED_MS" }
  }

  $structuredProfiles = @()
  if ($hasStructuredRuntime) {
    $structuredProfiles += @{ Name = "Neuron structured"; Prefix = "Neuron_STRUCT"; Exe = $cppStructuredRuntimeExe; Args = @() }
  }
  if ($hasBlasStructured) {
    $structuredProfiles += @{ Name = "C++ structured BLAS"; Prefix = "BLAS_STRUCT"; Exe = $cppStructuredBlasExe; Args = @() }
  }

  foreach ($profile in $pureProfiles) { Invoke-Profile -Profile $profile | Out-Null }
  foreach ($profile in $fusedProfiles) { Invoke-Profile -Profile $profile | Out-Null }
  foreach ($profile in $structuredProfiles) { Invoke-Profile -Profile $profile | Out-Null }

  $pureResults = @{}
  foreach ($profile in $pureProfiles) {
    $pureResults[$profile.Prefix] = @{
      fma = New-Object System.Collections.Generic.List[double]
      matmul = New-Object System.Collections.Generic.List[double]
    }
  }

  $fusedResults = @{}
  foreach ($profile in $fusedProfiles) {
    $fusedResults[$profile.Prefix] = New-Object System.Collections.Generic.List[double]
  }

  $structuredResults = @{}
  foreach ($profile in $structuredProfiles) {
    $structuredResults[$profile.Prefix] = @{
      circ_matmul = New-Object System.Collections.Generic.List[double]
      toep_matmul = New-Object System.Collections.Generic.List[double]
      hybrid_matmul = New-Object System.Collections.Generic.List[double]
      circ_fused = New-Object System.Collections.Generic.List[double]
      toep_fused = New-Object System.Collections.Generic.List[double]
      hybrid_fused = New-Object System.Collections.Generic.List[double]
    }
  }

  for ($trial = 1; $trial -le $Trials; $trial++) {
    Write-Host "Running trial $trial/$Trials ..."
    foreach ($profile in $pureProfiles) {
      $output = Invoke-Profile -Profile $profile
      $metrics = Parse-PureMetrics -OutputLines $output -Prefix $profile.Prefix
      $pureResults[$profile.Prefix]["fma"].Add([double]$metrics["fma"])
      $pureResults[$profile.Prefix]["matmul"].Add([double]$metrics["matmul"])
    }
    foreach ($profile in $fusedProfiles) {
      $output = Invoke-Profile -Profile $profile
      $value = Parse-SingleMetric -OutputLines $output -MetricLabel $profile.Label
      $fusedResults[$profile.Prefix].Add([double]$value)
    }
    foreach ($profile in $structuredProfiles) {
      $output = Invoke-Profile -Profile $profile
      $metrics = Parse-StructuredMetrics -OutputLines $output -Prefix $profile.Prefix
      $structuredResults[$profile.Prefix]["circ_matmul"].Add([double]$metrics["circ_matmul"])
      $structuredResults[$profile.Prefix]["toep_matmul"].Add([double]$metrics["toep_matmul"])
      $structuredResults[$profile.Prefix]["hybrid_matmul"].Add([double]$metrics["hybrid_matmul"])
      $structuredResults[$profile.Prefix]["circ_fused"].Add([double]$metrics["circ_fused"])
      $structuredResults[$profile.Prefix]["toep_fused"].Add([double]$metrics["toep_fused"])
      $structuredResults[$profile.Prefix]["hybrid_fused"].Add([double]$metrics["hybrid_fused"])
    }
  }

  $pureMedian = @{}
  foreach ($profile in $pureProfiles) {
    $prefix = $profile.Prefix
    $pureMedian[$prefix] = @{
      fma = Get-Median -Values $pureResults[$prefix]["fma"].ToArray()
      matmul = Get-Median -Values $pureResults[$prefix]["matmul"].ToArray()
    }
  }

  $fusedMedian = @{}
  foreach ($profile in $fusedProfiles) {
    $fusedMedian[$profile.Prefix] =
        Get-Median -Values $fusedResults[$profile.Prefix].ToArray()
  }

  $structuredMedian = @{}
  foreach ($profile in $structuredProfiles) {
    $prefix = $profile.Prefix
    $structuredMedian[$prefix] = @{
      circ_matmul = Get-Median -Values $structuredResults[$prefix]["circ_matmul"].ToArray()
      toep_matmul = Get-Median -Values $structuredResults[$prefix]["toep_matmul"].ToArray()
      hybrid_matmul = Get-Median -Values $structuredResults[$prefix]["hybrid_matmul"].ToArray()
      circ_fused = Get-Median -Values $structuredResults[$prefix]["circ_fused"].ToArray()
      toep_fused = Get-Median -Values $structuredResults[$prefix]["toep_fused"].ToArray()
      hybrid_fused = Get-Median -Values $structuredResults[$prefix]["hybrid_fused"].ToArray()
    }
  }

  Write-Host ""
  Write-Host ("=== Pure Workload Median (ms) [threads={0}] ===" -f $threadCount)
  foreach ($profile in $pureProfiles) {
    $vals = $pureMedian[$profile.Prefix]
    Write-Host ("{0,-15} FMA={1,8:N2} | MatMul={2,8:N2}" -f $profile.Name, $vals["fma"], $vals["matmul"])
  }

  $nppPureFma = [double]$pureMedian["Neuron"]["fma"]
  $nppPureMatmul = [double]$pureMedian["Neuron"]["matmul"]

  Write-Host ""
  Write-Host ("=== Pure Speedup (Neuron baseline) [threads={0}] ===" -f $threadCount)
  foreach ($profile in $pureProfiles) {
    if ($profile.Prefix -eq "Neuron") { continue }
    $vals = $pureMedian[$profile.Prefix]
    $fmaSpeed = Format-Speedup -Base ([double]$vals["fma"]) -Npp $nppPureFma
    $matmulSpeed = Format-Speedup -Base ([double]$vals["matmul"]) -Npp $nppPureMatmul
    Write-Host ("Neuron vs {0,-12} FMA={1} | MatMul={2}" -f $profile.Name, $fmaSpeed, $matmulSpeed)
  }

  Write-Host ""
  Write-Host ("=== Fused Workload Median (ms) [threads={0}] ===" -f $threadCount)
  foreach ($profile in $fusedProfiles) {
    $value = [double]$fusedMedian[$profile.Prefix]
    Write-Host ("{0,-15} Fused={1,8:N2}" -f $profile.Name, $value)
  }

  $nppFused = [double]$fusedMedian["Neuron"]
  Write-Host ""
  Write-Host ("=== Fused Speedup (Neuron baseline) [threads={0}] ===" -f $threadCount)
  foreach ($profile in $fusedProfiles) {
    if ($profile.Prefix -eq "Neuron") { continue }
    $speed = Format-Speedup -Base ([double]$fusedMedian[$profile.Prefix]) -Npp $nppFused
    Write-Host ("Neuron vs {0,-12} Fused={1}" -f $profile.Name, $speed)
  }

  if ($structuredProfiles.Count -gt 0) {
    Write-Host ""
    Write-Host ("=== Structured Workload Median (ms) [threads={0}] ===" -f $threadCount)
    foreach ($profile in $structuredProfiles) {
      $vals = $structuredMedian[$profile.Prefix]
      Write-Host ("{0,-18} CIRC_MM={1,8:N2} | TOEP_MM={2,8:N2} | HYB_MM={3,8:N2}" -f
          $profile.Name, $vals["circ_matmul"], $vals["toep_matmul"], $vals["hybrid_matmul"])
      Write-Host ("{0,-18} CIRC_FU={1,8:N2} | TOEP_FU={2,8:N2} | HYB_FU={3,8:N2}" -f
          "", $vals["circ_fused"], $vals["toep_fused"], $vals["hybrid_fused"])
    }

    if ($structuredMedian.ContainsKey("Neuron_STRUCT")) {
      $nppStruct = $structuredMedian["Neuron_STRUCT"]
      Write-Host ""
      Write-Host ("=== Structured Speedup (Neuron baseline) [threads={0}] ===" -f $threadCount)
      foreach ($profile in $structuredProfiles) {
        if ($profile.Prefix -eq "Neuron_STRUCT") { continue }
        $vals = $structuredMedian[$profile.Prefix]
        Write-Host ("Neuron vs {0,-16} CIRC_MM={1} | TOEP_MM={2} | HYB_MM={3}" -f
            $profile.Name,
            (Format-Speedup -Base ([double]$vals["circ_matmul"]) -Npp ([double]$nppStruct["circ_matmul"])),
            (Format-Speedup -Base ([double]$vals["toep_matmul"]) -Npp ([double]$nppStruct["toep_matmul"])),
            (Format-Speedup -Base ([double]$vals["hybrid_matmul"]) -Npp ([double]$nppStruct["hybrid_matmul"])))
        Write-Host ("Neuron vs {0,-16} CIRC_FU={1} | TOEP_FU={2} | HYB_FU={3}" -f
            "",
            (Format-Speedup -Base ([double]$vals["circ_fused"]) -Npp ([double]$nppStruct["circ_fused"])),
            (Format-Speedup -Base ([double]$vals["toep_fused"]) -Npp ([double]$nppStruct["toep_fused"])),
            (Format-Speedup -Base ([double]$vals["hybrid_fused"]) -Npp ([double]$nppStruct["hybrid_fused"])))
      }
    }
  }

  foreach ($profile in $pureProfiles) {
    $vals = $pureMedian[$profile.Prefix]
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "pure"
      Profile = $profile.Name
      Metric = "FMA_MS"
      MedianMs = [double]$vals["fma"]
      Trials = $Trials
    }
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "pure"
      Profile = $profile.Name
      Metric = "MATMUL_MS"
      MedianMs = [double]$vals["matmul"]
      Trials = $Trials
    }
  }
  foreach ($profile in $fusedProfiles) {
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "fused"
      Profile = $profile.Name
      Metric = "FUSED_MS"
      MedianMs = [double]$fusedMedian[$profile.Prefix]
      Trials = $Trials
    }
  }
  foreach ($profile in $structuredProfiles) {
    $vals = $structuredMedian[$profile.Prefix]
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "structured"
      Profile = $profile.Name
      Metric = "CIRC_MATMUL_MS"
      MedianMs = [double]$vals["circ_matmul"]
      Trials = $Trials
    }
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "structured"
      Profile = $profile.Name
      Metric = "TOEP_MATMUL_MS"
      MedianMs = [double]$vals["toep_matmul"]
      Trials = $Trials
    }
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "structured"
      Profile = $profile.Name
      Metric = "HYBRID_MATMUL_MS"
      MedianMs = [double]$vals["hybrid_matmul"]
      Trials = $Trials
    }
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "structured"
      Profile = $profile.Name
      Metric = "CIRC_FUSED_MS"
      MedianMs = [double]$vals["circ_fused"]
      Trials = $Trials
    }
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "structured"
      Profile = $profile.Name
      Metric = "TOEP_FUSED_MS"
      MedianMs = [double]$vals["toep_fused"]
      Trials = $Trials
    }
    $rows += [PSCustomObject]@{
      Timestamp = $now
      ThreadCount = $threadCount
      Scenario = "structured"
      Profile = $profile.Name
      Metric = "HYBRID_FUSED_MS"
      MedianMs = [double]$vals["hybrid_fused"]
      Trials = $Trials
    }
  }

  $threadRunSummaries += [PSCustomObject]@{
    ThreadCount = $threadCount
    PureMedian = $pureMedian
    FusedMedian = $fusedMedian
    StructuredMedian = $structuredMedian
  }
}

Write-Host ""
Write-Host "=== Thread Sweep Summary (median ms) ==="
$summaryHeader = "{0,7} | {1,10} | {2,10} | {3,10} | {4,10} | {5,10} | {6,10}" -f "Threads", "Neuron MM", "BLAS MM", "TF MM", "Neuron Fused", "BLAS Fused", "TF Fused"
Write-Host $summaryHeader
Write-Host ("-" * $summaryHeader.Length)
foreach ($summary in $threadRunSummaries) {
  $threadCount = [int]$summary.ThreadCount
  $pureMedian = [hashtable]$summary.PureMedian
  $fusedMedian = [hashtable]$summary.FusedMedian

  $nppMm = Get-NullablePureMetric -MedianMap $pureMedian -Prefix "Neuron" -Metric "matmul"
  $blasMm = Get-NullablePureMetric -MedianMap $pureMedian -Prefix "BLAS" -Metric "matmul"
  $tfMm = Get-NullablePureMetric -MedianMap $pureMedian -Prefix "TF" -Metric "matmul"
  $nppFused = Get-NullableFusedMetric -MedianMap $fusedMedian -Prefix "Neuron"
  $blasFused = Get-NullableFusedMetric -MedianMap $fusedMedian -Prefix "BLAS"
  $tfFused = Get-NullableFusedMetric -MedianMap $fusedMedian -Prefix "TF"

  Write-Host ("{0,7} | {1,10} | {2,10} | {3,10} | {4,10} | {5,10} | {6,10}" -f
      $threadCount,
      (Format-Nullable -Value $nppMm),
      (Format-Nullable -Value $blasMm),
      (Format-Nullable -Value $tfMm),
      (Format-Nullable -Value $nppFused),
      (Format-Nullable -Value $blasFused),
      (Format-Nullable -Value $tfFused))
}

$hasStructuredAny = $false
foreach ($summary in $threadRunSummaries) {
  if (([hashtable]$summary.StructuredMedian).Count -gt 0) {
    $hasStructuredAny = $true
    break
  }
}
if ($hasStructuredAny) {
  Write-Host ""
  Write-Host "=== Structured Thread Sweep Summary (median ms) ==="
  $structuredHeader = "{0,7} | {1,14} | {2,14} | {3,14} | {4,14}" -f "Threads", "Neuron HYB MM", "BLAS HYB MM", "Neuron HYB FU", "BLAS HYB FU"
  Write-Host $structuredHeader
  Write-Host ("-" * $structuredHeader.Length)
  foreach ($summary in $threadRunSummaries) {
    $threadCount = [int]$summary.ThreadCount
    $structuredMedian = [hashtable]$summary.StructuredMedian
    $nppHybridMm = Get-NullableStructuredMetric -MedianMap $structuredMedian -Prefix "Neuron_STRUCT" -Metric "hybrid_matmul"
    $blasHybridMm = Get-NullableStructuredMetric -MedianMap $structuredMedian -Prefix "BLAS_STRUCT" -Metric "hybrid_matmul"
    $nppHybridFu = Get-NullableStructuredMetric -MedianMap $structuredMedian -Prefix "Neuron_STRUCT" -Metric "hybrid_fused"
    $blasHybridFu = Get-NullableStructuredMetric -MedianMap $structuredMedian -Prefix "BLAS_STRUCT" -Metric "hybrid_fused"
    Write-Host ("{0,7} | {1,14} | {2,14} | {3,14} | {4,14}" -f
        $threadCount,
        (Format-Nullable -Value $nppHybridMm),
        (Format-Nullable -Value $blasHybridMm),
        (Format-Nullable -Value $nppHybridFu),
        (Format-Nullable -Value $blasHybridFu))
  }
}

$resultsDir = Join-Path $repoRoot "benchmarks/results"
New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null
$csvPath = Join-Path $resultsDir "ai_tensor_results.csv"
$rows | Export-Csv -Path $csvPath -NoTypeInformation
Write-Host ""
Write-Host "CSV report written: $csvPath"
Write-Host "Benchmark completed."
