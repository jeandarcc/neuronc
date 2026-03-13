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
    [switch]$SkipReleaseArtifacts,
    [switch]$Fast,
    [switch]$UltraFast
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$IssScript = Join-Path $RepoRoot "installer\windows\neuron.iss"
$CMakeListsPath = Join-Path $RepoRoot "CMakeLists.txt"

function Get-WorkspaceStateRoot {
    param([Parameter(Mandatory = $true)][string]$RepoPath)

    $baseRoot = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($baseRoot)) {
        $baseRoot = $env:TEMP
    }
    if ([string]::IsNullOrWhiteSpace($baseRoot)) {
        throw "LOCALAPPDATA and TEMP are both unavailable."
    }

    $repoName = Split-Path -Path $RepoPath -Leaf
    return [System.IO.Path]::GetFullPath((Join-Path $baseRoot ("Neuron\workspaces\" + $repoName)))
}

function Resolve-RepoPath {
    param([Parameter(Mandatory = $true)][string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $PathValue))
}

function Add-DirectoryToPath {
    param([string]$DirectoryPath)

    if ([string]::IsNullOrWhiteSpace($DirectoryPath)) {
        return
    }

    $resolved = Resolve-RepoPath -PathValue $DirectoryPath
    if (-not (Test-Path -LiteralPath $resolved)) {
        return
    }

    $normalized = $resolved.TrimEnd('\')
    $existing = @()
    foreach ($entry in ($env:PATH -split ';')) {
        if ([string]::IsNullOrWhiteSpace($entry)) {
            continue
        }
        try {
            $existing += [System.IO.Path]::GetFullPath($entry).TrimEnd('\')
        } catch {
            $existing += $entry.TrimEnd('\')
        }
    }

    if ($existing -contains $normalized) {
        return
    }

    $env:PATH = $normalized + ";" + $env:PATH
    Write-Host "Using toolchain bin on PATH: $normalized"
}

function Prime-CompilerPath {
    foreach ($candidate in @(
        $env:NEURON_TOOLCHAIN_BIN,
        "C:\msys64\mingw64\bin"
    )) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        $compilerCandidate = Join-Path $candidate "c++.exe"
        if (Test-Path -LiteralPath $compilerCandidate) {
            Add-DirectoryToPath -DirectoryPath $candidate
            return
        }
    }
}

function Get-ProjectVersion {
    param([Parameter(Mandatory = $true)][string]$Path)

    $match = Select-String -Path $Path -Pattern 'project\s*\(\s*Neuron\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
    if ($null -eq $match) {
        throw "Could not determine project version from $Path."
    }

    return $match.Matches[0].Groups[1].Value
}

function Get-CMakeCacheValue {
    param(
        [Parameter(Mandatory = $true)][string]$CachePath,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $pattern = '^{0}(?::[^=]+)?=(.*)$' -f [regex]::Escape($Name)
    foreach ($line in Get-Content -Path $CachePath) {
        if ($line -match $pattern) {
            return $Matches[1].Trim()
        }
    }

    throw "Missing CMake cache entry '$Name' in $CachePath."
}

function Ensure-CMakeConfigured {
    param([Parameter(Mandatory = $true)][string]$ConfiguredBuildDir)

    $cachePath = Join-Path $ConfiguredBuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath) {
        return
    }

    Write-Host "Configuring CMake build directory: $ConfiguredBuildDir"
    & cmake -S $RepoRoot -B $ConfiguredBuildDir
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }
}

function Get-CMakeBuildArguments {
    param([Parameter(Mandatory = $true)][string]$CachePath)

    $args = @("--build", $BuildDir, "--target", "neuron", "ncon")
    $configTypes = ""
    try {
        $configTypes = Get-CMakeCacheValue -CachePath $CachePath -Name "CMAKE_CONFIGURATION_TYPES"
    } catch {
        $configTypes = ""
    }

    if (-not [string]::IsNullOrWhiteSpace($configTypes)) {
        $args += @("--config", "Release")
    }

    return $args
}

function Reset-Directory {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path -Force | Out-Null
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination,
        [string[]]$ExcludeNames = @()
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Source directory does not exist: $Source"
    }

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $Source -Force) {
        if ($ExcludeNames -contains $item.Name) {
            continue
        }
        Copy-Item -LiteralPath $item.FullName -Destination $Destination -Recurse -Force
    }
}

function Get-BinaryDirectory {
    param([Parameter(Mandatory = $true)][string]$ConfiguredBuildDir)

    $defaultBinDir = Join-Path $ConfiguredBuildDir "bin"
    $defaultNeuron = Join-Path $defaultBinDir "neuron.exe"
    $defaultNcon = Join-Path $defaultBinDir "ncon.exe"
    if ((Test-Path -LiteralPath $defaultNeuron) -and (Test-Path -LiteralPath $defaultNcon)) {
        return $defaultBinDir
    }

    $fallback = Get-ChildItem -Path $ConfiguredBuildDir -Recurse -Filter "neuron.exe" -File | Select-Object -First 1
    if ($null -eq $fallback) {
        throw "Could not find neuron.exe under $ConfiguredBuildDir."
    }

    return $fallback.Directory.FullName
}

function Build-DefaultNucleus {
    param(
        [Parameter(Mandatory = $true)][string]$NeuronExePath,
        [Parameter(Mandatory = $true)][string]$CompilerPath,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory
    )

    if (-not (Test-Path -LiteralPath $NeuronExePath)) {
        throw "Cannot build default nucleus: neuron executable not found at $NeuronExePath."
    }

    New-Item -ItemType Directory -Path (Split-Path -Path $OutputPath -Parent) -Force | Out-Null

    Write-Host "Building default nucleus runtime: $OutputPath"
    Push-Location $WorkingDirectory
    try {
        & $NeuronExePath "build-nucleus" "--platform" "Windows" "--compiler" $CompilerPath "--output" $OutputPath
        if ($LASTEXITCODE -ne 0) {
            throw "Default nucleus build failed."
        }
    } finally {
        Pop-Location
    }

    if (-not (Test-Path -LiteralPath $OutputPath)) {
        throw "Default nucleus output was not produced at $OutputPath."
    }
}

function Copy-ToolchainBundle {
    param(
        [Parameter(Mandatory = $true)][string]$ToolchainRoot,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    New-Item -ItemType Directory -Path $Destination -Force | Out-Null

    foreach ($dirName in @("bin", "include", "lib", "libexec")) {
        $sourceDir = Join-Path $ToolchainRoot $dirName
        if (Test-Path -LiteralPath $sourceDir) {
            Copy-DirectoryContents -Source $sourceDir -Destination (Join-Path $Destination $dirName)
        }
    }

    $targetDirs = Get-ChildItem -LiteralPath $ToolchainRoot -Directory | Where-Object {
        $_.Name -match 'w64|mingw32|windows-gnu'
    }

    foreach ($dir in $targetDirs) {
        Copy-DirectoryContents -Source $dir.FullName -Destination (Join-Path $Destination $dir.Name)
    }

    # GCC plugin headers are only needed for building GCC plugins, not for
    # compiling Neuron runtime sources. Drop them from installer payload.
    $gccLibRoot = Join-Path $Destination "lib\gcc"
    if (Test-Path -LiteralPath $gccLibRoot) {
        $pluginIncludeDirs = Get-ChildItem -LiteralPath $gccLibRoot -Recurse -Directory -ErrorAction SilentlyContinue | Where-Object {
            $_.Name -eq "include" -and $_.Parent.Name -eq "plugin"
        }
        foreach ($pluginIncludeDir in $pluginIncludeDirs) {
            Remove-Item -LiteralPath $pluginIncludeDir.FullName -Recurse -Force
        }
    }
}

function Find-ISCC {
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $resolved = Resolve-RepoPath -PathValue $ExplicitPath
        if (-not (Test-Path -LiteralPath $resolved)) {
            throw "ISCC.exe was not found at $resolved."
        }
        return $resolved
    }

    $command = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    foreach ($candidate in @(
        (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    )) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    throw "ISCC.exe not found. Install Inno Setup 6 or pass -ISCCPath."
}

function Get-FileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function New-PortableZipArtifact {
    param(
        [Parameter(Mandatory = $true)][string]$VersionValue,
        [Parameter(Mandatory = $true)][string]$StagedSourceDir,
        [Parameter(Mandatory = $true)][string]$DestinationDir
    )

    $portableRootName = "Neuron-$VersionValue-windows-x64-portable"
    $portableRootDir = Join-Path $DestinationDir $portableRootName
    Write-Host "Preparing portable payload tree: $portableRootDir"
    Reset-Directory -Path $portableRootDir
    Copy-DirectoryContents -Source $StagedSourceDir -Destination $portableRootDir

    $zipPath = Join-Path $DestinationDir ($portableRootName + ".zip")
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Write-Host "Creating portable archive: $zipPath"
    Compress-Archive -LiteralPath $portableRootDir -DestinationPath $zipPath -CompressionLevel Optimal
    if (Test-Path -LiteralPath $portableRootDir) {
        Remove-Item -LiteralPath $portableRootDir -Recurse -Force
    }
    return $zipPath
}

function New-ReleaseArtifacts {
    param(
        [Parameter(Mandatory = $true)][string]$VersionValue,
        [Parameter(Mandatory = $true)][string]$StagedSourceDir,
        [Parameter(Mandatory = $true)][string]$InstallerOutputDir,
        [Parameter(Mandatory = $true)][string]$ReleaseRootDir,
        [string]$InstallerArtifactPath
    )

    $releaseArtifactDir = Join-Path $ReleaseRootDir ("Neuron-$VersionValue-windows-x64")
    Reset-Directory -Path $releaseArtifactDir

    $artifacts = @()
    if (-not [string]::IsNullOrWhiteSpace($InstallerArtifactPath) -and (Test-Path -LiteralPath $InstallerArtifactPath)) {
        $installerFileName = [System.IO.Path]::GetFileName($InstallerArtifactPath)
        $installerReleasePath = Join-Path $releaseArtifactDir $installerFileName
        Copy-Item -LiteralPath $InstallerArtifactPath -Destination $installerReleasePath -Force
        $artifacts += [pscustomobject]@{
            name = $installerFileName
            kind = "installer"
            path = $installerReleasePath
            size = (Get-Item -LiteralPath $installerReleasePath).Length
            sha256 = Get-FileSha256 -Path $installerReleasePath
        }
    }

    $portableZipPath = New-PortableZipArtifact -VersionValue $VersionValue -StagedSourceDir $StagedSourceDir -DestinationDir $releaseArtifactDir
    $artifacts += [pscustomobject]@{
        name = [System.IO.Path]::GetFileName($portableZipPath)
        kind = "portable_zip"
        path = $portableZipPath
        size = (Get-Item -LiteralPath $portableZipPath).Length
        sha256 = Get-FileSha256 -Path $portableZipPath
    }

    $checksumsPath = Join-Path $releaseArtifactDir "SHA256SUMS.txt"
    $checksumLines = @()
    foreach ($artifact in $artifacts) {
        $checksumLines += "{0} *{1}" -f $artifact.sha256, $artifact.name
    }
    Set-Content -LiteralPath $checksumsPath -Value $checksumLines -Encoding ascii

    $manifestPath = Join-Path $releaseArtifactDir "release-manifest.json"
    $manifest = [pscustomobject]@{
        version = $VersionValue
        platform = "windows-x64"
        generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
        stage_dir = $StagedSourceDir
        installer_output_dir = $InstallerOutputDir
        artifacts = @(
            foreach ($artifact in $artifacts) {
                [pscustomobject]@{
                    name = $artifact.name
                    kind = $artifact.kind
                    size = $artifact.size
                    sha256 = $artifact.sha256
                }
            }
        )
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8

    Write-Host "Release artifacts ready at $releaseArtifactDir"
    return $releaseArtifactDir
}

$WorkspaceStateRoot = Get-WorkspaceStateRoot -RepoPath $RepoRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $WorkspaceStateRoot "build"
}
if ([string]::IsNullOrWhiteSpace($StageDir)) {
    $StageDir = Join-Path $WorkspaceStateRoot "installer\stage"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $WorkspaceStateRoot "installer\out"
    if ($Fast) {
        $OutputDir += "-fast"
    } elseif ($UltraFast) {
        $OutputDir += "-ultrafast"
    }
}
if ([string]::IsNullOrWhiteSpace($ReleaseDir)) {
    $ReleaseDir = Join-Path $WorkspaceStateRoot "installer\release"
}

$BuildDir = Resolve-RepoPath -PathValue $BuildDir
$StageDir = Resolve-RepoPath -PathValue $StageDir
$OutputDir = Resolve-RepoPath -PathValue $OutputDir
$ReleaseDir = Resolve-RepoPath -PathValue $ReleaseDir

Prime-CompilerPath

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-ProjectVersion -Path $CMakeListsPath
}

Ensure-CMakeConfigured -ConfiguredBuildDir $BuildDir
$CachePath = Join-Path $BuildDir "CMakeCache.txt"
$CompilerPath = Get-CMakeCacheValue -CachePath $CachePath -Name "CMAKE_CXX_COMPILER"
$CompilerBinDir = Split-Path -Path $CompilerPath -Parent
Add-DirectoryToPath -DirectoryPath $CompilerBinDir

if (-not $SkipBuild) {
    Write-Host "Building neuron target in $BuildDir"
    $buildArgs = Get-CMakeBuildArguments -CachePath $CachePath
    & cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed."
    }
}

Reset-Directory -Path $StageDir
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null

$BuildBinDir = Get-BinaryDirectory -ConfiguredBuildDir $BuildDir
$StageBinDir = Join-Path $StageDir "bin"
New-Item -ItemType Directory -Path $StageBinDir -Force | Out-Null

$binArtifacts = Get-ChildItem -LiteralPath $BuildBinDir -File | Where-Object {
    $_.Name -eq "neuron.exe" -or $_.Name -eq "ncon.exe" -or $_.Extension -eq ".dll"
}

if ($binArtifacts.Count -eq 0) {
    throw "No installer artifacts found in $BuildBinDir."
}

foreach ($artifact in $binArtifacts) {
    Copy-Item -LiteralPath $artifact.FullName -Destination $StageBinDir -Force
}

$NeuronExePath = Join-Path $BuildBinDir "neuron.exe"
$DefaultNucleusPath = Join-Path $StageBinDir "nucleus.exe"
Build-DefaultNucleus -NeuronExePath $NeuronExePath -CompilerPath $CompilerPath -OutputPath $DefaultNucleusPath -WorkingDirectory $RepoRoot

$StageRuntimeDir = Join-Path $StageDir "runtime"
Copy-DirectoryContents -Source (Join-Path $RepoRoot "runtime\include") -Destination (Join-Path $StageRuntimeDir "include")
Copy-DirectoryContents -Source (Join-Path $RepoRoot "runtime\src") -Destination (Join-Path $StageRuntimeDir "src")
Copy-DirectoryContents -Source (Join-Path $RepoRoot "runtime\installer") -Destination (Join-Path $StageRuntimeDir "installer")
Copy-DirectoryContents `
    -Source (Join-Path $RepoRoot "runtime\minimal") `
    -Destination (Join-Path $StageRuntimeDir "minimal") `
    -ExcludeNames @("windows_x64", "linux_x64", "macos_arm64")

$StageIncludeDir = Join-Path $StageDir "include"
Copy-DirectoryContents -Source (Join-Path $RepoRoot "include") -Destination $StageIncludeDir

$StageSrcDir = Join-Path $StageDir "src"
New-Item -ItemType Directory -Path $StageSrcDir -Force | Out-Null
Copy-DirectoryContents -Source (Join-Path $RepoRoot "src\cli\templates") -Destination (Join-Path $StageSrcDir "cli\templates")
Copy-DirectoryContents -Source (Join-Path $RepoRoot "docs\learnneuron") -Destination (Join-Path $StageSrcDir "cli\templates\agents\learnneuron")
Copy-DirectoryContents -Source (Join-Path $RepoRoot "src\ncon") -Destination (Join-Path $StageSrcDir "ncon")
Copy-Item -LiteralPath (Join-Path $RepoRoot "src\ncon_mini_main.cpp") -Destination $StageSrcDir -Force

if (-not $NoToolchainBundle) {
    $ToolchainBinDir = Split-Path -Path $CompilerPath -Parent
    $ToolchainRoot = Split-Path -Path $ToolchainBinDir -Parent
    if (-not (Test-Path -LiteralPath (Join-Path $ToolchainBinDir "gcc.exe"))) {
        Write-Warning "gcc.exe was not found under $ToolchainBinDir. Skipping bundled toolchain."
    } else {
        Write-Host "Bundling toolchain from $ToolchainRoot"
        Copy-ToolchainBundle -ToolchainRoot $ToolchainRoot -Destination (Join-Path $StageDir "toolchain")
    }
}

$InstallerPath = ""
if ($SkipInstaller) {
    Write-Host "Installer stage ready at $StageDir"
} else {
    $ResolvedISCC = Find-ISCC -ExplicitPath $ISCCPath
    Write-Host "Building installer with $ResolvedISCC"

    $isccArgs = @("/DAppVersion=$Version", "/DSourceDir=$StageDir", "/DOutputDir=$OutputDir")
    if ($UltraFast) {
        $isccArgs += "/DCompressionLevel=none"
        $isccArgs += "/DSolidLevel=no"
    } elseif ($Fast) {
        $isccArgs += "/DCompressionLevel=lzma2/fast"
        $isccArgs += "/DSolidLevel=no"
    }

    & $ResolvedISCC $isccArgs $IssScript
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup build failed."
    }

    $InstallerPath = Join-Path $OutputDir ("Neuron-{0}-windows-x64.exe" -f $Version)
    if (Test-Path -LiteralPath $InstallerPath) {
        Write-Host "Installer ready: $InstallerPath"
    } else {
        Write-Host "Installer build completed. Check $OutputDir for the output executable."
    }
}

if ($SkipReleaseArtifacts) {
    Write-Host "Skipping release artifact generation."
} else {
    Write-Host "Generating release artifacts..."
    New-ReleaseArtifacts -VersionValue $Version -StagedSourceDir $StageDir -InstallerOutputDir $OutputDir -ReleaseRootDir $ReleaseDir -InstallerArtifactPath $InstallerPath | Out-Null
}
