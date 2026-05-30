# K6A Custom Kernel — AnyKernel3 Package

Flashable ZIP für OrangeFox / TWRP.

## Inhalt

- `Image.gz` — komprimierter Kernel (4.14.356-openela-rc1)
- `dtb.img` — Base Device Tree (xiaomi-sdmmagpie.dtb)
- `dtbo.img` — Device Tree Overlay (sweet-sdmmagpie-overlay.dtbo)

## Änderungen

- LMH DCVS deaktiviert (early return in `limits_mitigation_notify`)
- Kein Frequenz-Cap mehr durch thermische Limits
- Userspace `scaling_max_freq` wird respektiert

## Flash

1. ZIP aufs Gerät kopieren
2. In OrangeFox / TWRP: Install → ZIP auswählen
3. Reboot

## Build

CI läuft automatisch bei jedem Push auf `23.2`.
