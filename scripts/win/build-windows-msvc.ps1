<#
.SYNOPSIS
    Build AxiomDLP Agent on Windows with MSVC + vcpkg.
#>

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",

    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------
# Save VCPKG_ROOT AND PATH BEFORE VS env init.
# ----------------------------------------------------------------
$savedVcpkgRoot = $env:VCPKG_ROOT
if (-not $savedVcpkgRoot) {
    $savedVcpkgRoot = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
}
if (-not $savedVcpkgRoot) {
    $savedVcpkgRoot = "$env:USERPROFILE\vcpkg"
}

$savedPath = $env:PATH

# ----------------------------------------------------------------
# Resolve CMake and Ninja BEFORE trimming the PATH.
# Fallback to C:\tools if not found in the current PATH.
# ----------------------------------------------------------------
$ninjaPath = (Get-Command ninja.exe -ErrorAction SilentlyContinue).Source
if (-not $ninjaPath -and (Test-Path "C:\tools\ninja\ninja.exe")) {
    $ninjaPath = "C:\tools\ninja\ninja.exe"
}
if (-not $ninjaPath) {
    Write-Host "[ERROR] ninja.exe not found on PATH or at C:\tools\ninja\ninja.exe." -ForegroundColor Red
    exit 1
}
$ninjaDir = Split-Path $ninjaPath -Parent

$cmakePath = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
if (-not $cmakePath -and (Test-Path "C:\tools\cmake\bin\cmake.exe")) {
    $cmakePath = "C:\tools\cmake\bin\cmake.exe"
}
if (-not $cmakePath) {
    Write-Host "[ERROR] cmake.exe not found on PATH or at C:\tools\cmake\bin\cmake.exe." -ForegroundColor Red
    exit 1
}
$cmakeDir = Split-Path $cmakePath -Parent

# ----------------------------------------------------------------
# TRIM PATH BEFORE vcvarsall.bat.
# We include CMake and Ninja dirs so they are not lost.
# ----------------------------------------------------------------
$minimalPathEntries = @(
    "C:\Windows\system32",
    "C:\Windows",
    "C:\Windows\System32\Wbem",
    "C:\Windows\System32\WindowsPowerShell\v1.0"
)
if ($ninjaDir -and (Test-Path $ninjaDir)) {
    $minimalPathEntries = @($ninjaDir) + $minimalPathEntries
}
if ($cmakeDir -and (Test-Path $cmakeDir)) {
    $minimalPathEntries = @($cmakeDir) + $minimalPathEntries
}

$env:PATH = ($minimalPathEntries -join ";")
Write-Host "  [OK] PATH trimmed to $($env:PATH.Length) chars before vcvarsall.bat"
Write-Host "       (ninja dir: $ninjaDir)"
Write-Host "       (cmake dir: $cmakeDir)"

# ----------------------------------------------------------------
# Auto-initialize Visual Studio environment (MSVC toolchain)
# Always run this to guarantee cl.exe is injected into the PATH,
# even if the previous terminal session had VSINSTALLDIR set.
# ----------------------------------------------------------------
$desiredArch = if ($Arch -eq "x86") { "x86" } else { "x64" }
Write-Host "Initializing Visual Studio environment..." -ForegroundColor Yellow

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "[ERROR] Visual Studio Installer not found at $vswhere" -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $vsPath) {
    Write-Host "[ERROR] Visual Studio C++ workload not found." -ForegroundColor Red
    exit 1
}

$vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
if (-not (Test-Path $vcvarsall)) {
    Write-Host "[ERROR] vcvarsall.bat not found at $vcvarsall" -ForegroundColor Red
    exit 1
}

$vsArch = if ($Arch -eq "x86") { "x86" } else { "amd64" }

$envOutput = cmd /c "`"$vcvarsall`" $vsArch >nul 2>&1 && set"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] vcvarsall.bat returned exit code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}

foreach ($line in $envOutput) {
    if ($line -match "^(.*?)=(.*)$") {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}

if (-not $env:VSINSTALLDIR) {
    Write-Host "[ERROR] vcvarsall.bat did not populate VSINSTALLDIR." -ForegroundColor Red
    exit 1
}

if (-not $env:LIB -or -not $env:INCLUDE) {
    Write-Host "  [WARN] LIB or INCLUDE not set by vcvarsall.bat. Attempting to locate Windows SDK..." -ForegroundColor Yellow
    $sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
    if (Test-Path $sdkRoot) {
        $sdkVersions = Get-ChildItem "$sdkRoot\Lib" -Directory | Sort-Object Name -Descending
        if ($sdkVersions) {
            $sdkVersion = $sdkVersions[0].Name
            $targetArch = if ($Arch -eq "x86") { "x86" } else { "x64" }
            
            if (-not $env:INCLUDE) {
                $env:INCLUDE = "$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\um;$sdkRoot\Include\$sdkVersion\shared"
            }
            if (-not $env:LIB) {
                $env:LIB = "$sdkRoot\Lib\$sdkVersion\ucrt\$targetArch;$sdkRoot\Lib\$sdkVersion\um\$targetArch"
            }
            Write-Host "  [OK] Manually set LIB and INCLUDE for Windows SDK $sdkVersion ($targetArch)" -ForegroundColor Green
        }
    }
}

Write-Host "  [OK] VS env loaded: $env:VSINSTALLDIR" -ForegroundColor Green

# ----------------------------------------------------------------
# Restore VCPKG_ROOT and PATH (minimal + Ninja)
# ----------------------------------------------------------------
if ($savedVcpkgRoot) {
    $env:VCPKG_ROOT = $savedVcpkgRoot
}

if ($ninjaDir) {
    $env:PATH = "$ninjaDir;$env:PATH"
    Write-Host "  [OK] Ninja dir prepended to PATH: $ninjaDir" -ForegroundColor Green
}

$env:CC = "cl.exe"
$env:CXX = "cl.exe"
$env:CMAKE_MAKE_PROGRAM = $ninjaPath
Write-Host "  [OK] Ninja: $env:CMAKE_MAKE_PROGRAM" -ForegroundColor Green

# ----------------------------------------------------------------
# Locate project root
# ----------------------------------------------------------------
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $projectRoot

Write-Host "`n=== AxiomDLP Agent - Windows MSVC build ===" -ForegroundColor Cyan
Write-Host "  Project root: $projectRoot"
Write-Host "  Config:       $Configuration"
Write-Host "  Arch:         $Arch"

if (-not $env:VCPKG_ROOT) {
    Write-Host "[ERROR] VCPKG_ROOT is not set." -ForegroundColor Red
    exit 1
}
Write-Host "  [OK] VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green

# ----------------------------------------------------------------
# Build dir handling
# ----------------------------------------------------------------
$presetName = "windows-$Arch-msvc-$(($Configuration).ToLower())"
$buildDir = Join-Path $projectRoot "build\$presetName"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "  Cleaning $buildDir (--Clean requested)" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# ----------------------------------------------------------------
# Configure & Build using the absolute CMake path
# ----------------------------------------------------------------
Write-Host "`n--- Configure (preset: $presetName) ---" -ForegroundColor Yellow
& $cmakePath --preset $presetName "-DCMAKE_MAKE_PROGRAM=$env:CMAKE_MAKE_PROGRAM"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Configure failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`n--- Build ---" -ForegroundColor Yellow
& $cmakePath --build --preset $presetName --config $Configuration
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------
# Report
# ----------------------------------------------------------------
$exe = Join-Path $buildDir "axiomdlp-agent.exe"
if (Test-Path $exe) {
    $size = [math]::Round((Get-Item $exe).Length / 1MB, 2)
    Write-Host "`n=== Build succeeded ===" -ForegroundColor Cyan
    Write-Host "  Binary: $exe"
    Write-Host "  Size:   $size MB"
}
else {
    Write-Host "[WARN] Build finished but $exe not found. Check the preset's binaryDir." -ForegroundColor Yellow
}