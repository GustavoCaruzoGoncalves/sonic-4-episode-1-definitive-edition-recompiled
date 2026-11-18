# Vulkan / Adreno 825 — resumo vivo

**Ver também:** `../ISSUE_DRAW_ROOT_CAUSE.md`, `../VULKAN_CPU_RECORDING_INVESTIGATION.md`.

- Driver sistema (não Turnip) no POCO F7 retail.
- Threads `GPU Commands` / `GPU VSync` aparecem no perfil com `CommandProcessor::WriteRegister`, `MMIOHandler`, mutex kernel.
- IssueDraw (diag): `vertex_fetch` e `primitive` dominam CPU Vulkan — investigação retail em andamento.
- Referência Xendroid+: FIFO presentation, resolves diretos — candidatos futuros **com benchmark**.