<#
.SYNOPSIS
    Build AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    Auto-initializes the Visual Studio environment (vcvarsall.bat) so this
    script works from ANY PowerShell window. Preserves VCPKG_ROOT and PATH
    across vcvarsall.bat, which rewrites both:
      - VCPKG_ROOT -> VS-bundled vcpkg (wrong one)
      - PATH       -> VS toolchain only (drops chocolatey, ninja, etc.)

    Resolves Ninja on PATH and pins it into CMAKE_MAKE_PROGRAM. This
    ensures the OUTER CMake build always uses a known-good Ninja
    regardless of what vcvarsall.bat leaves on PATH.

    CRITICAL: The windows-2022 runner has a very long PATH. Windows cmd.exe
    has an 8191-character command-line limit. vcvarsall.bat internally builds
    a command that includes the entire PATH. When PATH is too long,
    vcvarsall.bat exits with code 255. This script trims PATH to only the
    entries needed for the build BEFORE calling vcvarsall.bat, and explicitly
    ensures LIB and INCLUDE are set for the linker.

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
# TRIM PATH BEFORE vcvarsall.bat.
#
# The windows-2022 runner has a PATH that routinely exceeds 7000
# characters. vcvarsall.bat builds a command line that includes the
# full PATH, and cmd.exe rejects command lines over 8191 chars with
# exit code 255. We keep only what vcvarsall.bat itself needs:
#   - System32 / Windows / Wbem (cmd.exe, where.exe, system DLLs)
#   - The directory containing ninja.exe (so find_program works)
#   - PowerShell's own directory (in case a sub-script needs it)
# ----------------------------------------------------------------
$ninjaCmd = Get-Command ninja.exe -ErrorAction SilentlyContinue
$ninjaDir = if ($ninjaCmd) { Split-Path $ninjaCmd.Source -Parent } else { $null }

$minimalPathEntries = @(
    "C:\Windows\system32",
    "C:\Windows",
    "C:\Windows\System32\Wbem",
    "C:\Windows\System32\WindowsPowerShell\v1.0"
)
if ($ninjaDir -and (Test-Path $ninjaDir)) {
    $minimalPathEntries = @($ninjaDir) + $minimalPathEntries
}

$env:PATH = ($minimalPathEntries -join ";")
Write-Host "  [OK] PATH trimmed to $($env:PATH.Length) chars before vcvarsall.bat"
Write-Host "       (ninja dir: $ninjaDir)"

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

    # Run vcvarsall.bat with the trimmed PATH.
    $envOutput = cmd /c "`"$vcvarsall`" $vsArch >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] vcvarsall.bat returned exit code $LASTEXITCODE" -ForegroundColor Red
        Write-Host "        PATH was trimmed to $($env:PATH.Length) chars but vcvarsall.bat still failed." -ForegroundColor Red
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

    # --- FIX: Verify LIB and INCLUDE are set ---
    # If vcvarsall.bat failed to populate these (e.g. SDK not found in registry),
    # manually inject the standard Windows 10/11 SDK paths to prevent LNK1104.
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
            } else {
                Write-Host "[ERROR] No SDK versions found in $sdkRoot\Lib" -ForegroundColor Red
            }
        } else {
            Write-Host "[ERROR] Windows Kits directory not found at $sdkRoot" -ForegroundColor Red
        }
    }

    Write-Host "  [OK] VS env loaded: $env:VSINSTALLDIR" -ForegroundColor Green
}
else {
    Write-Host "  [OK] VS env already loaded: $env:VSINSTALLDIR" -ForegroundColor Green
}

# ----------------------------------------------------------------
# Restore VCPKG_ROOT
# ----------------------------------------------------------------
if ($savedVcpkgRoot) {
    $previous = $env:VCPKG_ROOT
    $env:VCPKG_ROOT = $savedVcpkgRoot
    if ($previous -and $previous -ne $savedVcpkgRoot) {
        Write-Host "  [OK] Restored VCPKG_ROOT: $env:VCPKG_ROOT" -ForegroundColor Green
    }
}

# ----------------------------------------------------------------
# Restore PATH (minimal + Ninja)
# ----------------------------------------------------------------
if ($ninjaDir) {
    $env:PATH = "$ninjaDir;$env:PATH"
    Write-Host "  [OK] Ninja dir prepended to PATH: $ninjaDir" -ForegroundColor Green
}

# ----------------------------------------------------------------
# Force MSVC compilers
# ----------------------------------------------------------------
$env:CC = "cl.exe"
$env:CXX = "cl.exe"

# ----------------------------------------------------------------
# Resolve Ninja and pin it into CMAKE_MAKE_PROGRAM.
# ----------------------------------------------------------------
if (-not $ninjaCmd) {
    Write-Host @"
[ERROR] ninja.exe not found on PATH.
"@ -ForegroundColor Red
    exit 1
}

$env:CMAKE_MAKE_PROGRAM = $ninjaCmd.Source
Write-Host "  [OK] Ninja: $env:CMAKE_MAKE_PROGRAM" -ForegroundColor Green

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
    Write-Host "[ERROR] VCPKG_ROOT is not set." -ForegroundColor Red
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