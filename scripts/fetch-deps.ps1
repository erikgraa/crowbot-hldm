# Fetches the HLSDK and Metamod-r headers into include\ (not committed to this repo).
$root = Split-Path $PSScriptRoot -Parent
$tmp = Join-Path $env:TEMP "crowbot-metamod-src"
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
git clone --depth 1 https://github.com/theAsmodai/metamod-r.git $tmp
if ($LASTEXITCODE -ne 0) { exit 1 }
$inc = Join-Path $root "include"
New-Item -ItemType Directory -Force $inc | Out-Null
Copy-Item "$tmp\metamod\extra\example\include\*" $inc -Recurse -Force
Remove-Item $tmp -Recurse -Force
Write-Host "Headers installed to $inc"
