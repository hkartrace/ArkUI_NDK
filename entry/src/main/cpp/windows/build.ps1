$ErrorActionPreference = "Stop"

$LLVM = "C:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\native\llvm\bin"
$WINSDK_LIB = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22000.0"
$WINSDK_INC = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22000.0"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$CPP_DIR = Split-Path -Parent $SCRIPT_DIR
$OUT = "$SCRIPT_DIR\out"

Write-Host "[1/4] Creating output directory..."
New-Item -ItemType Directory -Force -Path $OUT | Out-Null

Write-Host "[2/4] Compiling napi_init_minimal.cpp..."
& "$LLVM\clang++.exe" -target x86_64-pc-windows-msvc `
    -fno-exceptions -fno-rtti -c `
    -o "$OUT\napi_init_minimal.obj" `
    "$CPP_DIR\napi_init_minimal.cpp"
if ($LASTEXITCODE -ne 0) { Write-Host "COMPILE FAILED"; exit 1 }

Write-Host "[3/4] Generating import library from ace_napi.def..."
& "$LLVM\lld-link.exe" /DEF:"$SCRIPT_DIR\ace_napi.def" `
    /OUT:"$OUT\ace_napi.lib" /MACHINE:X64
if ($LASTEXITCODE -ne 0) { Write-Host "IMPORT LIB FAILED"; exit 1 }

Write-Host "[4/4] Linking libentry.dll..."
& "$LLVM\lld-link.exe" /DLL /ENTRY:DllMain `
    "/OUT:$OUT\libentry.dll" `
    "/LIBPATH:$WINSDK_LIB\um\x64" `
    "/LIBPATH:$WINSDK_LIB\ucrt\x64" `
    "$OUT\napi_init_minimal.obj" `
    "$OUT\ace_napi.lib" `
    kernel32.lib ucrt.lib
if ($LASTEXITCODE -ne 0) { Write-Host "LINK FAILED"; exit 1 }

Write-Host ""
Write-Host "SUCCESS: $OUT\libentry.dll"
Write-Host ""
Write-Host "Verifying imports..."
& "$LLVM\llvm-readobj.exe" --coff-imports "$OUT\libentry.dll"
