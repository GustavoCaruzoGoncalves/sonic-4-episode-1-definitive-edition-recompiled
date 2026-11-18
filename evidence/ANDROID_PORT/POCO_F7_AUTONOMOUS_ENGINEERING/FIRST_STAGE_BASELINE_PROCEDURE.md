# Procedimento padrão — 1.ª fase, Sonic parado

**Cena fixa:** primeira fase, Sonic **sem movimento**, câmera/ambiente estáveis.

**Utilizador:** abre a 1.ª fase manualmente após cada `am start` ou reinício.

**Agente:**

1. `adb -s a63a397c install -r android/dist/Sonic4-arm64-v8a.apk`
2. `adb shell am start -n com.s4ep1recomp.sonic4/.Sonic4Activity` (sem taps, sem `force-stop` salvo após install de `.so`)
3. Aguardar `PACE_821C0770` no log (timeout 5 min)
4. Após ≥1 janela PACE estável, medir **20 s** (padrão A/B — ver `MEASUREMENT_POLICY.md`); **120 s** só para baseline publicado ou térmico
5. Coletar: log `sonic4_*.log`, `dumpsys battery`, `kgsl-3d0` freq, screencap, `simpleperf` 60 s (`raw/baseline_first_stage/<label>/`)
6. Não misturar com outras cenas nos relatórios A/B

**Build retail:** `REX_DIAG_PROFILE=OFF`, `REX_TIMING_PROBES=OFF` (hooks retail com PROBE PACE apenas).