param(
  [string]$Serial = '192.168.70.7:44743',
  [string]$Label = 'workload_gated',
  [int]$Seconds = 20,
  [int]$IntervalSec = 5,
  [string]$LogFile = ''
)

$ErrorActionPreference = 'Stop'
$adb = 'C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe'
$outDir = Join-Path (Split-Path $PSScriptRoot -Parent) "raw\power\$Label"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Get-LatestLog {
  $name = & $adb -s $Serial exec-out run-as com.s4ep1recomp.sonic4 ls -1t files/user/logs 2>$null |
    Where-Object { $_ -match '^sonic4_\d+\.log$' } | Select-Object -First 1
  if ($LogFile) { return $LogFile }
  return $name
}

function Parse-RenderVkLine([string]$Line) {
  $o = [ordered]@{}
  foreach ($key in @(
      'present_hz','draws_per_present','draws/s','barriers/s','rendering/s',
      'record_us_pf','submit_us_pf','last_present_draws','max_present_draws')) {
    if ($Line -match "$key=([\d.]+)") { $o[$key] = [double]$matches[1] }
  }
  return [pscustomobject]$o
}

function Test-FirstStageWorkload([pscustomobject]$Rv) {
  if (-not $Rv) { return $false }
  if ($Rv.draws_per_present -lt 175 -or $Rv.draws_per_present -gt 205) { return $false }
  if ($Rv.'draws/s' -lt 5500) { return $false }
  if ($Rv.'barriers/s' -lt 6500) { return $false }
  if ($Rv.last_present_draws -lt 120) { return $false }
  return $true
}

$meta = @(
  "label=$Label",
  "start_iso=$((Get-Date).ToString('o'))",
  "serial=$Serial",
  "gate=first_stage draws_per_present 175-205 draws/s>=5500 barriers/s>=6500 last_present_draws>=120"
) -join "`n"
$meta | Set-Content (Join-Path $outDir 'validation_meta.txt') -Encoding utf8

$samples = New-Object System.Collections.Generic.List[object]
$end = (Get-Date).AddSeconds($Seconds)
$log = Get-LatestLog
$rejected = 0

while ((Get-Date) -lt $end) {
  $log = Get-LatestLog
  if (-not $log) { Start-Sleep $IntervalSec; continue }
  $line = (& $adb -s $Serial exec-out run-as com.s4ep1recomp.sonic4 grep 'PROBE RENDER_VK' "files/user/logs/$log" 2>$null | Select-Object -Last 1)
  $rv = Parse-RenderVkLine ($line -join '')
  $paceLine = (& $adb -s $Serial exec-out run-as com.s4ep1recomp.sonic4 tail -n 30 "files/user/logs/$log" 2>$null |
    Select-String 'PACE_821C0770/s=([\d.]+).*frame_us=(\d+)' | Select-Object -Last 1)
  $pace = $null
  $frameUs = $null
  if ($paceLine) {
    $pace = [double]$paceLine.Matches[0].Groups[1].Value
    $frameUs = [int]$paceLine.Matches[0].Groups[2].Value
  }
  $ok = Test-FirstStageWorkload $rv
  if (-not $ok) { $rejected++ }
  $samples.Add([pscustomobject]@{
      ts = (Get-Date).ToString('o')
      log = $log
      workload_ok = $ok
      record_us_pf = $rv.record_us_pf
      draws_per_present = $rv.draws_per_present
      draws_s = $rv.'draws/s'
      barriers_s = $rv.'barriers/s'
      present_hz = $rv.present_hz
      pace_hz = $pace
      frame_us = $frameUs
      raw = ($line -join '').Trim()
    })
  Start-Sleep -Seconds $IntervalSec
}

$valid = @($samples | Where-Object { $_.workload_ok -and $_.record_us_pf })
$summary = [ordered]@{
  sample_count = $samples.Count
  rejected_workload = $rejected
  valid_count = $valid.Count
  verdict = 'INCONCLUSIVE'
  log_file = $log
}

if ($valid.Count -ge 3) {
  $rec = ($valid | ForEach-Object { $_.record_us_pf } | Sort-Object)
  $mid = [int][math]::Floor($rec.Count / 2)
  $summary.median_record_us_pf = if ($rec.Count % 2) { $rec[$mid] } else { ($rec[$mid - 1] + $rec[$mid]) / 2 }
  $summary.mean_record_us_pf = ($valid | Measure-Object -Property record_us_pf -Average).Average
  $summary.median_draws_per_present = ($valid | Measure-Object -Property draws_per_present -Average).Average
  $summary.median_pace_hz = ($valid | Where-Object { $_.pace_hz } | Measure-Object -Property pace_hz -Average).Average
  $summary.verdict = 'COMPARABLE'
} elseif ($valid.Count -gt 0) {
  $summary.verdict = 'PARTIAL'
}

$samples | Export-Csv (Join-Path $outDir 'render_vk_samples.csv') -NoTypeInformation
($summary.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) |
  Set-Content (Join-Path $outDir 'summary.txt') -Encoding utf8

Write-Host "Label=$Label verdict=$($summary.verdict) valid=$($summary.valid_count)/$($samples.Count) log=$log"
if ($summary.median_record_us_pf) {
  Write-Host "median_record_us_pf=$($summary.median_record_us_pf)"
}