<div align="center">
  <img src="assets/logo.png" alt="Badazz89" width="180"/>
  <h1>BadazzKernel</h1>
  <p>Gaming-optimized kernel for <b>SM7150 (sweet / sweet k6a)</b> — 4.14.369</p>
  <p>
    <img src="https://img.shields.io/badge/Kernel-4.14.369-blue?style=flat-square">
    <img src="https://img.shields.io/badge/k6a__gov-1.7.7-orange?style=flat-square">
    <img src="https://img.shields.io/badge/KernelSU--Next-v3.3.0--311-green?style=flat-square">
    <img src="https://img.shields.io/badge/SUSFS-v2.3.0-success?style=flat-square">
    <img src="https://img.shields.io/badge/Android-13%20%E2%80%93%2016-lightgrey?style=flat-square">
  </p>
</div>

---

## Overview

BadazzKernel is a performance and gaming tuned kernel for **Redmi Note 12 Pro 4G (sweet / sweetin)**. Core is the in-kernel governor **k6a_gov v1.7.7** (loadable module, `CONFIG_K6A_GOV=m`, insmodded by k6a-ctl) — the perfect base for the companion module **[k6a-ctl](https://github.com/vandalsquad187/k6a-ctl)**. Both work hand-in-hand in delegated mode: kernel throttles, module controls.

🤌🏻 Join my Telegram channel: https://t.me/Badazz89

---

## Architecture

```
linux-4.14.369
├── KernelSU-Next v3.4.2-329 (UAPI 6, 4.14 direct-patch) + SUSFS v2.3.0  # Root + Hide (submodule 7abf1c77)
├── fs/nomount — NoMount v22 (dev 8f05de7)      # VFS path redirection — zero-mount module injection (CONFIG_NOMOUNT=y)
├── drivers/thermal/k6a_gov/k6a_gov.c v1.7.7     # In-kernel gaming governor (CONFIG_K6A_GOV=m)
├── drivers/gpu/msm/kgsl_pwrctrl.c               # GPU pwrlevel export (k6a_gov)
├── drivers/devfreq/devfreq.c                    # BW floors (gpubw / llcc), 0 = release floor
├── drivers/gpu/msm/ + drivers/thermal/          # Thermal + cooling floors
├── drivers/usb/dwc3/ + phy/qcom                 # USB: SM6150 clocks, WAIT_FOR_LPM, bus_aggr, ep-cmd recovery
└── arch/arm64/boot/dts/qcom/                    # sweet / sdmmagpie DT (AOSP + MIUI overlay)
```

### Kernel Features

| Area | Feature | Details |
|------|---------|---------|
| **Governor** | **k6a_gov v1.7.7** | Loadable module (`CONFIG_K6A_GOV=m`, `k6a_gov.ko` shipped in the zip and loaded by k6a-ctl), state machine OFF→GAMING→CD_L2/L3/L4, hysteresis, **immediate escalation** L2→L3→L4, entry dwell on GAMING, L4 flap margin (`peak + 3 K < cd_l4_temp`), `profile=0` forces OFF immediately, throttle history (16), **residency accounting (`residency_ms`) + live `gold_cap_now` with GPU-cap ramping (tighten 1 OPP/tick, release 1 OPP/4 ticks, L4/entry-immediate), `stats_reset` node**, boot default **schedutil** |
| **CPU** | Gold clamp | `find_gold_cpu()` + `clamp_freq`, `enforce_max_freq` + `cpufreq_update_policy` (kthread only), hardened notifier, **released again on recovery/disable** |
| **Temp** | Multi-zone | Max over 4 Gold zones `cpu-1-0..3-usr`, fallback `cpu-0-0-usr` → `xo-therm` → `soc-therm` → `thermal_zone0`; `temp_src`/`temp_valid` in status, holds the last state if no zone answers |
| **GPU** | Native enforcement | `kgsl_k6a_get_levels` / `kgsl_k6a_set_max_level_idx`, caps per CD state |
| **BW** | Devfreq floors | `k6a_devfreq_set_bw` for `gpubw` + `cpu-llcc-ddr-bw`, per profile/CD state, **really released again when the state recovers** |
| **Profiles** | 6 profiles | `off/gaming/battery/badazz/custom/badazz_safe` — temps, Gold/GPU/BW floors |
| **Safety** | Battery guard + hash | `battery_guard` @`battery_guard_temp` (35..60 °C, default 45) → CD_L2, `poll_ms` 100..5000, `verify_build_hash` vs `k6a_features/git_hash` |
| **Thermal** | Cooling device | `k6a_gov` as `thermal_cooling_device`, `cool_cur` locked, `K6A_CD_L4` clamp |
| **Root** | KSU-Next + SUSFS | v3.4.2-329 **UAPI 6**, 4.14 direct-patch mode (submodule `7abf1c77`), SUSFS `v2.3.0` round 2 (sus_path/mount/kstat/map, **dup-safe adds + caps, KSU-domain passthrough — root sees the truth, `del_sus_path` cmd**), `tamper_syscall_table` |
| **Injection** | NoMount v22 | Built-in VFS path-redirection subsystem (`CONFIG_NOMOUNT=y`, dev `8f05de7`), control channel `add_key("nomount")` — the **NoMount metamodule** (v2.2.0) injects installed KSU modules as VFS operations instead of mounts: **`/proc/mounts` stays clean**, fully transparent to apps, UID isolation for exclusions. Verified live in Build 441 |
| **USB** | DWC3 / QUSB2 | SM6150 clocks (`GCC/DISPCC/CAMCC`), `WAIT_FOR_LPM` clear, `bus_aggr`/`GCTL` 50ms, `is_a_peripheral` fix — PC bootloop gone since v1.3.2. Full device-mode enumeration works since Build 336 (Faults 1/4 fixed) — see [`docs/devlog/readme-usb-fixedbugs.md`](docs/devlog/readme-usb-fixedbugs.md) |
| **Scheduler** | UCLAMP, SCHED_CASS | Latency/efficiency tuning |
| **Memory** | KSM, LRU_GEN, ZRAM lz4 | Gaming stability |
| **Net** | BBR | Low latency |
| **Wakelock** | BOEFFLA_WL_BLOCKER | Blocks wasteful wakelocks |
| **Monitor** | MSM_PERFORMANCE, PSI | `cpu_freq_times`, pressure stall |
| **NFC** | PN80T I2C driver | `CONFIG_NFC=n` intentional (NCI in userspace), `CONFIG_NFC_NQ_PN80T=y` provides `/dev/nq-nci` — fix via [Sweet2NfcFIX](https://github.com/vandalsquad187/Sweet2NfcFIX) module |
| **MIUI** | HyperOS overlay | `sweet_miui.config` overlay (`ARCH_SM6150/GCC/CAMCC/DISPCC/PDC`) + `xiaomi/sweet` DTS, unified AnyKernel, CI matrix `aosp`/`miui` |

> Detailed k6a_gov version changelogs (v1.6.0 → v1.7.7): [`docs/devlog/gov-version-changelog.md`](docs/devlog/gov-version-changelog.md).

---

## Companion: k6a-ctl

Module **[k6a-ctl v1.4.6](https://github.com/vandalsquad187/k6a-ctl/releases)** is the userspace companion — it is what actually **loads `k6a_gov.ko`** at boot:

```
k6a-ctl v1.4.6 (delegated=1)
├── service.sh        # insmod k6a_gov.ko + three-stage version lock + watchdog
├── k6a-controller    # Game detection, schedutil, auto badazz_safe @85°C
├── k6a-lib.sh        # gov_write/gov_read helpers
├── settings.conf     # delegated, profile, auto_badazz_temp
├── sepolicy.rule     # allow kernel sysfs dir/file — the build-hash read needs it
├── check_module.sh   # Build gate (repo-side, never shipped in the zip)
└── WebUI             # Gov status, BW floors, GPU caps, history, build hash
```

**Delegated mode** (`delegated=1`, default): kernel governor controls thermal (CPU/GPU/BW), module does game detection + sched tuning + monitoring. Legacy (`delegated=0`) is fallback only.

**Why `sepolicy.rule` matters**: `verify_build_hash()` runs inside the `k6a_gov` kernel
thread, whose SELinux domain is `u:r:kernel:s0` and which has **no** read access to sysfs
(`kernel → sysfs:file` = `0`, and the denial is `dontaudit`ed so it never shows in `dmesg`).
Without the two `allow kernel sysfs …` lines the check exhausts its 60 retries after 15 s and
`hash_state` stays `3` ("not checked"). Userspace reading the same node works fine, which is
exactly what makes this trap invisible.

### Kernel interfaces (`/sys/kernel/k6a_gov/`)

| Node | R/W | Purpose |
|------|-----|---------|
| `enable` | RW | 0/1 — kill switch, releases the Gold cap + BW floors + GPU cap |
| `profile` | RW | 0..5 — off/gaming/battery/badazz/custom/badazz_safe (4 = keeps the current thresholds) |
| `status` | RO | `version/state/temp/temp_src/temp_valid/temp_peak/ticks/state_age_ms/policy_max/throttle_events/gold_* /bw_* /gpu_floor/gpu_floor_last/gpu_last_idx/bt/hash_verified/hash_state/build_hash/battery_guard/batt_latched/thresholds_source/cd_l2_temp/cd_l3_temp/cd_l4_temp/cd_recover/vendor_sconfig/vendor_cpu_limits/game_pid/residency_ms` (Build450: hist lives in its own node) |
| `hysteresis` | RW | `fast normal` — dwell ms, `fast` 1..5000 (default 3000), `normal` 1..5000 (default 2000) |
| `hist` | RO | Transition ring (16), `ms:temp:from>to` comma-separated, ms since insmod, oldest first — empty without transitions |
| `cd_thresholds` | RW | `l2t l3t l4t rec` in **°C**, `l2g l3g l4g` in **kHz** (CPU-Gold-Caps) — Beispiel: `96 99 102 93 1708800 1209600 1094400` (GAMING) |
| `cd_ladder` | RW | 0/1 (default 1) — fine-grained CD ladder: piecewise-linear Gold-cap interpolation `l2g→l3g→l4g` over the threshold temps; `0` = flat Build459 behavior (A/B without re-flash) |
| `cd_silver_caps` | RW | `l2 l3 l4` — **Hz** — Silver (CPU-0) caps in the CD path, `0 0 0` = off (default) |
| `gpu_caps` | RW | `l2 l3 l4` — **Hz** — Beispiel: `800000000 565000000 430000000` (GAMING) |
| `bw_floors` | RW | `gpubw_l2 l3 l4 llcc_l2 l3 l4` — **raw DT-table votes** (gpubw/llcc devfreq tables, max ≈ 6881/7716), each ≤ 30000, 0 = no floor — Beispiel: `4000 3000 1500 0 4000 3000` (GAMING) |
| `battery_guard` | RW | 0/1 — battery guard (trip point in `battery_guard_temp`) |
| `battery_guard_temp` | RW | 35..60 °C, default 45 |
| `poll_ms` | RW | 100..5000 — kthread interval, default 250 |
| `legacy` | RW | 0/1 — enforcement on/off; 0 releases everything the governor holds |
| `game_pid` | RW | PID of the game — stored and echoed in `status` only, the kernel never acts on it |
| `poll_adaptive` | RW | 0/1 (default 1) — slow-poll (≥1 s) while OFF or battery-cool; instant fast-poll on a +3 K trend or any action-range exit. `0` = constant `poll_ms` |

**Units in this interface:** CPU limits/frequencies in **kHz**, GPU limits in **Hz**, BW floors as **raw DT-table votes** (gpubw/llcc devfreq), temperatures as °C in `k6a_gov` and **milli-°C** in `k6a_thermal`.

### k6a_thermal — CPU-cooling floor (Build 401, `/sys/kernel/k6a_thermal/`)

Own clamp layer in `cpu_cooling.c`: under thermal pressure the CPU max level is held at a floor instead of letting cpufreq collapse. SELinux label `u:object_r:sysfs:s0`, readable/writable as root.

| Node | R/W | Unit / Default | Purpose |
|------|-----|----------------|---------|
| `thermal_floor_enable` | RW | 0/1, default 1 | Master switch for the floor clamp |
| `thermal_floor_hard_temp` | RW | m°C, default `100000` (= 100 °C) | At/above this temp the floor is lifted entirely (hard override) |
| `thermal_floor_temp1` | RW | m°C, default `85000` (85 °C) | First floor step: hold CPU level ≥ 6 |
| `thermal_floor_temp2` | RW | m°C, default `90000` (90 °C) | Second step: hold level ≥ 7 |
| `thermal_floor_temp3` | RW | m°C, default `95000` (95 °C) | Third step: hold level ≥ 8 |

### ftrace tracepoint (Build 402)

`/sys/kernel/tracing/events/k6a_gov/k6a_state` — fires on every governor state transition with `from`, `to`, `temp`. Test: `echo 1 > /sys/kernel/tracing/events/k6a_gov/k6a_state/enable && echo 0 > /sys/kernel/k6a_gov/enable && echo 1 > /sys/kernel/k6a_gov/enable && grep k6a_state /sys/kernel/tracing/trace`.

> **Kernel provides the levers — k6a-ctl pulls them.** Without module k6a_gov runs with safe defaults. With module it reacts dynamically to load/temperature.

---

## Installation

1. Download **Release** from [GitHub Releases](https://github.com/vandalsquad187/BadazzKernel/releases) —
   two zips per release: `…-buildXX-aosp.zip` (Stock/HOS/AOSP) and `…-buildXX-miui.zip` (HyperOS)
2. Flash ZIP via **OrangeFox / TWRP** (AnyKernel3). The zip also copies **`k6a_gov.ko`** to
   `/data/adb/k6a_gov.ko` (`/` is dm-verity read-only, so the module cannot live in `/system`).
   Do **not** use `ksud module install` for upgrades — `su -c` here runs with `CapBnd=0` and it
   fails with `Permission denied`; unpack and `mv` the files in as root instead.
3.    Install **KernelSU-Next Manager** APK — one working build: CI run [`36750739326`](https://github.com/KernelSU-Next/KernelSU-Next/actions/runs/36750739326) (`2b31f718` → `v3.4.0-19-g2b31f718-spoofed`), **uapi 4** (our current kernel line runs **UAPI 6**, 4.14 direct-patch, submodule `7abf1c77`). The spoofed build ships a **randomised applicationId** (it has been `gojcms.hgelex.jabkht`), so `pm list packages | grep ksu` finds nothing — identify it by `versionName` instead. CI `34252913097` still needs the pending `88feb68` 4.14 port
4. (Required for auto-load) Install **[k6a-ctl](https://github.com/vandalsquad187/k6a-ctl/releases)**
   — `delegated=1`. Current release **v1.3.1**; without it nothing insmods `k6a_gov.ko` and the
   kernel falls back to the built-in legacy cooldown (thermal protection still works, just no
   in-kernel caps/BW floors).

### Build (Dev)

```bash
git clone https://github.com/vandalsquad187/BadazzKernel && cd BadazzKernel
git submodule update --init --recursive
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-gnueabihf-
make sweet_defconfig
# MIUI variant:
scripts/kconfig/merge_config.sh arch/arm64/configs/sweet_defconfig arch/arm64/configs/sweet_miui.config
make olddefconfig && make -j$(nproc) Image.gz dtb.img dtbo.img
# ZIP:
cp arch/arm64/boot/Image.gz arch/arm64/boot/dtb.img arch/arm64/boot/dtbo.img anykernel/
cd anykernel && zip -r9 ../Badazz-kernel-sweet-v4.14.369-buildNNN-aosp.zip . -x "*.git*"
```

CI builds automatically on every push to `main`: **two builds per push** —
`…-buildXX-aosp.zip` (Stock/HOS/AOSP — custom-USB profile) and
`…-buildXX-miui.zip` (MIUI/HyperOS — stock-USB profile, via `sweet_miui.config`
plus branch reverts). Both land in **one Release**. The `miui` build auto-resyncs
`miui/test` with `main` using `merge -X ours` (MIUI stock-USB blockade wins
conflicts; non-conflicting main changes flow in normally, merge commit is
`[skip ci]` to avoid trigger cascades). Pushes to `miui/**` only produce artifacts
(no release). Runtime ~13–17 min warm; `gh run list` / `gh run watch <id>`.

> **Supported defconfig:** only **`sweet_defconfig`** (aosp + miui overlay) is built and
> tested in CI. Legacy defconfigs in `arch/arm64/configs/` (`tucana`, `davinci`, `phoenix`,
> `toco`, …) are **unmaintained** — no CI, no flash testing, no support.

---

## Status & Debug

```bash
dmesg | grep k6a_gov          # v1.6.0 loaded, hash verified, gpu levels, pr_fmt prefix
cat /sys/kernel/k6a_gov/status   # incl. policy_max, state_age_ms, temp_src, temp_valid
cat /sys/kernel/k6a_gov/battery_guard; cat /sys/kernel/k6a_gov/poll_ms
echo 0 > /sys/kernel/k6a_gov/enable  # kill switch
```

k6a-ctl: `check_module.sh` validates delegated/profile, WebUI shows gov status live.

```bash
# USB health
dmesg | grep -E "Build31[3-9]|Build320|STARTFAIL|STARTTOUT"   # fault markers
dmesg | grep -E "Build336|set_alt done ok|USB_STATE=CONFIGURED"  # enumeration succeeded
dmesg | grep -E "dwc3|bus_aggr|GCTL"                          # no ep0out timeout loop, IRQ >0
```
Healthy connect: `mtp,adb` with `f1/f2` endpoints and `dwc3_msm mtp,adb` bound.

---

> USB fault history (Builds 300–336) and the fixed-bugs postmortem table: [`docs/devlog/readme-usb-fixedbugs.md`](docs/devlog/readme-usb-fixedbugs.md).

---

## Versions

| Version | Highlights |
|---------|------------|
| **k6a_gov v1.6.0 (Build 389 scope → `build389`; Fixes 1–10 → `build397`/`build398`, 2026-10-27)** | **GPU floor sysfs** (`gpu_floor`, Hz, 0 = off): GAMING-only KGSL `min_pwrlevel` pin via new export `kgsl_k6a_set_min_level_idx()` (on-device proof: `min_pwrlevel=2` pins 565 MHz, release → 355); user min captured once (`gpu_floor_saved_min`) and restored; status keys `gpu_floor=`, `gpu_floor_last=`, `bt=` (battery temp). **Fixes 1–10** (table above): Gold-clamp release safety, `enable` → GPU-floor reset, floor user-min restore, battery-guard temp bug + decoupling, init UAF, `game_pid` bounds + status echo, static struct + 0444 params + `pr_fmt`, L4 flap margin, `profile=off` force-OFF, GPU-cap user-value restore. KernelSU-Next submodule `72f8cc626` (v3.3.0-311, UAPI 6), SUSFS core synced to v2.3.0 (Build 394) |
| **Build 345 scope (K1–K7 + D) → release `build346`** | **Real build identity**: `drivers/thermal/Makefile` injects `git rev-parse --short=8 HEAD` as `K6A_GIT_HASH`/`K6A_BUILD_HASH` through `subdir-ccflags-y`, and both C files `#error` if the flag is missing instead of falling back to `full-synergy`. `verify_build_hash()` now compares the **whole** string (the old prefix `strncmp` accepted any runtime hash that merely started with the build one) and can report *unavailable*. **Fail-safe instead of fail-open**: a mismatch no longer turns enforcement off — it logs `pr_err`, sets `hash_state=2` (WebUI row turns red) and keeps the governor running. **K1** the gold cap is re-read after `cpufreq_update_policy()` and retried if it did not take; **K2** the gold cluster is identified by `gold_mask` (CPU 0 excluded, no more `?: 6` fallback) and the `CPUFREQ_ADJUST` notifier clamps every CPU of it; **K3** the battery guard latches until the pack is 5 K cooler; **K4** dwell is fixed (`fast` in, `normal` out) instead of being picked from a per-tick delta that flips with sensor noise; **K5** BW floors are written only on change and latched only after a successful readback; **K6** thermal zone lookups are cached; **K7** the boot default governor is `schedutil` instead of `performance`. Version stays **1.5.0** — vermagic and the hash do the locking. **Shipped and accepted on device 2026-10-03**. |
| **k6a_gov v1.5.0 (Build 341 scope → release `build344`)** | **`CONFIG_K6A_GOV=m`** — the governor ships as `k6a_gov.ko` inside the flash zip, is dropped at `/data/adb/k6a_gov.ko` and insmodded by k6a-ctl, so governor updates no longer need a kernel flash. **Version lock**: `hash_state` (0 pending / 1 verified / 2 mismatch / 3 not checked) with a bounded retry replaces a `hash_verified` latch that reported "verified" for a check skipped because `/sys` was not mounted yet; the per-boot `build/run version delta` warning is gone (the check was dropped — `LINUX_VERSION_CODE` stays pinned at `4.14.255` on purpose, see AGENTS.md, and vermagic does the real locking). Vermagic ties the `.ko` to exactly this build. **Shipped and accepted on device 2026-10-03**:
`version=1.5.0 hash_verified=1 hash_state=1`, `build hash verified (full-synergy) retries=0`.
Note the release is named `build344` — `build341`/`build342` are older docs-only builds and run
343 is the one that failed, because release names follow `github.run_number`. |
| **k6a_gov v1.4.0 (Build 340)** | Gold cap is released again on recovery/`enable=0`/`legacy=0` (`cpufreq_update_policy` instead of pokes into `policy->max`); **immediate escalation** L2→L3→L4 and L3→L4; entry dwell in GAMING (fast only for a ≥5 °C *rise*); temperature fail-safe holds the last state when no zone answers (`temp_src`, `temp_valid`); BW floors are actually released again (`k6a_devfreq_set_bw(…,0,0)` silently did nothing before); sysfs validation (all-or-none zero, monotonic caps, bounds) + `battery_guard_temp`, `profile` sanitised at init |
| **v1.3.2 (historical)** | **USB fix**: SM6150 clocks, `WAIT_FOR_LPM` deadlock, `bus_aggr`/`GCTL` 50ms, `is_a_peripheral` — no PC bootloop; **MIUI**: `sweet_miui.config` overlay + DTS `xiaomi/sweet` + CI matrix `aosp`/`miui`; **CI**: build number in release/ZIP (`vX-buildXX`); **KSU**: submodule at `b100bd28` (runtime `38309` = `35000 + git`, UAPIv4) until `88feb68` 4.14 port on PC; userspace ksud + manager `v3.4.0-19-g2b31f718` (CI `36750739326`), spoofed APK with random applicationId |
| **v1.3.1** | Hardening: `find_gold_cpu`, notifier/mutex fixes, `cool_cur`/`status_show` locked, `ticks` fix |
| v1.3.0 | BW floors write, hash coupling, `badazz_safe` profile 5 |
| v1.2.1 | `badazz_safe`, multi-zone temp, `clamp_freq` fix |
| v1.2.0 | GPU enforcement, history ringbuffer, BW monitor RO |
| v1.1.2 | `battery_guard`, `poll_ms`, GPU levels RO |

Full Changelog: `git log --oneline`

---


---

## Roadmap

| Priority | Item | Status | Next Step |
|----------|------|--------|-----------|
| **P0** | **KSU 88feb68 4.14 Port on PC** | **Superseded** — the shipped line is `72f8cc626` (v3.3.0-311, UAPI 6) **built-in as 4.14 direct-patch mode**, root fully working on device; the `88feb68` rebase plan stays parked (`badazzrebase.md`) | Only revisit if the `88feb68` UAPI becomes mandatory upstream |
| **P0** | **SUSFS full restore** | **Done** — SUSFS core synced to `v2.3.0` (Build 394, thin-core `fs/susfs/` split + hooks), susfs ABI v2.3.0; KSU_SUSFS + tamper validation in defconfig/CI gate | Nothing outstanding |
| **P1** | **MIUI/HOS verification** | `sweet_miui.config` + DTS + `miui/test` CI `aosp+miui` artifacts | Flash `…-miui.zip` on HyperOS `V14.0.1.0` (or `2.0`), test `dmesg` `fpc/goodix/touchfeature/ds28e16`, 120Hz, NFC, `usb` `host/device` |
| **P1** | **USB Fault 3 (charger)** | **Closed 2026-10-02** — 500 s capture on Build 338 (`printk 8 4 1 7`, 40 491 lines): 0× `APSD=OCP`, `smblib_rerun_apsd` exactly once per plug event, `vbus_notifier` only on FLOAT connect/detach | Nothing to fix; keep `~/tmp/cap334.sh` as the recipe if it ever returns |
| **P1** | **k6a_gov → loadable module** | **Done in Build 341 (`c42a67d07`), flashed as `build344` 2026-10-03** — `CONFIG_K6A_GOV=m`, CI builds `modules` and ships `k6a_gov.ko`, `anykernel.sh` drops it at `/data/adb/k6a_gov.ko` (`/system` is dm-verity, not writable), k6a-ctl insmods it with a three-stage version lock and the legacy cooldown as fallback | Verified on device: `version=1.5.0`, `hash_state=1` |
| **P1** | **Build 345 governor scope (K1–K7 + D)** | **Done — released as `build346`, flashed and verified 2026-10-03** — real git hash in the build, fail-safe hash mismatch, gold-cap readback, `gold_mask` clamp, battery latch, fixed dwell, BW change-detect, zone cache, `schedutil` boot default | Post-flash checklist green: `Build345:` markers, `hash_state=1`, `build_hash` = `git_hash` = `699eee6e`, `gold cap … policy_max=…` with 0 retries, 70/70 `policy_max == scaling_max_freq`, dwell 2 s out, 0 `set_bw` retries, battery 39.5 °C → `batt_latched=0`, idle 75 °C |
| **P1** | **k6a-ctl sync** | **Done** — repo at **v1.3.2 (`768d1bc`)**: `check_module.sh` gates `[1]`–`[6]` incl. the governor checks, `battery_guard`/`_temp`, `policy_max`/`state_age_ms`/`temp_src`/`temp_valid` WebUI keys, **F1 forces `schedutil` and re-asserts it on drift**, **device runs v1.3.2** | Nothing outstanding |
| **P2** | **Release hygiene** | `main` `v1.3.2` + `k6a-ctl` `v1.3.2` versioned ZIPs | Releases are `v4.14.369-badazz-buildXX` automatically via CI; next `main` release gets a changelog |
| **P2** | **Docs** | `README.md` + `AGENTS.md` refreshed at `build398` (k6a_gov Fixes 1–10, gpu_floor state, KSU/SUSFS versions, USB faults closed, r60 sampler/blob-killer watchdog saga, A17 readiness audit) | `Documentation/` is stock Linux 4.14 — leave it alone, keep project docs in the root `*.md` |

---

## Download

| Source | Link |
|--------|------|
| BadazzKernel Releases | [GitHub](https://github.com/vandalsquad187/BadazzKernel/releases) |
| k6a-ctl (Companion) | [GitHub](https://github.com/vandalsquad187/k6a-ctl) |
| Rebase Plan (PC) | `/sdcard/Download/badazzrebase.md` |

**Repo docs**: `README.md` (public), `AGENTS.md` (compact dev state, Fallen, Working Agreements),
`docs/devlog/` (history: status logs, build scopes, rounds, USB, CODM),
`CONTRIBUTING.md` (upstream kernel guide). `Documentation/` is the **stock Linux 4.14**
kernel documentation (6090 files) — kernel-internal API docs, nothing project-specific.

---

## Credits

- **BadazZ89** — Kernel, k6a_gov
- **vandalsquad187** — Base, CI
- **KernelSU-Next / SUSFS** — Root/Hide (submodule `7abf1c77`, v3.4.2-329, UAPI 6, SUSFS `v2.3.0` round 2)
- **AnyKernel3** — Flash template
- **MiDoNaSR545** — Reference for MIUI/HOS (`sweet_k6a-r-oss`)

---

## License

GPL v2 — see `COPYING`.
