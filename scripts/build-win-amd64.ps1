# Build ReXGlue SDK + sonic4 pilot (Windows x64 Release).
# Use from any shell:  powershell -ExecutionPolicy Bypass -File C:\S4EP1Recomp\scripts\build-win-amd64.ps1

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $Root "pilot\CMakeLists.txt"))) {
    $Root = "C:\S4EP1Recomp"
}

$LLVM = "C:/Program Files/LLVM/bin"
$Ninja = "C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
$Sdk = Join-Path $Root "third_party\rexglue-sdk"
$SdkBuild = Join-Path $Sdk "out\build\win-amd64"
$SdkInstall = Join-Path $Sdk "out\install\win-amd64"
$Pilot = Join-Path $Root "pilot"
$PilotBuild = Join-Path $Pilot "out\build\win-amd64-release"

foreach ($p in @("$LLVM/clang.exe", "$LLVM/clang++.exe", "$LLVM/llvm-rc.exe", $Ninja)) {
    if (-not (Test-Path $p)) {
        throw "Ferramenta ausente: $p"
    }
}

$env:Path = ($LLVM -replace "/", "\") + ";" + (Split-Path $Ninja) + ";" + $env:Path

$Vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $Vcvars) {
    Write-Host "==> Carregando ambiente MSVC (vcvars64)..." -ForegroundColor Cyan
    cmd /c "`"$Vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
}

$MsvcLink = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
if (-not $MsvcLink) {
    throw "link.exe nao encontrado no PATH. Instale VS 2022 Build Tools ou rode vcvars64."
}
Write-Host "==> Linker MSVC: $MsvcLink" -ForegroundColor DarkGray

Write-Host "==> Configurando SDK (win-amd64)..." -ForegroundColor Cyan
Push-Location $Sdk
try {
    cmake --preset win-amd64 `
        -DCMAKE_MAKE_PROGRAM="$Ninja" `
        -DCMAKE_C_COMPILER="$LLVM/clang.exe" `
        -DCMAKE_CXX_COMPILER="$LLVM/clang++.exe" `
        -DCMAKE_RC_COMPILER="$LLVM/llvm-rc.exe" `
        -DCMAKE_LINKER="$MsvcLink" `
        -DCMAKE_LINKER_TYPE=MSVC `
        -DREXGLUE_ENABLE_TRACY=OFF
    if ($LASTEXITCODE -ne 0) { throw "cmake configure SDK falhou ($LASTEXITCODE)" }

    Write-Host "==> Compilando SDK Release..." -ForegroundColor Cyan
    cmake --build $SdkBuild --config Release
    if ($LASTEXITCODE -ne 0) { throw "cmake build SDK falhou ($LASTEXITCODE)" }

    Write-Host "==> Instalando SDK..." -ForegroundColor Cyan
    cmake --install $SdkBuild --config Release
    if ($LASTEXITCODE -ne 0) { throw "cmake install SDK falhou ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}

Write-Host "==> Configurando pilot (win-amd64-release)..." -ForegroundColor Cyan
Push-Location $Pilot
try {
    cmake --preset win-amd64-release `
        -DCMAKE_PREFIX_PATH="$SdkInstall" `
        -DCMAKE_MAKE_PROGRAM="$Ninja" `
        -DCMAKE_C_COMPILER="$LLVM/clang.exe" `
        -DCMAKE_CXX_COMPILER="$LLVM/clang++.exe" `
        -DCMAKE_RC_COMPILER="$LLVM/llvm-rc.exe" `
        -DCMAKE_LINKER="$MsvcLink" `
        -DCMAKE_LINKER_TYPE=MSVC
    if ($LASTEXITCODE -ne 0) { throw "cmake configure pilot falhou ($LASTEXITCODE)" }

    Write-Host "==> Compilando sonic4 Release..." -ForegroundColor Cyan
    cmake --build $PilotBuild --config Release
    if ($LASTEXITCODE -ne 0) { throw "cmake build pilot falhou ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}

$Out = Join-Path $PilotBuild "sonic4.exe"
Copy-Item (Join-Path $SdkInstall "bin\rexruntime.dll") $PilotBuild -Force
Copy-Item (Join-Path $SdkInstall "bin\rexgpu-xenos.dll") $PilotBuild -Force -ErrorAction SilentlyContinue
if (Test-Path (Join-Path $SdkBuild "Release\rexgpu-xenos.dll")) {
    Copy-Item (Join-Path $SdkBuild "Release\rexgpu-xenos.dll") $PilotBuild -Force
}

Write-Host ""
Write-Host "OK: $Out" -ForegroundColor Green
Write-Host "     $(Get-Item $Out | Select-Object -ExpandProperty LastWriteTime)"
