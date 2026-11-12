<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio (2022) with the C++ workload.
    - Downloads a PINNED vcpkg registry (2024.11.16) as a ZIP. This is
      the last 2024 release and the newest one that still calls
      find_program(NINJA) first (pre-z_use_vcpkg_fetch). It also fully
      supports VS 2022 with the MSVC 14.44 toolset used by the
      windows-2022 runner image.
    - Downloads a matching prebuilt vcpkg.exe. Tries several release
      tags in order, then falls back to bootstrap-vcpkg.bat.
    - Retries every download up to 5 times with exponential backoff.
    - Validates magic bytes on every download:
         * ZIP (vcpkg registry)  must start with 0x50 0x4B ("PK")
         * EXE (vcpkg-tool)      must start with 0x4D 0x5A ("MZ")
      A 404 on github.com returns an HTML page with status 200 through
      a redirect; the magic byte check catches that and retries.
    - Verifies vcpkg.exe runs and the recipe folder is complete.
    - Persists VCPKG_ROOT for the current workflow (GITHUB_ENV), the
      current user (registry), and the current process.

    WHY PIN 2024.11.16:
      * vcpkg 2024.01.12 does NOT know about MSVC 14.44 (shipped on
        windows-2022 since ~April 2024) and passes an older toolset
        version to vcvarsall.bat, which exits 255.
      * vcpkg 2025.01.13+ routes tool acquisition through
        z_use_vcpkg_fetch, which triggers the `ninja -v` bug.
      * 2024.11.16 sits between the two: knows 14.44, still calls
        find_program(NINJA) first.
    The Ninja `-v` regression is also neutralised by
    VCPKG_KEEP_ENV_VARS=PATH and VCPKG_FORCE_SYSTEM_BINARIES=1
    (see the workflow build steps).

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
#>

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------
# PINNED VCPKG VERSION
#
# - $VCPKG_TAG      : registry release (used for the ZIP and the
#                     cache key in the workflow).
# - $VCPKG_TOOL_TAGS: release tags on microsoft/vcpkg-tool. Tried in
#                     order. `latest` is a safe final fallback.
# ----------------------------------------------------------------
$VCPKG_TAG       = "2024.11.16"
$VCPKG_TOOL_TAGS = @("2024-11-16", "2024-10-18", "latest")

Write-Host "=== AxiomDLP Agent - Windows MSVC bootstrap (pinned vcpkg $VCPKG_TAG) ===" -ForegroundColor Cyan

# ----------------------------------------------------------------
# Magic byte validator
# ----------------------------------------------------------------
function Test-MagicBytes {
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][int[]]$Expected
    )

    if (-not (Test-Path $FilePath)) { return $false }

    $stream = $null
    try {
        $stream = [System.IO.File]::OpenRead($FilePath)
        $actual = New-Object byte[] $Expected.Count
        $read = $stream.Read($actual, 0, $Expected.Count)
        if ($read -lt $Expected.Count) { return $false }
        for ($i = 0; $i -lt $Expected.Count; $i++) {
            if ($actual[$i] -ne $Expected[$i]) { return $false }
        }
        return $true
    }
    finally {
        if ($stream) { $stream.Close() }
    }
}

# ----------------------------------------------------------------
# Retry helper
# ----------------------------------------------------------------
function Download-WithRetry {
    param(
        [Parameter(Mandatory=$true)][string]$Url,
        [Parameter(Mandatory=$true)][string]$OutFile,
        [int[]]$ExpectedMagicBytes = @(),
        [int]$MaxAttempts = 5,
        [int]$BaseDelaySeconds = 3
    )

    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        try {
            Write-Host "  Downloading $Url (attempt $attempt/$MaxAttempts)..." -ForegroundColor Yellow

            Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing -TimeoutSec 300

            if (-not (Test-Path $OutFile) -or (Get-Item $OutFile).Length -eq 0) {
                throw "Downloaded file is missing or empty: $OutFile"
            }

            if ($ExpectedMagicBytes.Count -gt 0) {
                if (-not (Test-MagicBytes -FilePath $OutFile -Expected $ExpectedMagicBytes)) {
                    $size = (Get-Item $OutFile).Length
                    throw "Downloaded file does not start with expected magic bytes (size=$size). Likely an HTML error page."
                }
            }

            Write-Host "  [OK] Downloaded $((Get-Item $OutFile).Length) bytes" -ForegroundColor Green
            return
        }
        catch {
            Write-Host "  [WARN] Attempt $attempt failed: $_" -ForegroundColor Yellow

            if (Test-Path $OutFile) {
                Remove-Item $OutFile -Force -ErrorAction SilentlyContinue
            }

            if ($attempt -lt $MaxAttempts) {
                $delay = $BaseDelaySeconds * [math]::Pow(2, $attempt - 1)
                Write-Host "  Retrying in $delay seconds..." -ForegroundColor Yellow
                Start-Sleep -Seconds $delay
            }
            else {
                throw
            }
        }
    }
}

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
# 2. vcpkg recipes (ZIP) + prebuilt vcpkg.exe  (both PINNED)
# ----------------------------------------------------------------
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "$env:USERPROFILE\vcpkg" }
$vcpkgZip  = "$env:USERPROFILE\vcpkg.zip"
$toolchain = "$vcpkgRoot\scripts\buildsystems\vcpkg.cmake"

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
        Write-Host "`n[2/4] Existing vcpkg at $vcpkgRoot (tag='$existingTag') is not pinned to $VCPKG_TAG; replacing..." -ForegroundColor Yellow
    }
}

if ($needsInstall) {
    Write-Host "`n[2/4] Setting up pinned vcpkg $VCPKG_TAG at $vcpkgRoot..." -ForegroundColor Yellow

    # ------------------------------------------------------------
    # Recipe ZIP  (magic bytes: "PK")
    # ------------------------------------------------------------
    $zipUrl = "https://codeload.github.com/microsoft/vcpkg/zip/refs/tags/$VCPKG_TAG"
    Download-WithRetry -Url $zipUrl -OutFile $vcpkgZip -ExpectedMagicBytes @(0x50, 0x4B)

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

    $VCPKG_TAG | Out-File -FilePath "$vcpkgRoot\.vcpkg-tag" -Encoding ascii -NoNewline

    # ------------------------------------------------------------
    # vcpkg.exe  (magic bytes: "MZ")
    # ------------------------------------------------------------
    $toolDownloaded = $false
    $usedTag = $null
    foreach ($tag in $VCPKG_TOOL_TAGS) {
        $exeUrl = "https://github.com/microsoft/vcpkg-tool/releases/download/$tag/vcpkg.exe"
        try {
            Download-WithRetry -Url $exeUrl -OutFile "$vcpkgRoot\vcpkg.exe" -ExpectedMagicBytes @(0x4D, 0x5A)
            $toolDownloaded = $true
            $usedTag = $tag
            break
        }
        catch {
            Write-Host "  [WARN] vcpkg-tool tag '$tag' failed; trying next fallback..." -ForegroundColor Yellow
            if (Test-Path "$vcpkgRoot\vcpkg.exe") {
                Remove-Item "$vcpkgRoot\vcpkg.exe" -Force -ErrorAction SilentlyContinue
            }
        }
    }

    if (-not $toolDownloaded) {
        Write-Host "  All explicit tags failed; running bootstrap-vcpkg.bat..." -ForegroundColor Yellow
        Push-Location $vcpkgRoot
        try {
            & ".\bootstrap-vcpkg.bat" -disableMetrics
            if ($LASTEXITCODE -ne 0) {
                throw "bootstrap-vcpkg.bat returned exit code $LASTEXITCODE"
            }
        }
        finally {
            Pop-Location
        }
        if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
            Write-Host "[ERROR] bootstrap-vcpkg.bat did not produce vcpkg.exe" -ForegroundColor Red
            exit 1
        }
        $usedTag = "bootstrap"
    }

    Write-Host "  [OK] vcpkg.exe ready (source: $usedTag)" -ForegroundColor Green
}

# ----------------------------------------------------------------
# 3. Verify vcpkg installation
# ----------------------------------------------------------------
Write-Host "`n[3/4] Verifying vcpkg installation..." -ForegroundColor Yellow

if (-not (Test-Path "$vcpkgRoot\vcpkg.exe")) {
    Write-Host "[ERROR] vcpkg.exe missing at $vcpkgRoot" -ForegroundColor Red; exit 1
}
if (-not (Test-Path $toolchain)) {
    Write-Host "[ERROR] vcpkg toolchain file missing at $toolchain" -ForegroundColor Red; exit 1
}
if (-not (Test-Path "$vcpkgRoot\ports")) {
    Write-Host "[ERROR] vcpkg ports directory missing at $vcpkgRoot\ports" -ForegroundColor Red; exit 1
}
if (-not (Test-Path "$vcpkgRoot\triplets")) {
    Write-Host "[ERROR] vcpkg triplets directory missing at $vcpkgRoot\triplets" -ForegroundColor Red; exit 1
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
    Write-Host "[ERROR] vcpkg.exe failed to execute: $_" -ForegroundColor Red; exit 1
}

Write-Host "  [OK] vcpkg ready: $vcpkgRoot (pinned $VCPKG_TAG)" -ForegroundColor Green

# ----------------------------------------------------------------
# 4. Persist VCPKG_ROOT
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