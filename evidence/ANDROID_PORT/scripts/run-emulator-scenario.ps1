param(
  [int]$SteadySeconds = 150,
  [string]$Label = 'emu_bench1',
  [string]$Serial = 'emulator-5556'
)

$ErrorActionPreference = 'Stop'
$adb = 'C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe'
$package = 'com.s4ep1recomp.sonic4'
$raw = 'C:\S4EP1Recomp\evidence\ANDROID_PORT\raw\emulator\native_x86_64'
$shots = Join-Path $raw 'shots'
New-Item -ItemType Directory -Force -Path $raw, $shots | Out-Null

function Adb([string[]]$AdbArgs) { & $adb -s $Serial @AdbArgs }

function Snap([string]$Name) {
  $path = Join-Path $shots "${Label}_$Name.png"
  cmd /c "`"$adb`" -s $Serial exec-out screencap -p > `"$path`""
  Write-Host "screenshot: $path"
}

function State([string]$Name) {
  $path = Join-Path $raw "${Label}_$Name.txt"
  cmd /c "`"$adb`" -s $Serial shell `"echo ===PROP===; getprop ro.product.cpu.abi; getprop ro.product.model; echo ===TOP===; top -n 1 2>/dev/null | head -n 15`" > `"$path`""
  Write-Host "state: $path"
}

function NewestLog() {
  (Adb @('exec-out', 'run-as', $package, 'ls -1t files/user/logs') |
    Where-Object { $_ -match '^sonic4_\d+\.log$' } |
    Select-Object -First 1)
}

function WaitForLogPattern([string]$Pattern, [int]$TimeoutSeconds) {
  $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
  while ((Get-Date) -lt $deadline) {
    $log = NewestLog
    if ($log) {
      $text = (Adb @('exec-out', 'run-as', $package, "cat files/user/logs/$log")) -join "`n"
      if ($text -match $Pattern) { return $log }
    }
    Start-Sleep -Seconds 5
  }
  return $null
}

function AppAlive() {
  [bool](Adb @('shell', "pidof $package") | Where-Object { $_ -match '^\d+$' })
}

Adb @('shell', 'am force-stop', $package) | Out-Null
Start-Sleep -Seconds 2
Adb @('shell', 'am start -n com.s4ep1recomp.sonic4/.Sonic4Activity') | Out-Null

$ready = WaitForLogPattern -Pattern 'VulkanPresenter: Created' -TimeoutSeconds 240
if (-not $ready) { throw 'App did not create a Vulkan swapchain within 240 s' }
Write-Host "app ready: $ready"
Start-Sleep -Seconds 10
Snap '00_menu'

foreach ($i in 1..3) {
  Adb @('shell', 'input tap 2545 1015') | Out-Null
  Start-Sleep -Seconds 8
}

$pacing = WaitForLogPattern -Pattern 'PACE_821C0770/s=' -TimeoutSeconds 180
if (-not $pacing) { throw 'No PACE telemetry after navigation; app may not be in gameplay' }
if (-not (AppAlive)) { throw 'App exited before steady measurement' }
Write-Host "gameplay telemetry: $pacing"

Start-Sleep -Seconds 18
Snap '01_gameplay_start'
State 'start'
if (-not (AppAlive)) { throw 'App exited before steady measurement' }

Write-Host "Steady gameplay measurement: $SteadySeconds s"
Start-Sleep -Seconds $SteadySeconds
if (-not (AppAlive)) { throw 'App exited during steady measurement' }

Snap '02_gameplay_end'
State 'end'
$logcat = Join-Path $raw "${Label}_logcat.txt"
cmd /c "`"$adb`" -s $Serial logcat -d -t 2000 > `"$logcat`""

$newest = NewestLog
if (-not $newest) { throw 'No app log found' }
$localLog = Join-Path $raw "${Label}_$newest"
cmd /c "`"$adb`" -s $Serial exec-out run-as $package cat files/user/logs/$newest > `"$localLog`""
Write-Host "app log: $localLog"
python 'C:\S4EP1Recomp\evidence\ANDROID_PORT\tools\summarize_probe_log.py' $localLog
Write-Host "OK: $Label"