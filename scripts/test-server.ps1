param([int]$Seconds = 120, [int]$Quota = 4, [string]$Map = "crossfire")
# Rebuilds the plugin, restarts the local test HLDS (deps\hlds) and prints bot activity.
$root = Split-Path $PSScriptRoot -Parent
Get-Process hlds -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.Id }
Start-Sleep 2
cmd /c "$root\build.bat" 2>&1 | Select-Object -Last 2
if ($LASTEXITCODE -ne 0) { exit 1 }
$addon = "$root\deps\hlds\valve\addons\crowbot"
New-Item -ItemType Directory -Force $addon | Out-Null
Copy-Item "$root\build\crowbot.dll" "$addon\" -Force
Set-Content "$root\deps\hlds\valve\addons\metamod\plugins.ini" "win32 addons/crowbot/crowbot.dll"
Set-Content "$root\deps\hlds\valve\server.cfg" "hostname crowbot-test`nmp_teamplay 0`nsv_lan 1`nmp_timelimit 0`nmp_fraglimit 0`nlog on`ncrowbot_skill 3`ncrowbot_debug 1`ncrowbot_quota $Quota`n"
Set-Location "$root\deps\hlds"
Start-Process .\hlds.exe -ArgumentList "-console -game valve +maxplayers 8 +map $Map +sv_lan 1 +exec server.cfg -insecure -nomaster" -RedirectStandardOutput "$root\deps\hlds_out.txt" -WindowStyle Hidden | Out-Null
Start-Sleep $Seconds
Get-Content "$root\deps\hlds_out.txt" | Select-String 'entered|suicide|killed|\[dbg\]|rror' | ForEach-Object Line | Select-Object -Last 60
