param(
  [Parameter(Mandatory = $true)][ValidateSet('probe', 'retail')][string]$Variant,
  [int]$SteadySeconds = 150,
  [string]$Label = '',
  [int]$Width = 2772,
  [int]$Height = 1280
)

$ErrorActionPreference = 'Stop'
$Root = 'C:\S4EP1Recomp'
$RunDir = Join-Path $Root "evidence\ANDROID_PORT\raw\windows\run_$Variant"
$Shots = Join-Path $Root 'evidence\ANDROID_PORT\raw\windows\shots'
$Summarizer = Join-Path $Root 'evidence\ANDROID_PORT\tools\summarize_probe_log.py'
New-Item -ItemType Directory -Force -Path $Shots | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
if (-not $Label) { $Label = $stamp }
$log = Join-Path $RunDir "logs\${Variant}_$Label.log"
$shotMenu = Join-Path $Shots "${Variant}_${Label}_00_menu.png"
$shotStart = Join-Path $Shots "${Variant}_${Label}_01_gameplay_start.png"
$shotEnd = Join-Path $Shots "${Variant}_${Label}_02_gameplay_end.png"

# Reset the user data to the identical POCO save snapshot before every run.
$saveSource = Join-Path $Root 'evidence\ANDROID_PORT\raw\poco\user_from_device\user'
$userDir = Join-Path $RunDir 'user'
if (Test-Path $userDir) { Remove-Item $userDir -Recurse -Force }
Copy-Item $saveSource $userDir -Recurse -Force
$deviceLogs = Join-Path $userDir 'logs'
if (Test-Path $deviceLogs) { Remove-Item $deviceLogs -Recurse -Force }

$appArgs = @(
  '--game_data_root=C:\S4EP1Recomp\game\extracted',
  "--user_data_root=$userDir",
  '--gpu_plugin=xenos',
  '--gpu_backend=vulkan',
  '--license_mask=1',
  "--log_file=$log",
  '--mnk_mode=true',
  '--fullscreen=false',
  '--vsync=true',
  "--window_width=$Width",
  "--window_height=$Height",
  "--video_mode_width=$Width",
  "--video_mode_height=$Height"
)

Write-Host "Launching $Variant -> $log"
# Let the previous window/input focus settle before starting the next game.
Start-Sleep -Seconds 5
$proc = Start-Process -FilePath (Join-Path $RunDir 'sonic4.exe') -ArgumentList $appArgs -WorkingDirectory $RunDir -PassThru
try {
  Start-Sleep -Seconds 18
  & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-capture.ps1') $shotMenu
  & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-keys.ps1') -ProcessId $proc.Id -Keys ' '
  Start-Sleep -Seconds 6
  & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-keys.ps1') -ProcessId $proc.Id -Keys ' '
  Start-Sleep -Seconds 6
  & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-keys.ps1') -ProcessId $proc.Id -Keys ' '
  Start-Sleep -Seconds 12
  & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-capture.ps1') $shotStart
  Write-Host "Steady gameplay measurement: $SteadySeconds s"
  Start-Sleep -Seconds $SteadySeconds
  & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-capture.ps1') $shotEnd
}
finally {
  if (-not $proc.HasExited) {
    & (Join-Path $Root 'evidence\ANDROID_PORT\scripts\win-keys.ps1') -ProcessId $proc.Id -Keys '%{F4}' -DelayMs 100
    Start-Sleep -Seconds 3
  }
  if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
  # Keep stale Alt+F4/SendKeys focus from leaking into the next benchmark run.
  Start-Sleep -Seconds 5
}

Write-Host "=== launch config $Variant $Label ==="
Write-Host "requested window/video mode: ${Width}x${Height}"
Select-String -Path $log -Pattern 'VulkanPresenter: Created .* swapchain' | Select-Object -First 1 | ForEach-Object { $_.Line }
Write-Host "=== summary $Variant $Label ==="
python $Summarizer $log
if ($Variant -eq 'probe') {
  Write-Host '=== scene workload (last 5 windows) ==='
  Select-String -Path $log -Pattern 'PROBE SCENEDISP_821A8488/s=' | Select-Object -Last 5 | ForEach-Object { $_.Line }
  Write-Host '=== diag VK (last window) ==='
  Select-String -Path $log -Pattern 'PROBE DIAG_VK ' | Select-Object -Last 1 | ForEach-Object { $_.Line }
}
Write-Host "OK: $log"
