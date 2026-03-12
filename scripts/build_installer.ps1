[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$StageDir = "",
    [string]$OutputDir = "",
    [string]$ReleaseDir = "",
    [string]$Version = "",
    [string]$ISCCPath = "",
    [switch]$SkipBuild,
    [switch]$SkipInstaller,
    [switch]$NoToolchainBundle,
    [switch]$SkipReleaseArtifacts
)

$ScriptPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\installer\windows\Build-Installer.ps1"))
$ForwardedArgs = @{}

if (-not [string]::IsNullOrWhiteSpace($BuildDir)) {
    $ForwardedArgs.BuildDir = $BuildDir
}

if (-not [string]::IsNullOrWhiteSpace($StageDir)) {
    $ForwardedArgs.StageDir = $StageDir
}

if (-not [string]::IsNullOrWhiteSpace($OutputDir)) {
    $ForwardedArgs.OutputDir = $OutputDir
}

if (-not [string]::IsNullOrWhiteSpace($ReleaseDir)) {
    $ForwardedArgs.ReleaseDir = $ReleaseDir
}

if (-not [string]::IsNullOrWhiteSpace($Version)) {
    $ForwardedArgs.Version = $Version
}

if (-not [string]::IsNullOrWhiteSpace($ISCCPath)) {
    $ForwardedArgs.ISCCPath = $ISCCPath
}

if ($SkipBuild) {
    $ForwardedArgs.SkipBuild = $true
}

if ($SkipInstaller) {
    $ForwardedArgs.SkipInstaller = $true
}

if ($NoToolchainBundle) {
    $ForwardedArgs.NoToolchainBundle = $true
}

if ($SkipReleaseArtifacts) {
    $ForwardedArgs.SkipReleaseArtifacts = $true
}

& $ScriptPath @ForwardedArgs
exit $LASTEXITCODE
