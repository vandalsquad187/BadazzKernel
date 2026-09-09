<div align="center">
  <img src="assets/logo.png" alt="Badazz89" width="180"/>
  <h1>BadazzKernel</h1>
  <p>Gaming-optimized kernel for <b>SM7150 (sweet / sweet k6a)</b> — 4.14.369</p>
  <p>
    <img src="https://img.shields.io/badge/Kernel-4.14.369-blue?style=flat-square">
    <img src="https://img.shields.io/badge/k6a__gov-1.3.1-orange?style=flat-square">
    <img src="https://img.shields.io/badge/KernelSU--Next-33300-green?style=flat-square">
    <img src="https://img.shields.io/badge/SUSFS-yes-success?style=flat-square">
    <img src="https://img.shields.io/badge/Android-13%20%7C%2014-lightgrey?style=flat-square">
  </p>
</div>

---

## Overview

BadazzKernel is a performance and gaming tuned kernel for **Redmi Note 12 Pro 4G (sweet / sweetin)**. Core is the in-kernel governor **k6a_gov v1.3.1** — the perfect base for the companion module **[k6a-ctl](https://github.com/vandalsquad187/k6a-ctl)**. Both work hand-in-hand in delegated mode: kernel throttles, module controls.

🤌🏻 Join my Telegram channel: https://t.me/Badazz89

---

## Architecture

```
linux-4.14.369
├── KernelSU-Next 33300 (UAPIv2) + SUSFS v2.2.0  # Root + Hide (k6a-sweet a5ff54c)
├── drivers/thermal/k6a_gov/k6a_gov.c v1.3.1     # In-kernel gaming governor
├── drivers/gpu/msm/kgsl_pwrctrl.c               # GPU pwrlevel export (k6a_gov)
├── drivers/devfreq/devfreq.c                    # BW floors (gpubw / llcc)
├── drivers/gpu/msm/ + drivers/thermal/          # Thermal + cooling floors
├── drivers/usb/dwc3/ + phy/qcom                 # USB fix (SM6150 clocks, WAIT_FOR_LPM, bus_aggr)
└── arch/arm64/boot/dts/qcom/                    # sweet / sdmmagpie DT (AOSP + MIUI overlay)
```

### Kernel Features

| Area | Feature | Details |
|------|---------|---------|
| **Governor** | **k6a_gov v1.3.1** | Built-in (`CONFIG_K6A_GOV=y`), state machine OFF→GAMING→CD_L2/L3/L4, hysteresis, throttle history (16) |
| **CPU** | Gold clamp | `find_gold_cpu()` + `clamp_freq`, `enforce_max_freq` + `cpufreq_update_policy` (kthread only), hardened notifier |
| **Temp** | Multi-zone | Max over 4 Gold zones `cpu-1-0..3-usr`, fallback `xo-therm`/`soc-therm` |
| **GPU** | Native enforcement | `kgsl_k6a_get_levels` / `kgsl_k6a_set_max_level_idx`, caps per CD state |
| **BW** | Devfreq floors | `k6a_devfreq_set_bw` for `gpubw` + `cpu-llcc-ddr-bw`, per profile/CD state, reset on disable |
| **Profiles** | 6 profiles | `off/gaming/battery/badazz/custom/badazz_safe` — temps, Gold/GPU/BW floors |
| **Safety** | Battery guard + hash | `battery_guard` @45°C → CD_L2, `poll_ms` 100..5000, `verify_build_hash` vs `k6a_features/git_hash` |
| **Thermal** | Cooling device | `k6a_gov` as `thermal_cooling_device`, `cool_cur` locked, `K6A_CD_L4` clamp |
| **Root** | KSU-Next + SUSFS | 33300 UAPIv2, SUSFS `a5ff54c` `v2.2.0` (sus_path/mount/kstat/map), `tamper_syscall_table` |
| **USB** | DWC3 / QUSB2 fix | SM6150 clocks (`GCC/DISPCC/CAMCC`), `WAIT_FOR_LPM` clear, `bus_aggr`/`GCTL` 50ms, `is_a_peripheral` fix — no bootloop on PC |
| **Scheduler** | UCLAMP, SCHED_CASS | Latency/efficiency tuning |
| **Memory** | KSM, LRU_GEN, ZRAM lz4 | Gaming stability |
| **Net** | BBR | Low latency |
| **Wakelock** | BOEFFLA_WL_BLOCKER | Blocks wasteful wakelocks |
| **Monitor** | MSM_PERFORMANCE, PSI | `cpu_freq_times`, pressure stall |
| **NFC** | PN80T I2C driver | `CONFIG_NFC=n` intentional (NCI in userspace), `CONFIG_NFC_NQ_PN80T=y` provides `/dev/nq-nci` — fix via [Sweet2NfcFIX](https://github.com/vandalsquad187/Sweet2NfcFIX) module |
| **MIUI** | HyperOS overlay | `sweet_miui.config` overlay (`ARCH_SM6150/GCC/CAMCC/DISPCC/PDC`) + `xiaomi/sweet` DTS, unified AnyKernel, CI matrix `aosp`/`miui` |

---

## Companion: k6a-ctl

Module **[k6a-ctl v1.1.6](https://github.com/vandalsquad187/k6a-ctl)** ( -  `fix-susfs-88feb68` `766de04a` for 4.14) is the userspace companion:

```
k6a-ctl v1.1.6 (delegated=1)
├── k6a-controller   # Game detection, schedutil, auto badazz_safe @85°C
├── k6a-lib.sh       # gov_write/gov_read helpers
├── settings.conf    # delegated, profile, auto_badazz_temp
├── check_module.sh  # Delegation-aware gate
└── WebUI            # Gov status, BW floors, GPU caps, history
```

**Delegated mode** (`delegated=1`, default): kernel governor controls thermal (CPU/GPU/BW), module does game detection + sched tuning + monitoring. Legacy (`delegated=0`) is fallback only.

### Kernel interfaces (`/sys/kernel/k6a_gov/`)

| Node | R/W | Purpose |
|------|-----|---------|
| `enable` | RW | 0/1 — kill switch, resets BW floors + GPU cap |
| `profile` | RW | 0..5 — off/gaming/battery/badazz/custom/badazz_safe |
| `status` | RO | `version/state/temp/ticks/throttle_events/gold_* /bw_* /hash_verified/hist=` |
| `hysteresis` | RW | `fast normal` — dwell ms (e.g. `3 10`) |
| `cd_thresholds` | RW | `l2t l3t l4t rec l2g l3g l4g` — 7 values, Celsius |
| `gpu_caps` | RW | `l2 l3 l4` — GPU Hz caps |
| `bw_floors` | RW | `gpubw_l2 l3 l4 llcc_l2 l3 l4` — 6 BW floors |
| `battery_guard` | RW | 0/1 — battery guard 45°C |
| `poll_ms` | RW | 100..5000 — kthread interval |
| `legacy` | RW | 0/1 — enforcement on/off |
| `game_pid` | RW | PID of the game |

> **Kernel provides the levers — k6a-ctl pulls them.** Without module k6a_gov runs with safe defaults. With module it reacts dynamically to load/temperature.

---

## Installation

1. Download **Release** from [GitHub Releases](https://github.com/vandalsquad187/BadazzKernel/releases) (`Badazz-kernel-sweet-v4.14.369-buildXX.zip` or `-miui.zip` for HyperOS)
2. Flash ZIP via **OrangeFox / TWRP** (AnyKernel3)
3. Install **KernelSU-Next Manager** APK (use CI `34142634591` for now, `34252913097` requires pending `88feb68` 4.14 port)
4. (Recommended) Install **[k6a-ctl](https://github.com/vandalsquad187/k6a-ctl)** module — `delegated=1`

### Build (Dev)

```bash
git clone https://github.com/vandalsquad187/BadazzKernel && cd BadazzKernel
git submodule update --init --recursive
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- sweet_defconfig
# MIUI variant:
scripts/kconfig/merge_config.sh arch/arm64/configs/sweet_defconfig arch/arm64/configs/sweet_miui.config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
# ZIP:
cp arch/arm64/boot/Image.gz anykernel/ && cp arch/arm64/boot/dtb.img anykernel/ && cp arch/arm64/boot/dtbo.img anykernel/
cd anykernel && zip -r9 ../BadazzKernel-sweet-k6a-gov-v1.3.1.zip . -x "*.git*"
```

CI builds automatically on every push to `main` (release with build number) and to `miui/test` (artifacts only).

---

## Status & Debug

```bash
dmesg | grep k6a_gov          # v1.3.1 loaded, hash_verified, gpu levels
cat /sys/kernel/k6a_gov/status
cat /sys/kernel/k6a_gov/battery_guard; cat /sys/kernel/k6a_gov/poll_ms
echo 0 > /sys/kernel/k6a_gov/enable  # kill switch
```

k6a-ctl: `check_module.sh` validates delegated/profile, WebUI shows gov status live.

USB: `dmesg | grep -E "dwc3|bus_aggr|GCTL"` — no `ep0out timeout` loop, `IRQ >0`, `mtp,adb` `f1/f2` ok.

---

## Versions

| Version | Highlights |
|---------|------------|
| **v1.3.2 (current)** | **USB fix**: SM6150 clocks, `WAIT_FOR_LPM` deadlock, `bus_aggr`/`GCTL` 50ms, `is_a_peripheral` — no PC bootloop; **MIUI**: `sweet_miui.config` overlay + DTS `xiaomi/sweet` + CI matrix `aosp`/`miui`; **CI**: build number in release/ZIP (`vX-buildXX`); **KSU**: stay on `a5ff54c` (33300) until `88feb68` 4.14 port on PC (manager `34142634591` for now) |
| **v1.3.1** | Hardening: `find_gold_cpu`, notifier/mutex fixes, `cool_cur`/`status_show` locked, `ticks` fix |
| v1.3.0 | BW floors write, hash coupling, `badazz_safe` profile 5 |
| v1.2.1 | `badazz_safe`, multi-zone temp, `clamp_freq` fix |
| v1.2.0 | GPU enforcement, history ringbuffer, BW monitor RO |
| v1.1.2 | `battery_guard`, `poll_ms`, GPU levels RO |

Full Changelog: `git log --oneline`

---

## Fixed Bugs (v1.3.2)

| Bug | Symptom | Root Cause | Fix |
|-----|---------|------------|-----|
| **USB PC Bootloop** | PC host → instant reboot/bootloop, 67W charger OK, `No data transfer` didn't help, `bq2597x` init cut at 0.73s, stock boot.img OK | DWC3 `WAIT_FOR_LPM` deadlock + EP0 `TRB timeout` (`bus_aggr`/`noc_aggr` clocks off, `GCTL RAMCLKSEL` on `rev 00000000` wrong, `is_a_peripheral=0`) → WDT bite | SM6150 clocks (`GCC/DISPCC/CAMCC/SCC`), `WAIT_FOR_LPM` clear on extcon, `bus_aggr` enable + IOMMU guard, `GCTL` 50ms delay, `is_a_peripheral` `pullup`/`vbus_connect` — verified S21 + PC `IRQ>0` `mtp,adb` |
| **Identical Release Names** | All GitHub Releases/ZIPs named identically (`v4.14.369`) | `build-kernel.yml` `name: "Badazz-kernel v${VERSION}"` + ZIP `v${VERSION}.zip` without `run_number` | CI now `name: "v${VERSION}-badazz-build${run_number}"` + ZIP `v${VERSION}-build${run_number}[ -miui].zip` + artifact `…-buildXX-variant` |
| **KSU Manager "update required"** | Manager CI `34252913097` (`88feb68` `e801e16` version matching) on kernel `a5ff54c` → red banner | `e801e16` new UAPI + `bundled_lkm` check, old kernel UAPI mismatch; `88feb68` needs `KPROBES` + 5.x APIs (`syscall_fn_t`, `pgtable.h`, `lsm_hook`) not 4.14 compatible | Fork `dev` → `88feb68` for PC, hotfix `CONFIG_KPROBES=y` + `syscall_fn_t`/`pgtable`/`ksys_close` guards, then **revert** to `a5ff54c` until proper 4.14 port on PC (manager stay on `34142634591` for now) |
| **MIUI/HOS not booting** | Stock HyperOS `V14.0.1.0` `4.14.190-perf` needs `ARCH_SM6150/CAMCC/GCC/PDC` + `xiaomi/sweet` DTS (`GTX9896_K6`, `FPC1540`) | `sweet_defconfig` flattened to `atoll/sm6150`, `QUSB2` only | `sweet_miui.config` overlay (7 lines) + `xiaomi/sweet` DTS shim + CI matrix `aosp`/`miui` (`miui/test` artifacts, `main` release `…-miui.zip`) |

---

## Roadmap

| Priority | Item | Status | Next Step |
|----------|------|--------|-----------|
| **P0** | **KSU 88feb68 4.14 Port on PC** | `badazzrebase.md` ready | `k6a-sweet` rebase `a5ff54c` → `88feb68` (Squash 49 files, `meld` for `Kconfig/Makefile/selinux.c` `susfs_is_current`), new branch `k6a-sweet-88feb68`, Badazz bump, CI green ~15 min |
| **P0** | **SUSFS full restore** | Hotfix relaxed validation, `fs/susfs` `v2.2.0` intact but KSU side `k6a-sweet` patches pending | After rebase: `CONFIG_KSU_SUSFS` + `TAMPER_SYSCALL_TABLE` validation back, `nm vmlinux | grep susfs_is_current` green |
| **P1** | **MIUI/HOS verification** | `sweet_miui.config` + DTS + `miui/test` CI `aosp+miui` artifacts | Flash `…-miui.zip` on HyperOS `V14.0.1.0` (or `2.0`), test `dmesg` `fpc/goodix/touchfeature/ds28e16`, 120Hz, NFC, `usb` `host/device` |
| **P1** | **USB final verification** | `main` `3a17062` + `10d3d0bb9` already fixes PC/S21, `miui/test` pending | PC `fastboot boot` + `mtp,adb` `f1/f2`, `k6a-ctl` `control=auto` (workaround removed `v1.1.6`) |
| **P2** | **Release hygiene** | `main` `v1.3.2` + `k6a-ctl` `v1.1.6` versioned ZIPs | Next `main` release `v4.14.369-badazz-buildXX` (+ `-miui` variant) with changelog |
| **P2** | **Docs** | README now English `v1.3.2` | Keep `usbbug.md` + `badazzrebase.md` updated after PC rebase |

---

## Download

| Source | Link |
|--------|------|
| BadazzKernel Releases | [GitHub](https://github.com/vandalsquad187/BadazzKernel/releases) |
| k6a-ctl (Companion) | [GitHub](https://github.com/vandalsquad187/k6a-ctl) |
| Rebase Plan (PC) | `/sdcard/Download/badazzrebase.md` |

---

## Credits

- **BadazZ89** — Kernel, k6a_gov
- **vandalsquad187** — Base, CI
- **KernelSU-Next / SUSFS** — Root/Hide (`k6a-sweet` `a5ff54c`)
- **AnyKernel3** — Flash template
- **MiDoNaSR545** — Reference for MIUI/HOS (`sweet_k6a-r-oss`)

---

## License

GPL v2 — see `COPYING`.
