<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio 2022 (or Build Tools) with the C++ workload.
    - Downloads the vcpkg repository as a ZIP (no git clone).
    - Downloads the prebuilt vcpkg.exe (no compilation).
    - Sets VCPKG_ROOT persistently for the current user.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
    Fast mode: uses codeload ZIP + GitHub release exe. No git, no bootstrap.
#>

$ErrorActionPreference = "Stop"

Write-Host "=== AxiomDLP Agent - Windows MSVC bootstrap (fast mode) ===" -ForegroundColor Cyan

# ----------------------------------------------------------------
# 1. Verify Visual Studio
# ----------------------------------------------------------------
Write-Host "`n[1/3] Looking for Visual Studio..." -ForegroundColor Yellow

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host @"
[ERROR] Visual Studio Installer not found.

Install one of:
  - Visual Studio 2022 Community (free)
  - Visual Studio 2022 Build Tools (free, headless)

Make sure to select the workload:
  'Desktop development with C++'

Download: https://visualstudio.microsoft.com/downloads/
"@ -ForegroundColor Red
    exit 1
}

$vsInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $vsInstall) {
    Write-Host "[ERROR] Visual Studio found but the C++ workload is missing." -ForegroundColor Red
    Write-Host "        Open Visual Studio Installer, click Modify, check 'Desktop development with C++'." -ForegroundColor Red
    exit 1
}
Write-Host "  [OK] Found: $vsInstall" -ForegroundColor Green

# ----------------------------------------------------------------
# 2. vcpkg: ZIP recipes + prebuilt vcpkg.exe
# ----------------------------------------------------------------
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "$env:USERPROFILE\vcpkg" }
$vcpkgZip = "$env:USERPROFILE\vcpkg.zip"
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"

Write-Host "`n[2/3] Setting up vcpkg at $vcpkgRoot..." -ForegroundColor Yellow

# --- 2a. Recipe folder (ports/, scripts/, triplets/)
if (-not (Test-Path $toolchain)) {
    Write-Host "  Downloading vcpkg recipes ZIP (no git clone)..." -ForegroundColor Yellow

    $zipUrl = "https://codeload.github.com/microsoft/vcpkg/zip/refs/heads/master"
    try {
        Invoke-WebRequest -Uri $zipUrl -OutFile $vcpkgZip -UseBasicParsing
    }
    catch {
        Write-Host "  master not found; trying main branch..." -ForegroundColor Yellow
        $zipUrl = "https://codeload.github.com/microsoft/vcpkg/zip/refs/heads/main"
        Invoke-WebRequest -Uri $zipUrl -OutFile $vcpkgZip -UseBasicParsing
    }

    Write-Host "  Extracting..." -ForegroundColor Yellow
    Expand-Archive -Path $vcpkgZip -DestinationPath "$env:USERPROFILE" -Force

    $extracted = Get-ChildItem "$env:USERPROFILE" -Directory -Filter "vcpkg-*" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne "vcpkg" } | Select-Object -First 1

    if (-not $extracted) {
        Write-Host "[ERROR] Could not find extracted vcpkg-* folder." -ForegroundColor Red
        exit 1
    }

    if (Test-Path $vcpkgRoot) {
        Remove-Item -Recurse -Force $vcpkgRoot -ErrorAction SilentlyContinue
    }
    Rename-Item $extracted.FullName $vcpkgRoot
    Remove-Item $vcpkgZip -Force -ErrorAction SilentlyContinue
}
else {
    Write-Host "  [OK] vcpkg recipes already present." -ForegroundColor Green
}

# --- 2b. Prebuilt vcpkg.exe
if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "  Downloading prebuilt vcpkg.exe (no compilation)..." -ForegroundColor Yellow
    $exeUrl = "https://github.com/microsoft/vcpkg-tool/releases/latest/download/vcpkg.exe"
    Invoke-WebRequest -Uri $exeUrl -OutFile "$vcpkgRoot\vcpkg.exe" -UseBasicParsing
}
else {
    Write-Host "  [OK] vcpkg.exe already present." -ForegroundColor Green
}

# --- 2c. Verify both pieces
if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "[ERROR] vcpkg.exe download failed; check $vcpkgRoot" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $toolchain)) {
    Write-Host "[ERROR] vcpkg toolchain file missing at $toolchain" -ForegroundColor Red
    exit 1
}
Write-Host "  [OK] vcpkg ready: $vcpkgRoot" -ForegroundColor Green

# ----------------------------------------------------------------
# 3. Persist VCPKG_ROOT (both User scope and current process)
# ----------------------------------------------------------------
Write-Host "`n[3/3] Setting VCPKG_ROOT=$vcpkgRoot for current user..." -ForegroundColor Yellow

# Method 1: .NET API (silent if it works)
try {
    [System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")
}
catch {
    Write-Host "  setenv API failed; falling back to setx..." -ForegroundColor Yellow
}

# Method 2: setx fallback (writes to registry, works on all Windows versions)
$currentVal = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
if ($currentVal -ne $vcpkgRoot) {
    & setx VCPKG_ROOT "$vcpkgRoot" | Out-Null
}

# Also set in current process so immediate builds work
$env:VCPKG_ROOT = $vcpkgRoot

Write-Host "`n=== Setup complete ===" -ForegroundColor Cyan
Write-Host "Close this shell, open a new one, then run:" -ForegroundColor Cyan
Write-Host "  .\scripts\win\build-windows-msvc.ps1" -ForegroundColor Cyan