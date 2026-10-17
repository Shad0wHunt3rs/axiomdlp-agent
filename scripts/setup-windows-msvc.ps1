<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio 2022 (or Build Tools) with the C++ workload.
    - Clones vcpkg into %USERPROFILE%\vcpkg if missing.
    - Bootstraps vcpkg.
    - Sets VCPKG_ROOT persistently for the current user.

.NOTES
    Run once per machine. Requires PowerShell 5.1+ (built-in) or 7+.
#>

$ErrorActionPreference = "Stop"

Write-Host "=== AxiomDLP Agent — Windows MSVC bootstrap ===" -ForegroundColor Cyan

# ----------------------------------------------------------------
# 1. Verify Visual Studio
# ----------------------------------------------------------------
Write-Host "`n[1/3] Looking for Visual Studio..." -ForegroundColor Yellow

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error @"
Visual Studio Installer not found.

Install one of:
  - Visual Studio 2022 Community (free)
  - Visual Studio 2022 Build Tools (free, headless)

Make sure to select the workload:
  "Desktop development with C++"

Download: https://visualstudio.microsoft.com/downloads/
"@
    exit 1
}

$vsInstall = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $vsInstall) {
    Write-Error "Visual Studio found but the C++ workload is missing. Add 'Desktop development with C++'."
    exit 1
}
Write-Host "  Found: $vsInstall" -ForegroundColor Green

# ----------------------------------------------------------------
# 2. Clone + bootstrap vcpkg
# ----------------------------------------------------------------
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "$env:USERPROFILE\vcpkg" }

Write-Host "`n[2/3] Checking vcpkg at $vcpkgRoot..." -ForegroundColor Yellow

if (-not (Test-Path "$vcpkgRoot\.git")) {
    Write-Host "  Cloning vcpkg (one-time, ~100 MB)..." -ForegroundColor Yellow
    git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
}

if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "  Bootstrapping vcpkg..." -ForegroundColor Yellow
    Push-Location $vcpkgRoot
    .\bootstrap-vcpkg.bat -disableMetrics
    Pop-Location
}

if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Error "vcpkg bootstrap failed; check $vcpkgRoot"
    exit 1
}
Write-Host "  vcpkg ready: $vcpkgRoot\vcpkg.exe" -ForegroundColor Green

# ----------------------------------------------------------------
# 3. Persist VCPKG_ROOT
# ----------------------------------------------------------------
Write-Host "`n[3/3] Setting VCPKG_ROOT=$vcpkgRoot for current user..." -ForegroundColor Yellow
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgRoot, "User")
$env:VCPKG_ROOT = $vcpkgRoot

Write-Host "`n=== Setup complete ===" -ForegroundColor Cyan
Write-Host "Re-open your shell, then run: .\scripts\build-windows-msvc.ps1" -ForegroundColor Cyan
