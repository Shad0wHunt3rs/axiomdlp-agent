<#
.SYNOPSIS
    Build AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    Auto-initializes the Visual Studio environment (vcvarsall.bat) so this
    script works from ANY PowerShell window. Preserves VCPKG_ROOT and PATH
    across vcvarsall.bat, which rewrites both:
      - VCPKG_ROOT -> VS-bundled vcpkg (wrong one)
      - PATH       -> VS toolchain only (drops chocolatey, ninja, etc.)

    Also exports CMAKE_MAKE_PROGRAM to the resolved Ninja path and passes
    it to CMake as a cache variable. This is required when
    VCPKG_FORCE_SYSTEM_BINARIES=1 is set (CI does this) -- vcpkg's nested
    CMake invocations for its detect_compiler port do not inherit the
    parent process env, so the only reliable way to make Ninja visible
    inside vcpkg is via -DCMAKE_MAKE_PROGRAM=... on the configure line.

.PARAMETER Configuration
    Debug or Release (default: Release).

.PARAMETER Arch
    x64 or x86 (default: x64).

.PARAMETER Clean
    If set, delete the build directory before configuring.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
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
#
# vcvarsall.bat rewrites both of these. VCPKG_ROOT gets replaced with
# the VS-bundled vcpkg; PATH gets replaced with the VS toolchain only.
# The PATH rewrite is what caused the CI failure:
#
#     error: Could not fetch ninja.
#     CMake Error: CMAKE_MAKE_PROGRAM is not set.
#
# because after vcvarsall.bat, chocolatey's ninja.exe was no longer on
# PATH, so vcpkg's detect_compiler port and CMake's Ninja generator
# both failed to find it.
# ----------------------------------------------------------------
$savedVcpkgRoot = $env:VCPKG_ROOT
if (-not $savedVcpkgRoot) {
    $savedVcpkgRoot = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
}
if (-not $savedVcpkgRoot) {
    $savedVcpkgRoot = "$env:USERPROFILE\vcpkg"
}

$savedPath = $env:PATH
if (-not $savedPath) {
    $savedPath = [System.Environment]::GetEnvironmentVariable("PATH", "Machine") + ";" +
                 [System.Environment]::GetEnvironmentVariable("PATH", "User")
}

# ----------------------------------------------------------------
# Auto-initialize Visual Studio environment (MSVC toolchain)
# ----------------------------------------------------------------
$desiredArch = if ($Arch -eq "x86") { "x86" } else { "x64" }
$currentArch = $env:VSCMD_ARG_TGT_ARCH
if (-not $env:VSINSTALLDIR -or $currentArch -ne $desiredArch) {
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
    foreach ($line in $envOutput) {
        if ($line -match "^(.*?)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }

    if (-not $env:VSINSTALLDIR) {
        Write-Host "[ERROR] vcvarsall.bat did not populate VSINSTALLDIR." -ForegroundColor Red
        exit 1
    }
    Write-Host "  [OK] VS env loaded: $env:VSINSTALLDIR" -ForegroundColor Green
}
else {
    Write-Host "  [OK] VS env already loaded: $env:VSINSTALLDIR" -ForegroundColor Green
}

# ----------------------------------------------------------------
# Restore VCPKG_ROOT (vcvarsall.bat overwrites it to the VS-bundled
# vcpkg, which requires builtin-baseline and uses different ports).
# ----------------------------------------------------------------
if ($savedVcpkgRoot) {
    $previous = $env:VCPKG_ROOT
    $env:VCPKG_ROOT = $savedVcpkgRoot
    if ($previous -and $previous -ne $savedVcpkgRoot) {
        Write-Host "  [OK] Restored VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green
        Write-Host "       (vcvarsall.bat had set it to: $previous)" -ForegroundColor DarkGray
    }
}

# ----------------------------------------------------------------
# Restore PATH so chocolatey-installed tools (ninja in particular)
# remain visible to CMake and to vcpkg.
# ----------------------------------------------------------------
if ($savedPath) {
    $env:PATH = $env:PATH + ";" + $savedPath
    Write-Host "  [OK] Restored PATH (VS env first, original appended)" -ForegroundColor Green
}

# ----------------------------------------------------------------
# Refuse to use the VS-bundled vcpkg - it requires builtin-baseline
# in vcpkg.json which the standalone vcpkg does not need.
# ----------------------------------------------------------------
if ($env:VCPKG_ROOT -match "Microsoft Visual Studio.*VC[\\/]vcpkg") {
    Write-Host "[ERROR] VCPKG_ROOT points at the VS-bundled vcpkg:" -ForegroundColor Red
    Write-Host "        $env:VCPKG_ROOT" -ForegroundColor Red
    Write-Host "        This is different from your standalone vcpkg." -ForegroundColor Red
    Write-Host "        Set VCPKG_ROOT to the standalone path (e.g. C:\Users\User\vcpkg)" -ForegroundColor Red
    exit 1
}

# Force MSVC compilers (overrides any GCC on PATH)
$env:CC = "cl.exe"
$env:CXX = "cl.exe"

# ----------------------------------------------------------------
# Resolve Ninja and make it unmissable.
#
# This is the fix for the CI failure:
#
#     error: while detecting compiler information:
#     error: Could not fetch ninja. A downloadable copy of this tool
#     is available and can be used by unsetting
#     %VCPKG_FORCE_SYSTEM_BINARIES%.
#
# CI sets VCPKG_FORCE_SYSTEM_BINARIES=1, which forbids vcpkg from
# downloading its own Ninja. vcpkg's detect_compiler port then runs
# a NESTED cmake that does NOT inherit the parent process env, so a
# bare `$env:CMAKE_MAKE_PROGRAM = ...` is insufficient. We must both:
#
#   1. Put the Ninja directory FIRST on PATH (so any nested
#      find_program(NINJA) sees it before anything else), and
#   2. Pass -DCMAKE_MAKE_PROGRAM=<path> on the configure line (so
#      the value is baked into the CMake cache that every nested
#      cmake inherits from its parent CMakeLists.txt).
#
# We also fail fast if Ninja is absent, so the operator gets a clear
# error instead of vcpkg's opaque one.
# ----------------------------------------------------------------
$ninjaCmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
if (-not $ninjaCmd) {
    Write-Host @"
[ERROR] ninja.exe not found on PATH.

CI relies on VCPKG_FORCE_SYSTEM_BINARIES=1, which forbids vcpkg from
downloading its own Ninja. A system Ninja MUST be present.

Install it with:
    choco install ninja -y

Then re-open the shell (or re-run the workflow) so PATH picks it up.
"@ -ForegroundColor Red
    exit 1
}

$ninjaDir = Split-Path $ninjaCmd.Source -Parent
$env:PATH = "$ninjaDir;$env:PATH"
$env:CMAKE_MAKE_PROGRAM = $ninjaCmd.Source
Write-Host "  [OK] Ninja: $env:CMAKE_MAKE_PROGRAM" -ForegroundColor Green
Write-Host "  [OK] Ninja dir prepended to PATH: $ninjaDir" -ForegroundColor Green

# Sanity check: the file must actually exist.
if (-not (Test-Path $env:CMAKE_MAKE_PROGRAM)) {
    Write-Host "[ERROR] CMAKE_MAKE_PROGRAM points at a missing file: $env:CMAKE_MAKE_PROGRAM" -ForegroundColor Red
    exit 1
}

# ----------------------------------------------------------------
# Locate project root
# ----------------------------------------------------------------
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $projectRoot

Write-Host "`n=== AxiomDLP Agent - Windows MSVC build ===" -ForegroundColor Cyan
Write-Host "  Project root: $projectRoot"
Write-Host "  Config:       $Configuration"
Write-Host "  Arch:         $Arch"

# ----------------------------------------------------------------
# Verify VCPKG_ROOT
# ----------------------------------------------------------------
if (-not $env:VCPKG_ROOT) {
    Write-Host @"
[ERROR] VCPKG_ROOT is not set.

Run the one-time setup first:
    .\scripts\win\setup-windows-msvc.ps1

Then close this shell, open a new one, and try again.
"@ -ForegroundColor Red
    exit 1
}
if (-not (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
    Write-Host "[ERROR] vcpkg toolchain file not found at $env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -ForegroundColor Red
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

# Auto-clean if cached compiler is not MSVC
$cacheFile = Join-Path $buildDir "CMakeCache.txt"
if (Test-Path $cacheFile) {
    $cacheText = Get-Content $cacheFile -Raw
    if ($cacheText -match "CMAKE_C_COMPILER:[^=]*=.*(gcc|cc\.exe)" -or
        $cacheText -match "CMAKE_CXX_COMPILER:[^=]*=.*(g\+\+|c\+\+\.exe)") {
        Write-Host "  Stale CMake cache with GCC compiler found; cleaning build dir..." -ForegroundColor Yellow
        Remove-Item -Recurse -Force $buildDir
    }
}

# ----------------------------------------------------------------
# Configure.
#
# The preset's cacheVariables block already carries
#   "CMAKE_MAKE_PROGRAM": "$env{CMAKE_MAKE_PROGRAM}"
# so we only need to make sure the env var is set (it is, above).
# We ALSO pass -DCMAKE_MAKE_PROGRAM explicitly as a belt-and-braces
# measure: if a future preset edit forgets the cacheVariables entry,
# the build still works.
# ----------------------------------------------------------------
Write-Host "`n--- Configure (preset: $presetName) ---" -ForegroundColor Yellow
Write-Host "  CMAKE_MAKE_PROGRAM=$env:CMAKE_MAKE_PROGRAM"

cmake --preset $presetName "-DCMAKE_MAKE_PROGRAM=$env:CMAKE_MAKE_PROGRAM"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Configure failed." -ForegroundColor Red
    exit $LASTEXITCODE
}

# ----------------------------------------------------------------
# Build
# ----------------------------------------------------------------
Write-Host "`n--- Build ---" -ForegroundColor Yellow
cmake --build --preset $presetName --config $Configuration
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