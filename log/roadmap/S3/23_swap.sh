#!/bin/bash
# after the old 23_census.sh finishes the batch it holds next (its 23:28 queue place), stop it and continue with 23_census2.sh
cd /c/Users/maria/Desktop/Proyectos/SimGolf/log/roadmap/S3
until [ "$(grep -c '^{"rows"' 23_census_run.log)" -ge 5 ]; do sleep 10; done
until [ ! -f w8_heavy.lock/owner.txt ] || ! grep -q "^S3.23" w8_heavy.lock/owner.txt; do sleep 5; done
powershell -NoProfile -Command "Get-CimInstance Win32_Process | Where-Object { \$_.CommandLine -match '23_census.sh' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }"
rm -f w8_wait_S3.23
echo "swapped to 23_census2.sh $(date +%H:%M)" >> 23_census_run.log
cd /c/Users/maria/Desktop/Proyectos/SimGolf && exec bash log/roadmap/S3/23_census2.sh >> log/roadmap/S3/23_census_run.log 2>&1
