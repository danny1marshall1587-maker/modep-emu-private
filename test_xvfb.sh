#!/usr/bin/env bash
set -e

Xvfb :99 -screen 0 1024x768x24 &
XVFB_PID=$!
sleep 1
export DISPLAY=:99

reaper /var/modep/recordings/Patchbox_Prime_Live.RPP &
REAPER_PID=$!

sleep 2
xdotool key Return
sleep 1
xdotool key Return
sleep 2

xdotool key ctrl+p
sleep 2

# Scroll tree to bottom
xdotool mousemove 60 150 click 1
sleep 0.5
xdotool key End
sleep 0.5
xdotool key Up
sleep 1

# Tab navigation
# In REAPER Preferences dialog, Tab moves through controls.
# Let's see what has focus or press Tab several times.
xdotool key Tab
sleep 0.2
xdotool key Tab
sleep 0.2
xdotool key space
sleep 1

import -window root /tmp/reaper_tab_space.png

kill -9 $REAPER_PID || true
kill -9 $XVFB_PID || true
