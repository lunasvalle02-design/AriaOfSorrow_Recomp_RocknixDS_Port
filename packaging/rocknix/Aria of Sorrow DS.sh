#!/bin/bash
# Castlevania: Aria of Sorrow — dual-screen static recompilation port.
# ROCKNIX launcher (Anbernic RG DS / RG DS Plus). Lives in /storage/roms/ports.
#
# The game itself is NOT included: copy your own legally dumped
# "Castlevania - Aria of Sorrow (USA).gba" into the ariaofsorrowds/ folder.

GAMEDIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)/ariaofsorrowds"
LOG="$GAMEDIR/log.txt"
cd "$GAMEDIR" || exit 1
exec > "$LOG" 2>&1
echo "=== $(date) ==="

ROM="$GAMEDIR/Castlevania - Aria of Sorrow (USA).gba"
if [ ! -f "$ROM" ]; then
  # Accept any single .gba the user dropped in (it is SHA-1 checked anyway).
  CANDIDATE="$(ls "$GAMEDIR"/*.gba 2>/dev/null | head -n 1)"
  [ -n "$CANDIDATE" ] && ROM="$CANDIDATE"
fi
if [ ! -f "$ROM" ]; then
  echo "ROM not found. Copy 'Castlevania - Aria of Sorrow (USA).gba' to $GAMEDIR"
  exit 1
fi

# Controller mappings shipped by ROCKNIX, when SDL is not already given one.
if [ -z "$SDL_GAMECONTROLLERCONFIG" ]; then
  for db in /storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt \
            /usr/config/SDL-GameControllerDB/gamecontrollerdb.txt; do
    if [ -f "$db" ]; then export SDL_GAMECONTROLLERCONFIG_FILE="$db"; break; fi
  done
fi

# Under sway both panels are separate outputs; the game spans them with one
# window and draws the game on DSI-2 (top) and the map/status/menu on the
# touch panel DSI-1. Override with ARIA_DS_GAME_OUTPUT if your panels are named otherwise.
export ARIA_DS_GAME_OUTPUT="${ARIA_DS_GAME_OUTPUT:-DSI-2}"
if [ -n "$SWAYSOCK" ] || [ -n "$WAYLAND_DISPLAY" ]; then
  export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-wayland}"
fi

# Give the recompiled game the fast cores' full clock while it runs.
if [ -w /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
  OLD_GOV="$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
  echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
fi

chmod +x ./aria_recomp
./aria_recomp --handheld --rom "$ROM"
STATUS=$?

[ -n "$OLD_GOV" ] && echo "$OLD_GOV" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
echo "exit status $STATUS"
exit $STATUS
