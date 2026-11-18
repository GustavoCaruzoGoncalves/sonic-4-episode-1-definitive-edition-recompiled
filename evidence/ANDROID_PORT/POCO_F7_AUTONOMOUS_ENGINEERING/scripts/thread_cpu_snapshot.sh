#!/system/bin/sh
# On-device helper (push to /data/local/tmp). Prints one line per thread of the
# Sonic 4 process: tid|comm|on_cpu_ns|voluntary_cs|nonvoluntary_cs
P=$(pidof com.s4ep1recomp.sonic4)
[ -z "$P" ] && exit 1
for t in /proc/$P/task/*; do
  tid=${t##*/}
  comm=$(cat $t/comm 2>/dev/null)
  set -- $(cat $t/schedstat 2>/dev/null)
  ns=$1
  v=$(grep '^voluntary_ctxt_switches' $t/status 2>/dev/null | awk '{print $2}')
  n=$(grep '^nonvoluntary_ctxt_switches' $t/status 2>/dev/null | awk '{print $2}')
  echo "$tid|$comm|$ns|$v|$n"
done
