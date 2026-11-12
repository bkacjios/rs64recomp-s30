#!/bin/sh
# Headless perf test: no picture yet. Presses START every few seconds to get through the title and menus.
# Output: one [s30-perf] / [s30-cpu] line a second in /tmp/rs64.log.
cd /mnt/sd/home/ports/rs64 || exit 1
P=""
t=8000
while [ $t -le 90000 ]; do P="$P$t:1000,$((t+150)):0,"; t=$((t+3000)); done
S30_PRESS="${P%,}" ./roguesquadron . 2>&1 | tee /tmp/rs64.log
