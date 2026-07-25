$device = Get-PnpDevice -PresentOnly |
  Where-Object {
    $_.Class -eq "Ports" -and
    $_.InstanceId -match "VID_239A&PID_8029"
  } |
  Select-Object -First 1

if (-not $device) {
  Write-Host "RAK4631 USB serial device 239a:8029 was not found."
  exit 1
}

if ($device.FriendlyName -match "\((COM[0-9]+)\)") {
  $com = $Matches[1]
  Write-Host $com
  exit 0
}

Write-Host "Found device, but could not parse COM port:"
Write-Host $device.FriendlyName
exit 2
