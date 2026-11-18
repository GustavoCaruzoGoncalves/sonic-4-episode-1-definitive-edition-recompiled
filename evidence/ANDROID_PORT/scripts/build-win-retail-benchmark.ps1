$ErrorActionPreference = 'Stop'

# Build a matched Windows x64 Release binary with the Android retail hot-path
# configuration: REX_DIAG_PROFILE=OFF (SDK install) and REX_TIMING_PROBES=OFF.

$Root = 'C:\S4EP1Recomp'
$LLVM = 'C:/Program Files/LLVM/bin'
$Ninja = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$SdkInstall = Join-Path $Root 'third_party\rexglue-sdk\out\install\win-amd64'
$Pilot = Join-Path $Root 'pilot'
$Build = Join-Path $Pilot 'out\build\win-amd64-perf-retail'
$Out = Join-Path $Root 'evidence\ANDROID_PORT\raw\windows\bin'

$env:Path = "$LLVM;" + (Split-Path $Ninja) + ';' + $env:Path
$Vcvars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
if (Test-Path $Vcvars) {
  cmd /c "`"$Vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') { Set-Item -Path "env:$($matches[1])" -Value $matches[2] }
  }
}
$MsvcLink = (Get-Command link.exe -ErrorAction SilentlyContinue).Source
if (-not $MsvcLink) { throw 'link.exe not found after vcvars64' }

Push-Location $Pilot
try {
  cmake --preset win-amd64-release -B $Build `
    -DCMAKE_PREFIX_PATH="$SdkInstall" `
    -DCMAKE_MAKE_PROGRAM="$Ninja" `
    -DCMAKE_C_COMPILER="$LLVM/clang.exe" `
    -DCMAKE_CXX_COMPILER="$LLVM/clang++.exe" `
    -DCMAKE_RC_COMPILER="$LLVM/llvm-rc.exe" `
    -DCMAKE_LINKER="$MsvcLink" `
    -DCMAKE_LINKER_TYPE=MSVC `
    -DREX_TIMING_PROBES=OFF
  if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }

  cmake --build $Build --config Release
  if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)" }
}
finally { Pop-Location }

New-Item -ItemType Directory -Force -Path $Out | Out-Null
Copy-Item (Join-Path $Build 'sonic4.exe') $Out -Force
Copy-Item (Join-Path $SdkInstall 'bin\rexruntime.dll') $Out -Force
Copy-Item (Join-Path $SdkInstall 'bin\rexgpu-xenos.dll') $Out -Force

Write-Host "OK: $(Join-Path $Out 'sonic4.exe')"
