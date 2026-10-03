<div align="center">
  <img src="assets/logo.png" alt="Badazz89" width="180"/>
  <h1>BadazzKernel</h1>
  <p>Gaming-optimized kernel for <b>SM7150 (sweet / sweet k6a)</b> — 4.14.369</p>
  <p>
    <img src="https://img.shields.io/badge/Kernel-4.14.369-blue?style=flat-square">
    <img src="https://img.shields.io/badge/k6a__gov-1.3.1-orange?style=flat-square">
    <img src="https://img.shields.io/badge/KernelSU--Next-38309-green?style=flat-square">
    <img src="https://img.shields.io/badge/SUSFS-yes-success?style=flat-square">
    <img src="https://img.shields.io/badge/Android-13%20%7C%2014-lightgrey?style=flat-square">
  </p>
</div>

---

## Overview

BadazzKernel is a performance and gaming tuned kernel for **Redmi Note 12 Pro 4G (sweet / sweetin)**. Core is the in-kernel governor **k6a_gov v1.5.0** (shipped as a loadable module since Build 341) — the perfect base for the companion module **[k6a-ctl](https://github.com/vandalsquad187/k6a-ctl)**. Both work hand-in-hand in delegated mode: kernel throttles, module controls.

🤌🏻 Join my Telegram channel: https://t.me/Badazz89

---

## Architecture

```
linux-4.14.369
├── KernelSU-Next 38309 (UAPIv4) + SUSFS v2.2.0  # Root + Hide (submodule b100bd28)
├── drivers/thermal/k6a_gov/k6a_gov.c v1.5.0     # In-kernel gaming governor (CONFIG_K6A_GOV=m)
├── drivers/gpu/msm/kgsl_pwrctrl.c               # GPU pwrlevel export (k6a_gov)
├── drivers/devfreq/devfreq.c                    # BW floors (gpubw / llcc), 0 = release floor
├── drivers/gpu/msm/ + drivers/thermal/          # Thermal + cooling floors
├── drivers/usb/dwc3/ + phy/qcom                 # USB: SM6150 clocks, WAIT_FOR_LPM, bus_aggr, ep-cmd recovery
└── arch/arm64/boot/dts/qcom/                    # sweet / sdmmagpie DT (AOSP + MIUI overlay)
```

### Kernel Features

| Area | Feature | Details |
|------|---------|---------|
| **Governor** | **k6a_gov v1.5.0** | Loadable module (`CONFIG_K6A_GOV=m`, `k6a_gov.ko` shipped in the zip and loaded by k6a-ctl), state machine OFF→GAMING→CD_L2/L3/L4, hysteresis, **immediate escalation** L2→L3→L4, entry dwell on GAMING, throttle history (16), boot default **schedutil** |
| **CPU** | Gold clamp | `find_gold_cpu()` + `clamp_freq`, `enforce_max_freq` + `cpufreq_update_policy` (kthread only), hardened notifier, **released again on recovery/disable** |
| **Temp** | Multi-zone | Max over 4 Gold zones `cpu-1-0..3-usr`, fallback `cpu-0-0-usr` → `xo-therm` → `soc-therm` → `thermal_zone0`; `temp_src`/`temp_valid` in status, holds the last state if no zone answers |
| **GPU** | Native enforcement | `kgsl_k6a_get_levels` / `kgsl_k6a_set_max_level_idx`, caps per CD state |
| **BW** | Devfreq floors | `k6a_devfreq_set_bw` for `gpubw` + `cpu-llcc-ddr-bw`, per profile/CD state, **really released again when the state recovers** |
| **Profiles** | 6 profiles | `off/gaming/battery/badazz/custom/badazz_safe` — temps, Gold/GPU/BW floors |
| **Safety** | Battery guard + hash | `battery_guard` @`battery_guard_temp` (35..60 °C, default 45) → CD_L2, `poll_ms` 100..5000, `verify_build_hash` vs `k6a_features/git_hash` |
| **Thermal** | Cooling device | `k6a_gov` as `thermal_cooling_device`, `cool_cur` locked, `K6A_CD_L4` clamp |
| **Root** | KSU-Next + SUSFS | 38309 **UAPIv4** (submodule `b100bd28`), SUSFS `v2.2.0` (sus_path/mount/kstat/map), `tamper_syscall_table` |
| **USB** | DWC3 / QUSB2 | SM6150 clocks (`GCC/DISPCC/CAMCC`), `WAIT_FOR_LPM` clear, `bus_aggr`/`GCTL` 50ms, `is_a_peripheral` fix — PC bootloop gone since v1.3.2. Full device-mode enumeration works since Build 336 (Faults 1/4 fixed) — see [USB status](#usb-status) |
| **Scheduler** | UCLAMP, SCHED_CASS | Latency/efficiency tuning |
| **Memory** | KSM, LRU_GEN, ZRAM lz4 | Gaming stability |
| **Net** | BBR | Low latency |
| **Wakelock** | BOEFFLA_WL_BLOCKER | Blocks wasteful wakelocks |
| **Monitor** | MSM_PERFORMANCE, PSI | `cpu_freq_times`, pressure stall |
| **NFC** | PN80T I2C driver | `CONFIG_NFC=n` intentional (NCI in userspace), `CONFIG_NFC_NQ_PN80T=y` provides `/dev/nq-nci` — fix via [Sweet2NfcFIX](https://github.com/vandalsquad187/Sweet2NfcFIX) module |
| **MIUI** | HyperOS overlay | `sweet_miui.config` overlay (`ARCH_SM6150/GCC/CAMCC/DISPCC/PDC`) + `xiaomi/sweet` DTS, unified AnyKernel, CI matrix `aosp`/`miui` |

---

## Companion: k6a-ctl

Module **[k6a-ctl v1.3.1](https://github.com/vandalsquad187/k6a-ctl/releases)** is the userspace companion — it is what actually **loads `k6a_gov.ko`** at boot:

```
k6a-ctl v1.3.1 (delegated=1)
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
| `status` | RO | `version/state/temp/temp_src/temp_valid/ticks/state_age_ms/policy_max/throttle_events/gold_* /bw_* /hash_verified/hash_state/build_hash/batt_latched/hist=` |
| `hysteresis` | RW | `fast normal` — dwell ms, `fast` 1..1000 (default 500), `normal` 1..5000 (default 2000) |
| `cd_thresholds` | RW | `l2t l3t l4t rec` in **°C** then `l2g l3g l4g` in **Hz** — 7 values, all-or-none zero, Gold caps must be non-increasing |
| `gpu_caps` | RW | `l2 l3 l4` — GPU Hz caps, all-or-none zero, must be non-increasing, ≤ 2 000 000 000 |
| `bw_floors` | RW | `gpubw_l2 l3 l4 llcc_l2 l3 l4` — MB/s, each ≤ 30000, 0 = no floor |
| `battery_guard` | RW | 0/1 — battery guard (trip point in `battery_guard_temp`) |
| `battery_guard_temp` | RW | 35..60 °C, default 45 |
| `poll_ms` | RW | 100..5000 — kthread interval, default 250 |
| `legacy` | RW | 0/1 — enforcement on/off; 0 releases everything the governor holds |
| `game_pid` | RW | PID of the game — stored and echoed in `status` only, the kernel never acts on it |

> **Kernel provides the levers — k6a-ctl pulls them.** Without module k6a_gov runs with safe defaults. With module it reacts dynamically to load/temperature.

---

## Installation

1. Download **Release** from [GitHub Releases](https://github.com/vandalsquad187/BadazzKernel/releases) (`Badazz-kernel-sweet-v4.14.369-buildXX.zip` or `-miui.zip` for HyperOS)
2. Flash ZIP via **OrangeFox / TWRP** (AnyKernel3). The zip also copies **`k6a_gov.ko`** to
   `/data/adb/k6a_gov.ko` (`/` is dm-verity read-only, so the module cannot live in `/system`).
   Do **not** use `ksud module install` for upgrades — `su -c` here runs with `CapBnd=0` and it
   fails with `Permission denied`; unpack and `mv` the files in as root instead.
3. Install **KernelSU-Next Manager** APK — current: CI run [`36750739326`](https://github.com/KernelSU-Next/KernelSU-Next/actions/runs/36750739326) (`2b31f718` → `v3.4.0-19-g2b31f718-spoofed`), **uapi 4** like our kernel. The spoofed build ships a **randomised applicationId** (it has been `gojcms.hgelex.jabkht`), so `pm list packages | grep ksu` finds nothing — identify it by `versionName` instead. CI `34252913097` still needs the pending `88feb68` 4.14 port
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
cd anykernel && zip -r9 ../BadazzKernel-sweet-k6a-gov-v1.3.2.zip . -x "*.git*"
```

CI builds automatically on every push to `main` (release with build number) and to `miui/test` (artifacts only).
Runtime ~13–17 min: `gh run list` / `gh run watch <id>`.

---

## Status & Debug

```bash
dmesg | grep k6a_gov          # v1.5.0 loaded, hash verified, gpu levels, Build341 markers
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

## USB status

The v1.3.2 PC-bootloop fix landed. Builds 307–336 then worked through a chain of deeper faults.
**The blocker is closed — full enumeration now works.**

| # | Issue | State |
|---|-------|-------|
| **Fault 1** | `SETEPCFG` ep0out timeout → `RESTART_USB_SESSION` → gadget torn down and restarted every ~2 s until unplug | **Storm stopped** — Build 320 adds quiescent-core recovery + a give-up counter, Build 331 holds the dwc3 core PM reference across a connection |
| **Fault 2** | OTG host mode: `usb1-port1: Cannot enable. Maybe the USB cable is bad?` ×4 → `unable to enumerate` | **No longer reproduces** — retested on Build 336 with a USB stick: port enable, enumeration, `usb-storage`, SCSI and the vold mount all succeed |
| **Fault 3** | Charger: `APSD=OCP` re-runs every 5 s | **No longer reproduces** — retested 2026-10-02 on Build 338 with a 500 s capture (`printk 8 4 1 7`, `cap334.sh`, 40 491 lines): 0× `APSD=OCP`, 0× `Oops`/`power cycle`, `smblib_rerun_apsd` = 5 = exactly one per plug event (no 5 s loop), `vbus_notifier` = 9 lines in 3 bursts of 3 and only on `APSD=FLOAT` connect (×2) + the matching detach (×1) — never for DCP/HVDCP2 |
| **Fault 4** | **Instant whole-SoC reset the moment a USB-C cable is plugged in** (5 s of vibration, then reboot) | **Fixed in Build 336** — see below |

### Fault 4 — instant reset on attach (fixed in Build 336)

Plugging sweet into a Galaxy S21 reset the phone immediately: 5 s of vibration, then a reboot. For
several builds the log was completely silent, which pointed the investigation at PMIC hardware and
runtime suspend. Both were wrong.

The real cause was one uninitialized local in `ffs_func_eps_enable()`:

```c
struct ffs_data *ffs;                 /* never assigned before use ... */
ffs_log("enter: ...");                /* ... this reads ffs->ipc_log */
```

`ffs_log()` expands to `ipc_log_string(ffs->ipc_log, ...)` — a dereference of an uninitialized
pointer. It runs on **every** `SET_CONFIGURATION`, i.e. on the first real setup packet of any
enumeration, so the reset was host-independent; the S21 was simply the host that talked far enough
to trigger it. The resulting oops kills the dwc3 bottom-half work item, `panic_on_oops` turns it
into `panic=5` (the 5 s of vibration) and the PMIC performs a PS_HOLD power-cycle.

Three compounding problems made it invisible:

1. `panic_on_oops=1` + `panic=5` meant the oops never reached a durable log.
2. The power-cycle is reported as a **`'cold' boot`**, which wipes the 4 MB ramoops region — so
   `/sys/fs/pstore` and `/proc/last_kmsg` are always empty afterwards, even with
   `CONFIG_PSTORE_RAM=y`.
3. The capture script flushed too slowly, dropping everything written since its last sync.

**Build 335** added breadcrumbs at every step of `ffs_func_set_alt()`; **Build 336** fixes the bug.
Verified on device:

```
Build336: eps_enable ffs=ffffffde9e981800 ipc_log=ffffffdea3516200 ... count=2
Build335: set_alt eps_enable ret=0
Build335: set_alt done ok
android_work: sent uevent USB_STATE=CONFIGURED
```

Zero oopses, `/sys/class/udc/a600000.dwc3/state` = `configured`, `usb/online=1`,
`real_type=USB_PD`.

Faults 1–3 detail and the full evidence tables live in `AGENTS.md`.

**Fault 2 was retested on Build 336** with a USB stick on a USB-C OTG adapter and no longer
reproduces: `xhci-hcd` registers both buses, `usb 1-1: New USB device found`, `usb-storage`
creates `sdg`/`sdg1`, and vold mounts the volume. Zero `Cannot enable` / `power cycle` /
`unable to enumerate`. One trap worth knowing: Termux bundles the libaums *userspace* mass-storage
provider, and whenever it probes the stick it claims the interface and the disk disappears **without
a single line in `dmesg`** — that is userspace, not a kernel fault.

Empirical rule from all logs so far (Build 319):

| `DSTS` condition | ep-cmd result |
|---|---|
| `COREIDLE=1`, `USBLNKST!=3` | 9/9 OK |
| `COREIDLE=0` | 692/692 timeout |
| `COREIDLE=1`, `USBLNKST=3` (U3/Suspend) | 1/1 timeout |

Build 320 raised `CONFIG_LOG_BUF_SHIFT` 17 → 20 (128 KB → 1 MB), and the kernel boots with
`log_buf_len=2M loglevel=6`.

---

## Versions

| Version | Highlights |
|---------|------------|
| **Build 345 scope (K1–K7 + D) → release `build346`** | **Real build identity**: `drivers/thermal/Makefile` injects `git rev-parse --short=8 HEAD` as `K6A_GIT_HASH`/`K6A_BUILD_HASH` through `subdir-ccflags-y`, and both C files `#error` if the flag is missing instead of falling back to `full-synergy`. `verify_build_hash()` now compares the **whole** string (the old prefix `strncmp` accepted any runtime hash that merely started with the build one) and can report *unavailable*. **Fail-safe instead of fail-open**: a mismatch no longer turns enforcement off — it logs `pr_err`, sets `hash_state=2` (WebUI row turns red) and keeps the governor running. **K1** the gold cap is re-read after `cpufreq_update_policy()` and retried if it did not take; **K2** the gold cluster is identified by `gold_mask` (CPU 0 excluded, no more `?: 6` fallback) and the `CPUFREQ_ADJUST` notifier clamps every CPU of it; **K3** the battery guard latches until the pack is 5 K cooler; **K4** dwell is fixed (`fast` in, `normal` out) instead of being picked from a per-tick delta that flips with sensor noise; **K5** BW floors are written only on change and latched only after a successful readback; **K6** thermal zone lookups are cached; **K7** the boot default governor is `schedutil` instead of `performance`. Version stays **1.5.0** — vermagic and the hash do the locking. **Shipped and accepted on device 2026-10-03**. |
| **k6a_gov v1.5.0 (Build 341 scope → release `build344`)** | **`CONFIG_K6A_GOV=m`** — the governor ships as `k6a_gov.ko` inside the flash zip, is dropped at `/data/adb/k6a_gov.ko` and insmodded by k6a-ctl, so governor updates no longer need a kernel flash. **Version lock**: `hash_state` (0 pending / 1 verified / 2 mismatch / 3 not checked) with a bounded retry replaces a `hash_verified` latch that reported "verified" for a check skipped because `/sys` was not mounted yet; the per-boot `build/run version delta` warning is gone (the check was dropped — `LINUX_VERSION_CODE` stays pinned at `4.14.255` on purpose, see AGENTS.md, and vermagic does the real locking). Vermagic ties the `.ko` to exactly this build. **Shipped and accepted on device 2026-10-03**:
`version=1.5.0 hash_verified=1 hash_state=1`, `build hash verified (full-synergy) retries=0`.
Note the release is named `build344` — `build341`/`build342` are older docs-only builds and run
343 is the one that failed, because release names follow `github.run_number`. |
| **k6a_gov v1.4.0 (Build 340)** | Gold cap is released again on recovery/`enable=0`/`legacy=0` (`cpufreq_update_policy` instead of pokes into `policy->max`); **immediate escalation** L2→L3→L4 and L3→L4; entry dwell in GAMING (fast only for a ≥5 °C *rise*); temperature fail-safe holds the last state when no zone answers (`temp_src`, `temp_valid`); BW floors are actually released again (`k6a_devfreq_set_bw(…,0,0)` silently did nothing before); sysfs validation (all-or-none zero, monotonic caps, bounds) + `battery_guard_temp`, `profile` sanitised at init |
| **v1.3.2 (current)** | **USB fix**: SM6150 clocks, `WAIT_FOR_LPM` deadlock, `bus_aggr`/`GCTL` 50ms, `is_a_peripheral` — no PC bootloop; **MIUI**: `sweet_miui.config` overlay + DTS `xiaomi/sweet` + CI matrix `aosp`/`miui`; **CI**: build number in release/ZIP (`vX-buildXX`); **KSU**: submodule at `b100bd28` (runtime `38309` = `35000 + git`, UAPIv4) until `88feb68` 4.14 port on PC; userspace ksud + manager `v3.4.0-19-g2b31f718` (CI `36750739326`), spoofed APK with random applicationId |
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
| **USB attach → whole-SoC reset** (Fault 4) | USB-C into a Galaxy S21 → 5 s vibration + reboot, log empty, `/sys/fs/pstore` empty every time | `ffs_func_eps_enable()` used the local `struct ffs_data *ffs` in an `ffs_log()` **before** `ffs = func->ffs;` → `ipc_log_string(ffs->ipc_log, …)` dereferences an uninitialised pointer (`+0x1a8`, x0=0) → oops kills `dwc_wq` → `panic=5` → PS_HOLD `'cold' boot` wipes ramoops | **Build 336**: assign `ffs` before the `ffs_log`; **Build 335** breadcrumbs proved it. Verified: `USB_STATE=CONFIGURED`, UDC `configured`, zero oops |
| **Identical Release Names** | All GitHub Releases/ZIPs named identically (`v4.14.369`) | `build-kernel.yml` `name: "Badazz-kernel v${VERSION}"` + ZIP `v${VERSION}.zip` without `run_number` | CI now `name: "v${VERSION}-badazz-build${run_number}"` + ZIP `v${VERSION}-build${run_number}[ -miui].zip` + artifact `…-buildXX-variant` |
| **KSU Manager "update required"** | Manager CI `34252913097` (`88feb68` `e801e16` version matching) on kernel `a5ff54c` → red banner | `e801e16` new UAPI + `bundled_lkm` check, old kernel UAPI mismatch; `88feb68` needs `KPROBES` + 5.x APIs (`syscall_fn_t`, `pgtable.h`, `lsm_hook`) not 4.14 compatible | Fork `dev` → `88feb68` for PC, hotfix `CONFIG_KPROBES=y` + `syscall_fn_t`/`pgtable`/`ksys_close` guards, then **revert** to the current line (`b100bd28`, UAPIv4) until a proper 4.14 port exists. **Resolved 2026-10-02**: manager + ksud `v3.4.0-19-g2b31f718` (CI `36750739326`, uapi 4) run against our kernel `GET_INFO version=38309 ≥ 34634` with no red banner — the old `34142634591` pin is obsolete |
| **MIUI/HOS not booting** | Stock HyperOS `V14.0.1.0` `4.14.190-perf` needs `ARCH_SM6150/CAMCC/GCC/PDC` + `xiaomi/sweet` DTS (`GTX9896_K6`, `FPC1540`) | `sweet_defconfig` flattened to `atoll/sm6150`, `QUSB2` only | `sweet_miui.config` overlay (7 lines) + `xiaomi/sweet` DTS shim + CI matrix `aosp`/`miui` (`miui/test` artifacts, `main` release `…-miui.zip`) |

---

## Roadmap

| Priority | Item | Status | Next Step |
|----------|------|--------|-----------|
| **P0** | **KSU 88feb68 4.14 Port on PC** | `badazzrebase.md` ready | `k6a-sweet` rebase `b100bd28` → `88feb68` (Squash 49 files, `meld` for `Kconfig/Makefile/selinux.c` `susfs_is_current`), new branch `k6a-sweet-88feb68`, Badazz bump, CI green ~15 min |
| **P0** | **SUSFS full restore** | Hotfix relaxed validation, `fs/susfs` `v2.2.0` intact but KSU side `k6a-sweet` patches pending | After rebase: `CONFIG_KSU_SUSFS` + `TAMPER_SYSCALL_TABLE` validation back, `nm vmlinux \| grep susfs_is_current` green |
| **P1** | **MIUI/HOS verification** | `sweet_miui.config` + DTS + `miui/test` CI `aosp+miui` artifacts | Flash `…-miui.zip` on HyperOS `V14.0.1.0` (or `2.0`), test `dmesg` `fpc/goodix/touchfeature/ds28e16`, 120Hz, NFC, `usb` `host/device` |
| **P1** | **USB Fault 3 (charger)** | **Closed 2026-10-02** — 500 s capture on Build 338 (`printk 8 4 1 7`, 40 491 lines): 0× `APSD=OCP`, `smblib_rerun_apsd` exactly once per plug event, `vbus_notifier` only on FLOAT connect/detach | Nothing to fix; keep `~/tmp/cap334.sh` as the recipe if it ever returns |
| **P1** | **k6a_gov → loadable module** | **Done in Build 341 (`c42a67d07`), flashed as `build344` 2026-10-03** — `CONFIG_K6A_GOV=m`, CI builds `modules` and ships `k6a_gov.ko`, `anykernel.sh` drops it at `/data/adb/k6a_gov.ko` (`/system` is dm-verity, not writable), k6a-ctl insmods it with a three-stage version lock and the legacy cooldown as fallback | Verified on device: `version=1.5.0`, `hash_state=1` |
| **P1** | **Build 345 governor scope (K1–K7 + D)** | **Done — released as `build346`, flashed and verified 2026-10-03** — real git hash in the build, fail-safe hash mismatch, gold-cap readback, `gold_mask` clamp, battery latch, fixed dwell, BW change-detect, zone cache, `schedutil` boot default | Post-flash checklist green: `Build345:` markers, `hash_state=1`, `build_hash` = `git_hash` = `699eee6e`, `gold cap … policy_max=…` with 0 retries, 70/70 `policy_max == scaling_max_freq`, dwell 2 s out, 0 `set_bw` retries, battery 39.5 °C → `batt_latched=0`, idle 75 °C |
| **P1** | **k6a-ctl sync** | **Done** — repo at **v1.3.2 (`768d1bc`)**: `check_module.sh` gates `[1]`–`[6]` incl. the governor checks, `battery_guard`/`_temp`, `policy_max`/`state_age_ms`/`temp_src`/`temp_valid` WebUI keys, **F1 forces `schedutil` and re-asserts it on drift**, **device runs v1.3.2** | Nothing outstanding |
| **P2** | **Release hygiene** | `main` `v1.3.2` + `k6a-ctl` `v1.3.2` versioned ZIPs | Releases are `v4.14.369-badazz-buildXX` automatically via CI; next `main` release gets a changelog |
| **P2** | **Docs** | `README.md` + `AGENTS.md` refreshed at `build346` (module conversion, `LINUX_VERSION_CODE` revert, the `hash_state=3` SELinux finding, the Build-345 scope + its post-flash record) | `Documentation/` is stock Linux 4.14 — leave it alone, keep project docs in the root `*.md` |

---

## Download

| Source | Link |
|--------|------|
| BadazzKernel Releases | [GitHub](https://github.com/vandalsquad187/BadazzKernel/releases) |
| k6a-ctl (Companion) | [GitHub](https://github.com/vandalsquad187/k6a-ctl) |
| Rebase Plan (PC) | `/sdcard/Download/badazzrebase.md` |

**Repo docs**: `README.md` (public), `AGENTS.md` (dev/agent state + build & debug recipes),
`CONTRIBUTING.md` (upstream kernel guide). `Documentation/` is the **stock Linux 4.14**
kernel documentation (6090 files) — kernel-internal API docs, nothing project-specific.

---

## Credits

- **BadazZ89** — Kernel, k6a_gov
- **vandalsquad187** — Base, CI
- **KernelSU-Next / SUSFS** — Root/Hide (submodule `b100bd28`, UAPIv4, SUSFS `v2.2.0`)
- **AnyKernel3** — Flash template
- **MiDoNaSR545** — Reference for MIUI/HOS (`sweet_k6a-r-oss`)

---

## License

GPL v2 — see `COPYING`.
