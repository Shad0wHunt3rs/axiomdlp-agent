<#
.SYNOPSIS
    One-time setup for building AxiomDLP Agent on Windows with MSVC + vcpkg.

.DESCRIPTION
    - Verifies Visual Studio (2022 or newer) with the C++ workload.
    - Downloads a PINNED vcpkg registry (2024.01.12) as a ZIP.
    - Downloads a matching prebuilt vcpkg.exe. The matching vcpkg-tool
      release tag is 2024-01-11 (vcpkg-tool tags are one day earlier
      than registry tags for this release line).
    - Retries every download up to 5 times with exponential backoff.
    - Validates magic bytes on every download:
         * ZIP (vcpkg registry)  must start with 0x50 0x4B ("PK")
         * EXE (vcpkg-tool)      must start with 0x4D 0x5A ("MZ")
      A 404 on github.com returns an HTML page with status 200 through
      the redirect, so size checks alone are not sufficient. The magic
      byte check catches that case and triggers a retry.
    - Verifies vcpkg.exe runs and the recipe folder is complete.
    - Persists VCPKG_ROOT for the current workflow (GITHUB_ENV), the
      current user (registry), and the current process.

    WHY PIN 2024.01.12: vcpkg snapshots from mid-2025 onward route tool
    acquisition through z_use_vcpkg_fetch, which on Windows downloads
    Ninja and then verifies it by running `ninja -v` (verbose build)
    instead of `ninja --version`. In an empty directory that exits 1,
    and vcpkg aborts with "Command failed: ninja.exe -v".
    2024.01.12 predates this regression and calls find_program(NINJA)
    first, so with VCPKG_FORCE_SYSTEM_BINARIES=1 the system (Chocolatey)
    Ninja is used and the bug never fires.

.NOTES
    This file is intentionally ASCII-only for maximum encoding compatibility.
#>

$ErrorActionPreference = "Stop"

# ----------------------------------------------------------------
# PINNED VCPKG VERSION -- do not bump without re-verifying the Ninja
# acquisition path.
#
# - $VCPKG_TAG      : registry release (used for the ZIP and for the
#                     cache key in the workflow).
# - $VCPKG_TOOL_TAGS: release tags on microsoft/vcpkg-tool. Tried in
#                     order. 2024-01-11 matches registry 2024.01.12.
#                     2023-12-04 is a safe older fallback.
# ----------------------------------------------------------------
$VCPKG_TAG       = "2024.01.12"
$VCPKG_TOOL_TAGS = @("2024-01-11", "2023-12-04")

Write-Host "=== AxiomDLP Agent - Windows MSVC bootstrap (pinned vcpkg $VCPKG_TAG) ===" -ForegroundColor Cyan

# ----------------------------------------------------------------
# Magic byte validator -- reads only the first N bytes, cheap even
# for large files. Returns $true if the file starts with the expected
# bytes.
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
# Retry helper -- windows-latest runners occasionally drop github.com
# connections mid-transfer. Retry up to 5 times with exponential backoff.
# ----------------------------------------------------------------
function Download-WithRetry {
    param(
        [Parameter(Mandatory=$true)][string]$Url,
        [Parameter(Mandatory=$true)][string]$OutFile,
        [int[]]$ExpectedMagicBytes = @(),
        [int]$MaxAttempts = 5,
        [int]$BaseDelaySeconds = 3
    )

    # Force TLS 1.2 -- older PowerShell defaults to TLS 1.0 which
    # github.com rejects on some runner images.
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        try {
            Write-Host "  Downloading $Url (attempt $attempt/$MaxAttempts)..." -ForegroundColor Yellow

            # -UseBasicParsing avoids the IE engine dependency, which
            # is what throws "The connection was closed unexpectedly"
            # on headless runners.
            Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing -TimeoutSec 300

            if (-not (Test-Path $OutFile) -or (Get-Item $OutFile).Length -eq 0) {
                throw "Downloaded file is missing or empty: $OutFile"
            }

            # Magic byte check catches HTML error pages that GitHub
            # sometimes returns with status 200 via a redirect.
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
  - Any newer Visual Studio (2026+ also works)

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

# If the tree exists but is not our pinned tag, replace it.
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
    # Recipe ZIP  (magic bytes: "PK" = 0x50 0x4B)
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
    # vcpkg.exe  (magic bytes: "MZ" = 0x4D 0x5A)
    #
    # The vcpkg-tool release tag for registry 2024.01.12 is
    # 2024-01-11 (one day earlier -- vcpkg-tool tags use the UTC
    # date the tool was built, the registry uses the date the
    # registry snapshot was tagged). 2023-12-04 is a safe fallback
    # from the same release generation.
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
        Write-Host "[ERROR] All vcpkg-tool tag fallbacks failed: $($VCPKG_TOOL_TAGS -join ', ')" -ForegroundColor Red
        Write-Host "        Check https://github.com/microsoft/vcpkg-tool/releases for available tags." -ForegroundColor Red
        exit 1
    }

    Write-Host "  [OK] vcpkg.exe downloaded from tag $usedTag" -ForegroundColor Green
}

# ----------------------------------------------------------------
# 3. Verify vcpkg.exe runs and recipe folder is complete
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