<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio (2022 or newer) with the C++ workload.
    - Downloads a PINNED vcpkg release (2024.01.12) as a ZIP.
    - Downloads the matching prebuilt vcpkg.exe from the same date.
    - Verifies vcpkg.exe runs and the recipe folder is complete.
    - Persists VCPKG_ROOT for the current workflow (GITHUB_ENV), the
      current user (registry), and the current process.

    WHY PIN: recent vcpkg snapshots (late 2025 onward) route all tool
    acquisition through `z_use_vcpkg_fetch`, which on Windows downloads
    Ninja and then verifies it by running `ninja -v` (verbose build)
    instead of `ninja --version` (print version). In the empty
    `vcpkg-parallel-configure` directory this exits 1, and vcpkg
    treats it as a broken Ninja download and aborts the build with:

        Command failed: ...\ninja.exe -v
        Error code: 1

    vcpkg tag 2024.01.12 predates this regression. It checks
    find_program(NINJA) first, so with VCPKG_FORCE_SYSTEM_BINARIES=1
    and the Chocolatey Ninja on PATH, compiler detection succeeds.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
#>

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------
# PINNED VCPKG VERSION
# ----------------------------------------------------------------
$VCPKG_TAG      = "2024.01.12"
$VCPKG_TOOL_TAG = "2024-01-12"

Write-Host "=== AxiomDLP Agent - Windows MSVC bootstrap (pinned vcpkg $VCPKG_TAG) ===" -ForegroundColor Cyan

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
  - Any newer Visual Studio (2026+ also works with this pipeline)

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
# 2. vcpkg recipes (ZIP) + prebuilt vcpkg.exe  (both PINNED)
# ----------------------------------------------------------------
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "$env:USERPROFILE\vcpkg" }
$vcpkgZip  = "$env:USERPROFILE\vcpkg.zip"
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"

# Detect an existing tree that does NOT match the pinned tag and rebuild.
$needsInstall = $true
if (Test-Path $toolchain) {
    $existingTag = $null
    $tagFile = "$vcpkgRoot\.vcpkg-tag"
    if (Test-Path $tagFile) {
        $existingTag = (Get-Content $tagFile -Raw).Trim()
    }
    if ($existingTag -eq $VCPKG_TAG) {
        Write-Host "`n[2/4] Pinned vcpkg $VCPKG_TAG already present at $vcpkgRoot" -ForegroundColor Green
        $needsInstall = $false
    }
    else {
        Write-Host "`n[2/4] Existing vcpkg at $vcpkgRoot is not pinned to $VCPKG_TAG; replacing..." -ForegroundColor Yellow
    }
}

if ($needsInstall) {
    Write-Host "`n[2/4] Setting up pinned vcpkg $VCPKG_TAG at $vcpkgRoot..." -ForegroundColor Yellow

    Write-Host "  Downloading vcpkg recipes ZIP (tag $VCPKG_TAG)..." -ForegroundColor Yellow
    $zipUrl = "https://codeload.github.com/microsoft/vcpkg/zip/refs/tags/$VCPKG_TAG"
    Invoke-WebRequest -Uri $zipUrl -OutFile $vcpkgZip -UseBasicParsing

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

    # Marker so subsequent runs don't re-download.
    $VCPKG_TAG | Out-File -FilePath "$vcpkgRoot\.vcpkg-tag" -Encoding ascii -NoNewline

    Write-Host "  Downloading prebuilt vcpkg.exe (tag $VCPKG_TOOL_TAG, no compilation)..." -ForegroundColor Yellow
    $exeUrl = "https://github.com/microsoft/vcpkg-tool/releases/download/$VCPKG_TOOL_TAG/vcpkg.exe"
    Invoke-WebRequest -Uri $exeUrl -OutFile "$vcpkgRoot\vcpkg.exe" -UseBasicParsing
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
    exit 1
}
if (-not (Test-Path "$vcpkgRoot\triplets")) {
    Write-Host "[ERROR] vcpkg triplets directory missing at $vcpkgRoot\triplets" -ForegroundColor Red
    exit 1
}

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

Write-Host "  [OK] vcpkg ready: $vcpkgRoot (pinned $VCPKG_TAG)" -ForegroundColor Green

# ----------------------------------------------------------------
# 4. Persist VCPKG_ROOT  (three mechanisms: GITHUB_ENV, registry, process)
# ----------------------------------------------------------------
Write-Host "`n[4/4] Setting VCPKG_ROOT=$vcpkgRoot..." -ForegroundColor Yellow

if ($env:GITHUB_ENV) {
    Add-Content -Path $env:GITHUB_ENV -Value "VCPKG_ROOT=$vcpkgRoot"
    Write-Host "  [OK] Wrote VCPKG_ROOT to GITHUB_ENV" -ForegroundColor Green
}
else {
    Write-Host "  [--] GITHUB_ENV not set; skipping" -ForegroundColor DarkGray
}

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

$env:VCPKG_ROOT = $vcpkgRoot
Write-Host "  [OK] Set VCPKG_ROOT in current process" -ForegroundColor Green

Write-Host "`n=== Setup complete (vcpkg $VCPKG_TAG) ===" -ForegroundColor Cyan
Write-Host "  VCPKG_ROOT=$env:VCPKG_ROOT" -ForegroundColor Cyan