$ErrorActionPreference = 'Stop'

$Root = 'C:\S4EP1Recomp'
$Evidence = Join-Path $Root 'evidence\ANDROID_PORT'
$RawWindows = Join-Path $Evidence 'raw\windows'
$SaveSource = Join-Path $Evidence 'raw\poco\user_from_device\user'
$ProbeSrc = Join-Path $Root 'pilot\out\build\win-amd64-release'
$RetailSrc = Join-Path $RawWindows 'bin'

$runs = @(
  @{ Name = 'probe'; Source = $ProbeSrc },
  @{ Name = 'retail'; Source = $RetailSrc }
)

foreach ($run in $runs) {
  $dir = Join-Path $RawWindows ("run_" + $run.Name)
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  New-Item -ItemType Directory -Force -Path (Join-Path $dir 'logs') | Out-Null

  foreach ($file in @('sonic4.exe', 'rexruntime.dll', 'rexgpu-xenos.dll')) {
    Copy-Item (Join-Path $run.Source $file) (Join-Path $dir $file) -Force
  }

  $user = Join-Path $dir 'user'
  if (Test-Path $user) { Remove-Item $user -Recurse -Force }
  Copy-Item $SaveSource $user -Recurse -Force
  # Keep only the save/achievement state; logs from the device copy are not needed.
  $deviceLogs = Join-Path $user 'logs'
  if (Test-Path $deviceLogs) { Remove-Item $deviceLogs -Recurse -Force }

  $logFile = Join-Path $dir ("logs\" + $run.Name + ".log")
  $config = @(
    "# isolated benchmark config for $($run.Name)",
    "log_file = `"$logFile`"",
    'fullscreen = false',
    'window_width = 2772',
    'window_height = 1280',
    'video_mode_width = 2772',
    'video_mode_height = 1280',
    'vsync = true',
    'game_data_root = "C:\S4EP1Recomp\game\extracted"',
    "user_data_root = `"$user`"",
    'gpu_plugin = "xenos"',
    'gpu_backend = "vulkan"',
    'license_mask = 1',
    'mnk_mode = true'
  ) -join "`n"
  Set-Content -Path (Join-Path $dir 'sonic4.toml') -Value $config -Encoding UTF8
  Write-Host "OK: $dir"
}
