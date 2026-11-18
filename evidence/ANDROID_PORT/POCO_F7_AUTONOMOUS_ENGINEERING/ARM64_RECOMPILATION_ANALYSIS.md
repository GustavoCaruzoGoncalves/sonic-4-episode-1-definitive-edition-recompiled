# Análise ARM64 recompilada — resumo vivo

**Ver também:** `../ARM64_CODEGEN_CORRELATION_REPORT.md`, `CPU_EXECUTION_ARCHITECTURE_AUDIT.md`.

- **12 864** funções guest → símbolos `sub_*` em `libmain.so`.
- Mix típico hot path: alta densidade de `ldr`/`str`/`rev` (endian PPC).
- Hotspots dinâmicos: `sub_821C62C8`, `sub_821C4870` (~13 % CPU em `simpleperf` 2026-11-17).
- **Sem** interpretador PPC; overhead é **código gerado** + helpers `__savegprlr_*`.