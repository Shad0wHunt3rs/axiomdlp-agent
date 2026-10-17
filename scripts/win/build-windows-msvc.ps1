<#
.SYNOPSIS
    Build AxiomDLP Agent on Windows with MSVC + vcpkg.

.PARAMETER Configuration
    Debug or Release (default: Release).

.PARAMETER Arch
    x64 or x86 (default: x64).

.PARAMETER Clean
    If set, delete the build directory before configuring.

.EXAMPLE
    .\scripts\build-windows-msvc.ps1
    .\scripts\build-windows-msvc.ps1 -Configuration Debug
    .\scripts\build-windows-msvc.ps1 -Clean
#>

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "x86")]
    [string]$Arch = "x64",

    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $projectRoot

Write-Host "=== AxiomDLP Agent — Windows MSVC build ===" -ForegroundColor Cyan
Write-Host "  Project root: $projectRoot"
Write-Host "  Config:       $Configuration"
Write-Host "  Arch:         $Arch"

if (-not $env:VCPKG_ROOT) {
    Write-Error @"
VCPKG_ROOT is not set.

Run the one-time setup first:
    .\scripts\win\setup-windows-msvc.ps1

Then re-open your shell.
"@
    exit 1
}
if (-not (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
    Write-Error "vcpkg toolchain file not found at $env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
    exit 1
}
Write-Host "  VCPKG_ROOT:   $env:VCPKG_ROOT" -ForegroundColor Green

$presetName = "windows-$Arch-msvc-$(($Configuration).ToLower())"
$buildDir   = Join-Path $projectRoot "build\$presetName"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "  Cleaning $buildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "`n--- Configure (preset: $presetName) ---" -ForegroundColor Yellow
cmake --preset $presetName
if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed."; exit $LASTEXITCODE }

Write-Host "`n--- Build ---" -ForegroundColor Yellow
cmake --build --preset $presetName --config $Configuration
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed."; exit $LASTEXITCODE }

$exe = Join-Path $buildDir "axiomdlp-agent.exe"
if (Test-Path $exe) {
    $size = [math]::Round((Get-Item $exe).Length / 1MB, 2)
    Write-Host "`n=== Build succeeded ===" -ForegroundColor Cyan
    Write-Host "  Binary: $exe"
    Write-Host "  Size:   $size MB"
} else {
    Write-Warning "Build finished but $exe not found. Check the preset's binaryDir."
}
