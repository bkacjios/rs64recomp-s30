#!/bin/sh
# Rogue Squadron (static recompilation): binary and rogue_squadron.z64 (USA v1.0) in /mnt/sd/home/ports/rs64.
# Log in /tmp/rs64.log. Optional debug environment in rs64.env (VAR=value lines).
[ -f /mnt/sd/home/ports/rs64/rs64.env ] && set -a && . /mnt/sd/home/ports/rs64/rs64.env && set +a
cd /mnt/sd/home/ports/rs64 && exec ./roguesquadron . > /tmp/rs64.log 2>&1
