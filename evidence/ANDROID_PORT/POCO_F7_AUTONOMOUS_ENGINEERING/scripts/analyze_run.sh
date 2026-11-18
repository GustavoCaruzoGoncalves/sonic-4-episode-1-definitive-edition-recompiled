#!/usr/bin/env bash
# Summarize a gameplay run log (telemetry build): frame-time distribution from
# PROBE FRAMES windows (n>150 frames, i.e. gameplay) and spike attribution from
# PROBE SPIKE lines. Usage: analyze_run.sh <full.log> [label]
F=${1:?log}
echo "== ${2:-$F}"
grep "PROBE FRAMES" "$F" | sed 's/.*PROBE FRAMES //' | awk '
{ for (i = 1; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
  if (v["n"] > 150) { n++; fr += v["n"]; g20 += v["gt20ms"]; g33 += v["gt33ms"]; g50 += v["gt50ms"];
    p50 += v["p50_us"]; p90 += v["p90_us"]; p99 += v["p99_us"]; gu += v["guest_us_avg"];
    pso += v["pso"]; psm += v["pso_ms"]; ww += v["wwf"]; wm += v["wwf_ms"]; ql += v["qlock_ms"];
    pr += v["present_ms"]; aq += v["acq_ms"]; tl += v["tex_load"]; if (v["max_us"] > mx) mx = v["max_us"] } }
END { if (!n) { print "no gameplay windows"; exit }
  printf "windows=%d frames=%d gt20=%.1f%% gt33=%.2f%% gt50=%.2f%% p50=%.1fms p90=%.1fms p99=%.1fms max=%.0fms guest_avg=%.1fms\n",
    n, fr, 100*g20/fr, 100*g33/fr, 100*g50/fr, p50/n/1000, p90/n/1000, p99/n/1000, mx/1000, gu/n/1000
  printf "per second: pso=%.2f (%.0fms) wwf=%.0f (%.0fms) qlock=%.0fms present=%.0fms acquire=%.0fms tex_load=%.0f\n",
    pso/(n*5), psm/(n*5), ww/(n*5), wm/(n*5), ql/(n*5), pr/(n*5), aq/(n*5), tl/(n*5) }'
grep "PROBE SPIKE" "$F" | sed 's/.*PROBE SPIKE //' | awk '
{ for (i = 1; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
  n++; it += v["interval_us"]; g += v["guest_us"]; su += v["submit_us"]; ql += v["qlock_us"];
  pr += v["present_us"]; ps += v["pso_us"]; id += v["idraw_us"] }
END { if (n) printf "spikes=%d avg: interval=%.1fms guest=%.1fms submit=%.1fms qlock=%.1fms present=%.1fms pso=%.1fms idraw=%.1fms\n",
  n, it/n/1000, g/n/1000, su/n/1000, ql/n/1000, pr/n/1000, ps/n/1000, id/n/1000 }'
