# ADB wireless — POCO F7

**Data:** 2026-11-17

## Descoberta (mDNS)

```
adb mdns services
→ adb-a63a397c-aBfeaA  _adb-tls-connect._tcp  192.168.70.7:44743
```

**Host PC:** Wi-Fi `192.168.70.11/24` (mesma sub-rede).

## Ligação

```text
adb connect 192.168.70.7:44743
adb devices -l
→ 192.168.70.7:44743  device  model:25053PC47G  (onyx / POCO F7)
```

**Serial a usar nos scripts:** `192.168.70.7:44743` (não `a63a397c` em wireless-only).

## Notas

- Pairing TLS só foi necessário via mDNS discover (já emparelhado no dispositivo).
- Telefone em carregador durante testes de bateria (`AC powered: true` em `dumpsys battery`).