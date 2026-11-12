<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio 2022 (or Build Tools) with the C++ workload.
    - Downloads the vcpkg repository as a ZIP (no git clone).
    - Downloads the prebuilt vcpkg.exe (no compilation).
    - Verifies vcpkg.exe runs and the recipe folder is complete.
    - Persists VCPKG_ROOT for the current workflow (GITHUB_ENV), the
      current user (registry), and the current process.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
    Fast mode: uses codeload ZIP + GitHub release exe. No git, no bootstrap.
#>

$ErrorActionPreference = "Stop"

Write-Host "=== AxiomDLP Agent - Windows MSVC bootstrap (fast mode) ===" -ForegroundColor Cyan

# ----------------------------------------------------------------
# 1. Verify Visual Studio
# ----------------------------------------------------------------
Write-Host "`n[1/4] Looking for Visual Studio..." -ForegroundColor Yellow

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
# 2. vcpkg recipes (ZIP) + prebuilt vcpkg.exe
# ----------------------------------------------------------------
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "$env:USERPROFILE\vcpkg" }
$vcpkgZip = "$env:USERPROFILE\vcpkg.zip"
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"

Write-Host "`n[2/4] Setting up vcpkg at $vcpkgRoot..." -ForegroundColor Yellow

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

# ----------------------------------------------------------------
# 3. Verify vcpkg.exe runs and recipe folder is complete
# ----------------------------------------------------------------
Write-Host "`n[3/4] Verifying vcpkg installation..." -ForegroundColor Yellow

if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "[ERROR] vcpkg.exe missing at $vcpkgRoot" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $toolchain)) {
    Write-Host "[ERROR] vcpkg toolchain file missing at $toolchain" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path "$vcpkgRoot\ports")) {
    Write-Host "[ERROR] vcpkg ports directory missing at $vcpkgRoot\ports" -ForegroundColor Red
    Write-Host "        The recipe ZIP may have extracted incorrectly." -ForegroundColor Red
    exit 1
}
if (-not (Test-Path "$vcpkgRoot\triplets")) {
    Write-Host "[ERROR] vcpkg triplets directory missing at $vcpkgRoot\triplets" -ForegroundColor Red
    exit 1
}

# vcpkg.exe must actually run - catches wrong-architecture exe downloads
try {
    $vcpkgVersionOutput = & "$vcpkgRoot\vcpkg.exe" version 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] vcpkg.exe version returned exit code $LASTEXITCODE" -ForegroundColor Red
        Write-Host "        Output: $vcpkgVersionOutput" -ForegroundColor Red
        exit 1
    }
    Write-Host "  [OK] vcpkg.exe runs: $vcpkgVersionOutput" -ForegroundColor Green
}
catch {
    Write-Host "[ERROR] vcpkg.exe failed to execute: $_" -ForegroundColor Red
    exit 1
}

Write-Host "  [OK] vcpkg ready: $vcpkgRoot" -ForegroundColor Green

# ----------------------------------------------------------------
# 4. Persist VCPKG_ROOT
#
# Three independent mechanisms, so this works both under GitHub
# Actions and on a developer workstation:
#
#   a) $GITHUB_ENV (CI only)  — the ONLY way to propagate an env var
#      from one workflow step to the next. Each step is a fresh
#      PowerShell process, so `$env:VCPKG_ROOT = ...` inside this
#      step is invisible to the next step unless we also write to
#      $GITHUB_ENV.
#
#   b) User-scope registry    — what `setx`/SetEnvironmentVariable
#      "User" writes. Picked up by new login shells. Used locally and
#      as a fallback if $GITHUB_ENV is somehow unavailable.
#
#   c) Current process        — makes the "Close this shell, open a
#      new one" message on a developer workstation true, and lets the
#      same PowerShell session run the build immediately.
# ----------------------------------------------------------------
Write-Host "`n[4/4] Setting VCPKG_ROOT=$vcpkgRoot..." -ForegroundColor Yellow

# (a) GitHub Actions cross-step propagation
if ($env:GITHUB_ENV) {
    Add-Content -Path $env:GITHUB_ENV -Value "VCPKG_ROOT=$vcpkgRoot"
    Write-Host "  [OK] Wrote VCPKG_ROOT to GITHUB_ENV (propagates to next steps)" -ForegroundColor Green
}
else {
    Write-Host "  [--] GITHUB_ENV not set (not running under GitHub Actions); skipping" -ForegroundColor DarkGray
}

# (b) User-scope registry (for developer workstations and CI fallback)
try {
    [System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")
    Write-Host "  [OK] Set VCPKG_ROOT in User-scope registry" -ForegroundColor Green
}
catch {
    Write-Host "  [WARN] SetEnvironmentVariable(User) failed; falling back to setx: $_" -ForegroundColor Yellow
    $currentVal = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
    if ($currentVal -ne $vcpkgRoot) {
        & setx VCPKG_ROOT "$vcpkgRoot" | Out-Null
    }
}

# (c) Current process
$env:VCPKG_ROOT = $vcpkgRoot
Write-Host "  [OK] Set VCPKG_ROOT in current process" -ForegroundColor Green

Write-Host "`n=== Setup complete ===" -ForegroundColor Cyan
Write-Host "  VCPKG_ROOT=$env:VCPKG_ROOT" -ForegroundColor Cyan
Write-Host ""
Write-Host "If running locally: close this shell, open a new one, then run:" -ForegroundColor Cyan
Write-Host "  .\scripts\win\build-windows-msvc.ps1" -ForegroundColor Cyan
Write-Host ""
Write-Host "If running under GitHub Actions: proceed to the next step." -ForegroundColor Cyan