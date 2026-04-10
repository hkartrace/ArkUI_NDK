$ErrorActionPreference = "Stop"

$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$CPP_DIR = Split-Path -Parent $SCRIPT_DIR
$PROJECT_DIR = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $CPP_DIR)))
$DLL_PATH = "$SCRIPT_DIR\out\libentry.dll"
$HAP_SRC = "$PROJECT_DIR\entry\build\default\outputs\default\entry-default-unsigned.hap"
$HAP_DST = "$SCRIPT_DIR\out\entry-preview.hap"
$STAGING = "$SCRIPT_DIR\out\staging"

if (-not (Test-Path $DLL_PATH)) {
    Write-Host "ERROR: libentry.dll not found at $DLL_PATH"
    Write-Host "Run build.ps1 first."
    exit 1
}

if (-not (Test-Path $HAP_SRC)) {
    Write-Host "ERROR: Source .hap not found at $HAP_SRC"
    Write-Host "Build the project in DevEco Studio first."
    exit 1
}

Write-Host "[1/4] Cleaning staging directory..."
if (Test-Path $STAGING) { Remove-Item -Recurse -Force $STAGING }
New-Item -ItemType Directory -Force -Path "$STAGING\libs\x86-64" | Out-Null

Write-Host "[2/4] Extracting original .hap..."
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($HAP_SRC, $STAGING)

Write-Host "[3/4] Adding libentry.dll as libs/x86-64/libentry.so..."
Copy-Item $DLL_PATH "$STAGING\libs\x86-64\libentry.so" -Force

Write-Host "[4/4] Packaging into entry-preview.hap..."
if (Test-Path $HAP_DST) { Remove-Item -Force $HAP_DST }
[System.IO.Compression.ZipFile]::CreateFromDirectory($STAGING, $HAP_DST)

Remove-Item -Recurse -Force $STAGING

$hapSize = (Get-Item $HAP_DST).Length / 1KB
Write-Host ""
Write-Host "SUCCESS: $HAP_DST ($([math]::Round($hapSize, 1)) KB)"
Write-Host ""
Write-Host "Contains libs/x86-64/libentry.so for patched previewer."
Write-Host "Usage: Previewer.exe -gui -hap entry-preview.hap"
