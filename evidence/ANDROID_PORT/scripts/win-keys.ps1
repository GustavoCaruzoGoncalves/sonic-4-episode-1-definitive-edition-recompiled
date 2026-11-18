param(
  [Parameter(Mandatory = $true)][int]$ProcessId,
  [Parameter(Mandatory = $true)][string]$Keys,
  [int]$DelayMs = 250
)
Add-Type -AssemblyName Microsoft.VisualBasic, System.Windows.Forms
$process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
if (-not $process) { throw "Process $ProcessId is not running" }
$activated = $false
for ($attempt = 0; $attempt -lt 10 -and -not $activated; $attempt++) {
  try {
    [Microsoft.VisualBasic.Interaction]::AppActivate($ProcessId) | Out-Null
    $activated = $true
  }
  catch {
    Start-Sleep -Milliseconds 500
  }
}
if (-not $activated) { throw "Could not activate process $ProcessId" }
Start-Sleep -Milliseconds 300
foreach ($chunk in ($Keys -split '\|')) {
  [System.Windows.Forms.SendKeys]::SendWait($chunk)
  Start-Sleep -Milliseconds $DelayMs
}
