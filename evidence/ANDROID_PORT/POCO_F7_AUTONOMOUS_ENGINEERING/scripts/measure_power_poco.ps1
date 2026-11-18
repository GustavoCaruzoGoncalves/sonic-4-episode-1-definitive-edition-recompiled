param(

  [string]$Serial = '192.168.70.7:44743',

  [string]$Label = 'power_sample',

  [int]$Seconds = 20,

  [int]$IntervalSec = 5,

  [string]$LogFile = ''

)



$adb = 'C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe'

$outDir = Join-Path (Split-Path $PSScriptRoot -Parent) "raw\power\$Label"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null



$startIso = (Get-Date).ToString('o')

$metaPath = Join-Path $outDir 'collection_meta.txt'

@(

  "start_iso=$startIso",

  "serial=$Serial",

  "duration_s=$Seconds",

  "interval_s=$IntervalSec",

  "label=$Label",

  "log_file_pref=$LogFile"

) | Set-Content $metaPath -Encoding utf8



function Get-BatteryFields {

  param([string[]]$Dump)

  $status = ''

  $ac = ''

  $lev = ''

  $temp = ''

  foreach ($line in $Dump) {

    if ($line -match '^\s*status:\s*(\d+)') { $status = $matches[1] }

    if ($line -match '^\s*AC powered:\s*(true|false)') { $ac = $matches[1] }

    if ($line -match '^\s*level:\s*(\d+)') { $lev = $matches[1] }

    if ($line -match '^\s*temperature:\s*(\d+)') { $temp = $matches[1] }

  }

  [pscustomobject]@{ status = $status; ac = $ac; level = $lev; temp = $temp }

}



$rows = New-Object System.Collections.Generic.List[string]

$rows.Add('ts_epoch_s,current_uA,voltage_uV,gpu_hz,gpubusy_busy,gpubusy_total,temp_0.1C,level_pct,battery_status,ac_online,pace_hz,frame_us,log_file')



$end = (Get-Date).AddSeconds($Seconds)

while ((Get-Date) -lt $end) {

  $ts = [int][double]::Parse((Get-Date -UFormat %s))

  $cur = (& $adb -s $Serial shell "cat /sys/class/power_supply/battery/current_now" 2>$null | Select-Object -First 1).Trim()

  $volt = (& $adb -s $Serial shell "cat /sys/class/power_supply/battery/voltage_now" 2>$null | Select-Object -First 1).Trim()

  $batDump = @(& $adb -s $Serial shell "dumpsys battery" 2>$null)

  $bf = Get-BatteryFields $batDump

  $gpuLines = @(& $adb -s $Serial shell "cat /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq; cat /sys/class/kgsl/kgsl-3d0/gpubusy")

  $ghz = ($gpuLines | Where-Object { $_ -match '^\d+$' } | Select-Object -First 1)

  if ($ghz) { $ghz = $ghz.Trim() } else { $ghz = '' }

  $gbLine = ($gpuLines | Where-Object { $_ -match '^\s*\d+\s+\d+' } | Select-Object -First 1)

  $gb = if ($gbLine) { ($gbLine.Trim() -split '\s+') } else { @('', '') }



  $pace = ''

  $frameUs = ''

  $log = $LogFile

  if (-not $log) {

    $log = & $adb -s $Serial exec-out run-as com.s4ep1recomp.sonic4 ls -1t files/user/logs 2>$null |

      Where-Object { $_ -match '^sonic4_\d+\.log$' } | Select-Object -First 1

  }

  if ($log) {

    $lt = & $adb -s $Serial exec-out run-as com.s4ep1recomp.sonic4 tail -n 20 "files/user/logs/$log" 2>$null

    $paceLine = $lt | Select-String 'PACE_821C0770/s=([\d.]+)' | Select-Object -Last 1

    if ($paceLine) { $pace = $paceLine.Matches[0].Groups[1].Value }

    $frameLine = $lt | Select-String 'frame_us=(\d+)' | Select-Object -Last 1

    if ($frameLine) { $frameUs = $frameLine.Matches[0].Groups[1].Value }

  }



  $rows.Add("$ts,$cur,$volt,$ghz,$($gb[0]),$($gb[1]),$($bf.temp),$($bf.level),$($bf.status),$($bf.ac),$pace,$frameUs,$log")

  Start-Sleep -Seconds $IntervalSec

}



$endIso = (Get-Date).ToString('o')

Add-Content $metaPath "end_iso=$endIso"



$csv = Join-Path $outDir 'power_timeseries.csv'

$rows | Set-Content $csv -Encoding utf8

Write-Host "Wrote $csv"


