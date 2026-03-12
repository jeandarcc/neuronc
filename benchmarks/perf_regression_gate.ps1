param(
  [string]$BaselineCsv = "benchmarks/results/baseline_ai_tensor_results.csv",
  [string]$CurrentCsv = "benchmarks/results/ai_tensor_results.csv",
  [double]$MaxRegressionPercent = 5.0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path $BaselineCsv)) {
  throw "Baseline CSV not found: $BaselineCsv"
}
if (-not (Test-Path $CurrentCsv)) {
  throw "Current CSV not found: $CurrentCsv"
}

$baselineRows = Import-Csv $BaselineCsv |
  Where-Object { $_.Profile -match "^N\+\+" } |
  Group-Object Scenario, Metric, ThreadCount
$currentRows = Import-Csv $CurrentCsv |
  Where-Object { $_.Profile -match "^N\+\+" } |
  Group-Object Scenario, Metric, ThreadCount

$baselineMap = @{}
foreach ($group in $baselineRows) {
  $threadCount = if ($group.Group[0].PSObject.Properties.Name -contains "ThreadCount") { $group.Group[0].ThreadCount } else { "default" }
  $key = "$($group.Group[0].Scenario)|$($group.Group[0].Metric)|$threadCount"
  $baselineMap[$key] = [double]$group.Group[0].MedianMs
}

$currentMap = @{}
foreach ($group in $currentRows) {
  $threadCount = if ($group.Group[0].PSObject.Properties.Name -contains "ThreadCount") { $group.Group[0].ThreadCount } else { "default" }
  $key = "$($group.Group[0].Scenario)|$($group.Group[0].Metric)|$threadCount"
  $currentMap[$key] = [double]$group.Group[0].MedianMs
}

$failed = $false
foreach ($key in $baselineMap.Keys) {
  if (-not $currentMap.ContainsKey($key)) {
    Write-Host "WARN: missing metric in current run: $key"
    continue
  }
  $base = [double]$baselineMap[$key]
  $curr = [double]$currentMap[$key]
  if ($base -le 0.0) {
    continue
  }

  $regressionPercent = (($curr - $base) / $base) * 100.0
  Write-Host ("{0,-24} baseline={1,8:N2} current={2,8:N2} regression={3,6:N2}%" -f $key, $base, $curr, $regressionPercent)
  if ($regressionPercent -gt $MaxRegressionPercent) {
    $failed = $true
  }
}

if ($failed) {
  throw "Performance regression gate failed (> $MaxRegressionPercent%)."
}

Write-Host "Performance regression gate passed."
