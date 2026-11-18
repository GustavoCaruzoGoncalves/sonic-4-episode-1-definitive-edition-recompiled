#!/usr/bin/env bash
# 20 s first-stage window: per-thread CPU (cores) + context switches, plus the
# PROBE lines (PACE / RENDER_VK / VBLANK) logged during the window.
# Usage: measure_threads.sh <label> [seconds]
set -u
export MSYS_NO_PATHCONV=1
LABEL=${1:?label}
SECS=${2:-20}
ADB=/c/Users/Administrator/Downloads/platform-tools-latest-windows/platform-tools/adb.exe
S=$($ADB devices | awk 'NR>1 && $2=="device" {print $1; exit}')
HERE=$(cygpath -m "$(cd "$(dirname "$0")" && pwd)")
OUT="$HERE/../raw/threads/$LABEL"
mkdir -p "$OUT"
$ADB -s "$S" push "$HERE/thread_cpu_snapshot.sh" /data/local/tmp/tcs.sh >/dev/null
LOG=$($ADB -s "$S" exec-out run-as com.s4ep1recomp.sonic4 ls -1t files/user/logs | tr -d '\r' | head -1)
T0=$($ADB -s "$S" shell 'cat /proc/uptime' | awk '{print $1}')
$ADB -s "$S" shell sh /data/local/tmp/tcs.sh | tr -d '\r' > "$OUT/snap0.txt"
sleep "$SECS"
$ADB -s "$S" shell sh /data/local/tmp/tcs.sh | tr -d '\r' > "$OUT/snap1.txt"
T1=$($ADB -s "$S" shell 'cat /proc/uptime' | awk '{print $1}')
$ADB -s "$S" exec-out run-as com.s4ep1recomp.sonic4 cat "files/user/logs/$LOG" | tr -d '\r' > "$OUT/full.log"
LINES=$(( SECS / 5 + 1 ))
{
  echo "label=$LABEL serial=$S log=$LOG window_s=$(awk -v a=$T0 -v b=$T1 'BEGIN{printf "%.2f", b-a}')"
  echo "-- per-thread (cores = on-cpu time / wall time)"
  awk -F'|' -v dt="$(awk -v a=$T0 -v b=$T1 'BEGIN{print b-a}')" '
    NR==FNR { ns[$1]=$3; v[$1]=$4; n[$1]=$5; next }
    ($1 in ns) { c=($3-ns[$1])/1e9/dt; tot+=c;
      if (c>0.01) printf "%-22s cores=%.3f vcs/s=%6.0f ivcs/s=%5.0f\n", $2, c, ($4-v[$1])/dt, ($5-n[$1])/dt }
    END { printf "TOTAL cores=%.3f\n", tot }' "$OUT/snap0.txt" "$OUT/snap1.txt" | sort -t= -k2 -nr
  echo "-- probes (last window)"
  grep -E "PROBE (PACE frames|RENDER_VK|VBLANK)" "$OUT/full.log" | tail -n $((LINES * 3))
} | tee "$OUT/summary.txt"
