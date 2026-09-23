#!/bin/sh
# SPDX-License-Identifier: Zlib
#
# Drives gesture-propagation: opens the window, drags the pointer from the
# first child down across the third, then asks the probe to report.
#
# Prefers Xvfb, so the drag does not fight the real cursor. Falls back to the
# running display only when told to (PROBE_USE_REAL_DISPLAY=1), because
# xdotool will move the actual pointer.

set -e
cd "$(dirname "$0")"

[ -x ./gesture-propagation ] || { echo "build it first: make gesture-propagation"; exit 2; }
command -v xdotool >/dev/null || { echo "xdotool is required"; exit 2; }

run_probe () {
    ./gesture-propagation >probe.out 2>probe.err &
    probe_pid=$!
    # wait for the window; xdotool search retries on its own
    # GTK also creates 1x1 helper windows; take the one with real geometry
    win=""
    for _ in 1 2 3 4 5 6 7 8 9 10 11 12; do
        for w in $(xdotool search --name gesture-propagation 2>/dev/null); do
            eval "$(xdotool getwindowgeometry --shell "$w" 2>/dev/null)"
            if [ "${WIDTH:-0}" -gt 100 ] && [ "${HEIGHT:-0}" -gt 100 ]; then
                win=$w
                break
            fi
        done
        [ -n "$win" ] && break
        sleep 0.4
    done
    [ -n "$win" ] || { echo "the window never showed up"; kill $probe_pid 2>/dev/null; exit 3; }

    xdotool windowactivate --sync "$win" 2>/dev/null || true
    eval "$(xdotool getwindowgeometry --shell "$win")"
    # press in child 0, cross child 1, release in child 2. The moves are
    # stepped so the parent sees several drag-update events, the way a real
    # drag would arrive.
    xdotool mousemove --sync "$((X + 200))" "$((Y + 60))"
    sleep 0.3
    xdotool mousedown 1
    sleep 0.2
    step=0
    while [ $step -lt 10 ]; do
        step=$((step + 1))
        xdotool mousemove --sync "$((X + 200 + step * 3))" "$((Y + 60 + step * 26))"
        sleep 0.12
    done
    xdotool mouseup 1
    sleep 0.3

    kill -USR1 $probe_pid
    wait $probe_pid
    rc=$?
    cat probe.out
    [ -s probe.err ] && { echo "--- stderr ---"; cat probe.err; }
    return $rc
}

if [ "$1" = "--inner" ]; then
    export GDK_BACKEND=x11
    run_probe
elif command -v Xvfb >/dev/null && command -v xvfb-run >/dev/null; then
    echo "using Xvfb"
    exec xvfb-run -a -s "-screen 0 800x900x24" "$0" --inner
elif [ "$PROBE_USE_REAL_DISPLAY" = "1" ]; then
    echo "WARNING: using the live display; the pointer will move on its own for ~2s"
    export GDK_BACKEND=x11
    run_probe
else
    cat <<'MSG'
Xvfb not found.

  Install:  sudo pacman -S xorg-server-xvfb     (Arch)
            sudo apt install xvfb               (Debian/Ubuntu)

  Or run on the current session, aware that the pointer will be moved:
            PROBE_USE_REAL_DISPLAY=1 ./run-gesture-probe.sh
MSG
    exit 2
fi
