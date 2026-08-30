# BadazzKernel — AnyKernel3 Package

Flashable ZIP für OrangeFox / TWRP (sweet / sweetin).

## Inhalt

- `Image.gz` — komprimierter Kernel (4.14.369, k6a_gov v1.3.1)
- `dtb.img` — Base Device Tree (xiaomi-sdmmagpie.dtb)
- `dtbo.img` — Overlay (sweet-sdmmagpie-overlay.dtbo)

## Features im Image

- **k6a_gov v1.3.1** built-in (`CONFIG_K6A_GOV=y`) — CPU/GPU/BW Thermal Governor, 6 Profile, Battery Guard
- **KSU-Next 33300** + **SUSFS** — Root + Hide
- **Sched/Mem/Net**: UCLAMP, SCHED_CASS, KSM, LRU_GEN, ZRAM lz4, BBR, BOEFFLA_WL_BLOCKER

Details: `/sys/kernel/k6a_gov/status`

## Flash

1. ZIP aufs Gerät kopieren
2. OrangeFox / TWRP → Install → ZIP
3. Reboot
4. Optional: [k6a-ctl](https://github.com/vandalsquad187/k6a-ctl) installieren (`delegated=1`)

## Build

CI baut bei jedem Push auf `main` automatisch ein Release. Lokal siehe `../README.md`.

## Debug

```bash
dmesg | grep k6a_gov
cat /sys/kernel/k6a_gov/status
```
