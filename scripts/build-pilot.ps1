# Pilot-only build helper (Windows x64 Release).
# Loads the MSVC environment (needed so link.exe can find rc.exe for the
# embedded manifest) and then runs ninja in the existing pilot build dir.
# Usage: powershell -ExecutionPolicy Bypass -File C:\S4EP1Recomp\scripts\build-pilot.ps1

$ErrorActionPreference = "Stop"

$LLVM = "C:\Program Files\LLVM\bin"
$Ninja = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$PilotBuild = "C:\S4EP1Recomp\pilot\out\build\win-amd64-release"

$env:Path = $LLVM + ";" + (Split-Path $Ninja) + ";" + $env:Path

$Vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $Vcvars) {
    Write-Host "==> Loading MSVC env (vcvars64)..."
    cmd /c "`"$Vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match "^(.*?)=(.*)$") {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
}

Write-Host "==> Building pilot..."
& $Ninja -C $PilotBuild
Write-Host "==> ninja exit: $LASTEXITCODE"
if ($LASTEXITCODE -ne 0) { throw "pilot build failed ($LASTEXITCODE)" }
