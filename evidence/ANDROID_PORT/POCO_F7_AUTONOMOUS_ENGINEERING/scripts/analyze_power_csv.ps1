param([string]$CsvPath)

$csv = Import-Csv $CsvPath

function Get-Stats([double[]]$arr) {

  if (-not $arr -or $arr.Count -eq 0) { return $null }

  $s = $arr | Sort-Object

  $mid = ($s[[int](($s.Count - 1) / 2)] + $s[[int]($s.Count / 2)]) / 2

  [pscustomobject]@{

    n     = $arr.Count

    min   = [math]::Round(($s | Measure-Object -Minimum).Minimum, 3)

    max   = [math]::Round(($s | Measure-Object -Maximum).Maximum, 3)

    avg   = [math]::Round(($s | Measure-Object -Average).Average, 3)

    median = [math]::Round($mid, 3)

    stdev = if ($arr.Count -gt 1) {

      $avg = ($arr | Measure-Object -Average).Average

      [math]::Round([math]::Sqrt((($arr | ForEach-Object { ($_ - $avg) * ($_ - $avg) } | Measure-Object -Sum).Sum) / ($arr.Count - 1)), 3)

    } else { 0 }

  }

}

$pace = @($csv | Where-Object { $_.pace_hz } | ForEach-Object { [double]$_.pace_hz })

$ft = @($csv | Where-Object { $_.frame_us } | ForEach-Object { [int]$_.frame_us })

$gpu = @($csv | ForEach-Object { [double]$_.gpu_hz })

$cur = @($csv | ForEach-Object { [double]$_.current_uA })

$volt = @($csv | ForEach-Object { [double]$_.voltage_uV })

$gpct = [System.Collections.Generic.List[double]]::new()

for ($i = 1; $i -lt $csv.Count; $i++) {

  $db = [double]$csv[$i].gpubusy_busy - [double]$csv[$i - 1].gpubusy_busy

  $dt = [double]$csv[$i].gpubusy_total - [double]$csv[$i - 1].gpubusy_total

  if ($dt -gt 0) { $gpct.Add(100.0 * $db / $dt) }

}

$pwr = @($csv | ForEach-Object { ([double]$_.current_uA * [double]$_.voltage_uV) / 1e9 })

$gratio = @($csv | ForEach-Object {
  $t = [double]$_.gpubusy_total
  if ($t -gt 0) { 100.0 * [double]$_.gpubusy_busy / $t }
})

@{

  pace_hz             = Get-Stats $pace

  frame_us            = Get-Stats ($ft | ForEach-Object { [double]$_ })

  gpu_hz              = Get-Stats $gpu

  current_uA          = Get-Stats $cur

  voltage_uV          = Get-Stats $volt

  gpubusy_pct_5s      = Get-Stats @($gpct)
  gpubusy_lifetime_pct = Get-Stats $gratio

  power_mW_IxV        = Get-Stats $pwr

  valid_pace_samples  = $pace.Count

  total_csv_rows      = $csv.Count

} | ConvertTo-Json -Depth 4


