# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/BadazzKernel` branch `main`
- **Kernel**: `4.14.369` — `K6A_GOV v1.3.1` built-in, `LOCALVERSION=-BadazzKernel-sweet-v1.3.2`
- **Local**: clean, `main` @ `3be72b736` (Build 320: quiescent-core recovery + RESTART_USB_SESSION storm breaker)
- **GitHub**: CI builds on every push to `main` (release) and `miui/test` (artifacts only); latest release `v4.14.369-badazz-build320`
- **KSU-Next Submodule**: `b100bd28` (`v3.3.0-95-gb100bd28`, dev-4.14-prctl-fix, UAPIv4, `KSU_VERSION=33300`)

## Repo & Docs Layout

| Path | What it is |
|------|------------|
| `README.md` | Public, English. Features, install, versions, fixed bugs, roadmap |
| `AGENTS.md` | This file — agent/dev-facing state, build & debug recipes |
| `CONTRIBUTING.md` | Upstream kernel contribution guide (not project-specific) |
| `Documentation/` | **Stock Linux 4.14 kernel docs** (6090 files, `00-INDEX`, ABI/, devicetree/, …). No project content — do not put Badazz docs here |
| `anykernel/` | AnyKernel3 flash template used by CI |
| `.github/workflows/build-kernel.yml` | The CI that produces the flashable ZIP + release |

## CI (`build-kernel.yml`)

- Trigger: push to `main` (release) / `miui/test` (artifact only) / `workflow_dispatch`
- Toolchain: distro `gcc-aarch64-linux-gnu` + `gcc-arm-linux-gnueabihf` (GCC, **not** clang), ccache
- Steps: `make sweet_defconfig` → inject `CONFIG_LOCALVERSION=…-build${run_number}` → `olddefconfig`
  → validate required configs (`KSU`, `KSU_SUSFS`, `KSU_TAMPER_SYSCALL_TABLE`, `K6A_GOV`,
  `NFC_NQ_PN80T`, `PHY_QCOM_QUSB2`) → `make Image.gz dtb.img dtbo.img`
  → verify `CONFIG_PHY_QCOM_QUSB2` survived + `phy-qcom-qusb2.o` was built
  → copy into `anykernel/` + `build-info.txt` → zip `Badazz-kernel-sweet-v${VERSION}-build${run_number}.zip`
  → release `v${VERSION}-badazz-build${run_number}`
- Build time ~13–17 min; `gh run list` / `gh run watch <id>` to follow

## USB Debugging (Build 300–320, open)

Status: **device mode still fails intermittently (Fault 1), host mode broken, Build 320 pushed, awaiting on-device test.**

Key files: `drivers/usb/dwc3/gadget.c`, `drivers/usb/dwc3/dwc3-msm.c`,
`drivers/power/supply/qcom/smb5-lib.c`, `arch/arm64/boot/dts/qcom/sdmmagpie-*.dts(i)`.
Known-good reference commits: `096e0a0c9`, `d8cf6ae19` (high-speed DTS), `79b3b4147` (bus_aggr).

Empirical rule across **all** logs (Build 319):

| Condition | Result |
|---|---|
| `DSTS.COREIDLE=1` && `USBLNKST!=3` | 9/9 ep-cmd OK |
| `DSTS.COREIDLE=0` | 692/692 ep-cmd timeout |
| `DSTS.COREIDLE=1` && `USBLNKST=3` (U3/Suspend) | 1/1 timeout (host never bus-reset) |

**Fault 1 — ep-cmd timeout storm** (open, Build 320 is the current attempt)
- Chain: `run_stop(0)` does not see end-of-frame → core left `COREIDLE=0/HLT=0`
  → next `__dwc3_gadget_start()` programs EP0, `SETEPCFG` hangs 5 s → `RESTART_USB_SESSION`
  → `dwc3_restart_usb_work` tears down + restarts → same failure every ~2 s until unplug
- Build 307–319 added diagnostics (`ep cmd dump`, `Build313 PRESTART/STARTFAIL`, `Build319 prestart not quiescent`)
- Build 320 adds `dwc3_gadget_ensure_quiescent()` (≤50 ms wait + one `DCTL.CSFTRST`, process context
  in `dwc3_otg_start_peripheral`) and `start_fail_streak >= 3` give-up in `run_stop(is_on=1)`
  so no ep-cmd is issued and the storm stops. Streak clears on successful start or a *real*
  disconnect (`!mdwc->in_restart`).
- Markers to grep: `Build320:`, `Build313 STARTFAIL`, `Build319: prestart not quiescent`, `STARTGIVEUP`

**Fault 2 — host mode**: `usb1-port1: Cannot enable. Maybe the USB cable is bad?` ×4 + `attempt power cycle`
then `unable to enumerate` (see `dm313otg.txt`). Not touched by Build 320.

**Fault 3 — charger**: `APSD=OCP` rerun loop every 5 s. Expected to produce **no** `vbus_notifier` line —
`smblib_handle_apsd_done()` only calls `smblib_notify_device_mode()` for SDP/CDP/FLOAT (smb5-lib.c:8021).

**Ring buffer**: `CONFIG_LOG_BUF_SHIFT` was 17 (128 KB) and wrapped between t=4268 s and t=5942 s,
losing the whole Fault 1 window. Build 320 bumps it to 20 (1 MB).

## Local Build Notes (Termux)

A full local kernel build is **not** possible here: `scripts/mod/modpost` fails to link against
Termux/bionic `elf.h` (`ELF64_ST_TYPE` circular), there is no `bison`/`perl`, and no
`aarch64-linux-gnu-gcc`. Builds go through CI instead. Use a targeted syntax check for edits:

```bash
cd BadazzKernel && make ARCH=arm64 sweet_defconfig   # regenerates .config
CLANG_INC=$(clang -print-resource-dir)/include
clang --target=aarch64-linux-gnu -nostdinc -isystem $CLANG_INC \
  -I./arch/arm64/include -I./arch/arm64/include/generated -I./include \
  -I./arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi \
  -I./include/uapi -I./include/generated/uapi -I./drivers/usb/dwc3 \
  -Idrivers/usb/host -Idrivers/base/power \
  -include ./include/linux/kconfig.h -D__KERNEL__ -DMODULE -mlittle-endian \
  -std=gnu89 -Werror=implicit-function-declaration -Werror=format \
  -fsyntax-only drivers/usb/dwc3/gadget.c
```

## k6a_gov v1.3.1

### Location
- `drivers/thermal/k6a_gov/k6a_gov.c` (997 lines, `CONFIG_K6A_GOV=y`)
- `drivers/thermal/k6a_gov/Kconfig` / `Makefile`
- `drivers/gpu/msm/kgsl_pwrctrl.c` — `kgsl_k6a_get_levels()` + `kgsl_k6a_set_max_level_idx()`
- `drivers/devfreq/devfreq.c` — `k6a_devfreq_get_bw()` + `k6a_devfreq_set_bw()`

### Features
- State Machine: OFF→GAMING→CD_L2/L3/L4, hysteresis fast/normal + dwell
- Temp: max over 4 Gold zones `cpu-1-0..3-usr`, fallback `xo-therm`/`soc-therm`
- CPU: `find_gold_cpu()` portable, `clamp_freq` order-independent, `enforce_max_freq` + `cpufreq_update_policy` (kthread only)
- GPU: native enforcement via KGSL pwrlevels, caps per CD state
- BW: floors `gpubw` + `cpu-llcc-ddr-bw` per profile/CD state, `bw_floors` sysfs, reset on disable
- Battery: `battery_guard` @45°C → CD_L2 via `power_supply`
- Poll: `poll_ms` 100..5000
- Profiles: 0 off, 1 gaming, 2 battery, 3 badazz, 4 custom, 5 badazz_safe
- Safety: `verify_build_hash` vs `k6a_features/git_hash`, `hash_verified` in status
- History: `K6A_HIST_N=16` ringbuffer, `hist=` in status
- Sysfs: `enable/profile/status/hysteresis/cd_thresholds/gpu_caps/bw_floors/battery_guard/poll_ms/legacy/game_pid`
- Hardening: `get_cd_max_freq` lock-free (caller holds lock), `cool_cur`/`status_show` locked, `freq_init_worker` mutex, ticks fix

### sweet_defconfig
- `CONFIG_K6A_GOV=y`
- `CONFIG_KSU=y` `33300` UAPIv4, `CONFIG_KSU_SUSFS=y` + all sub-options + `TAMPER_SYSCALL_TABLE`
- `CONFIG_SCHED_TUNE=y` `CONFIG_KSM=y` `CONFIG_BOEFFLA_WL_BLOCKER=y`
- `CONFIG_MSM_PERFORMANCE=y` `CONFIG_CPU_FREQ_TIMES=y` `CONFIG_PSI=y`
- `CONFIG_LOG_BUF_SHIFT=20` (1 MB ring buffer, raised from 17 in Build 320)
- `LOCALVERSION="-BadazzKernel-sweet-v1.3.2"`

### Build
```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-gnueabihf- sweet_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-gnueabihf- -j$(nproc) \
     Image.gz dtb.img dtbo.img
# single object:
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- drivers/thermal/k6a_gov/k6a_gov.o
# ZIP (CI does this — see .github/workflows/build-kernel.yml):
cp arch/arm64/boot/Image.gz arch/arm64/boot/dtb.img arch/arm64/boot/dtbo.img anykernel/
cd anykernel && zip -r9 ../BadazzKernel-sweet-k6a-gov-v1.3.2.zip . -x "*.git*"
```
Local full builds are blocked in Termux — see **Local Build Notes (Termux)** above.

### Git History (main)
Full log: `git log --oneline -40`. Current head is the USB debug series:

```
3be72b736 Build 320: recover a non-quiescent device core before start + stop the RESTART_USB_SESSION storm
71dc0630b Build 319: restore GUSB3PIPECTL.SUSPHY lost by stop_peripheral + wait for DSTS.COREIDLE
1fd9bdade Build 318: drop Build-311 core-soft-reset heal + dump GEVTEN/DEVTEN/EVSIZ/EVCNT on ep-cmd timeout
3ba2e4675 Build 317: run_stop no longer aborts on start failure + restore high-speed DTS
194e605be Build 316: restore bus_aggr/noc_aggr clocks in start_peripheral + PM/clock/bus-vote diagnostics
6c6a35c9b Build 315: ep-cmd wait budget = wall time (udelay 10us), not read count
ed9e0ef1d Build 314: QUSB2 PHY power/DPDM/set_suspend diagnostics (no behavior change)
bff0adc79 Build 313: link/PHY state + DEVT diagnostics (no behavior change)
0c88aa2d4 Build 312: run_stop wait real frame time — 1500 tight reads < 1ms HS frame
7bdc78a1d Build 307: drop Build 306 core reset, enable AHB/AXI clocks, dump EP-CMD state on timeout
839d4d70a Build 303: restore working USB state (Sept 8 build 096e0a0c9)
```

k6a_gov history: `81d69ae` v1.3.1 ticks fix, `d4835b6` deadlock, `53bb809` v1.3.1 hardening,
`967c134` v1.3.0 BW floors + profile 5, `dfcb96b` v1.2.1, `602a281` `CONFIG_K6A_GOV=y`.

## k6a-ctl Companion
- **Repo**: `vandalsquad187/k6a-ctl` branch `main` @ `b130d68`
- **Mode**: `delegated=1` — Kernel handles thermal (CPU/GPU/BW), module handles game detection + sched + auto `badazz_safe` @85°C
- **WebUI**: Gov-Status, BW-Floors, GPU-Caps, History timeline
- **Check**: `bin/check_module.sh` delegation-aware gate
- **Synergy**: `k6a-ctl` writes `profile`/`enable` to `/sys/kernel/k6a_gov/`, reads `status`

## KernelSU-Next SUSFS
- SUSFS in `fs/susfs.c`, `include/linux/susfs.h`
- Hooks in `fs/stat.c` (`CONFIG_KSU_SUSFS_SUS_KSTAT`) and `kernel/sys.c` (`CONFIG_KSU_SUSFS_SPOOF_UNAME`)
- Submodule `KernelSU-Next` @ `b100bd28` (`v3.3.0-95-gb100bd28`, `KSU_VERSION=33300`, UAPIv4)
- `git submodule update --init --recursive` required for fresh clone

## Common Issues
1. **Fresh clone build fails**: need `CONFIG_KSU_SUSFS=y` + sub-options, submodule init
2. **SUSFS implicit declaration**: guard calls with `#ifdef CONFIG_KSU_SUSFS_*`
3. **Deadlock on cat status**: fixed in d4835b6 — `get_cd_max_freq` must not take mutex
4. **Hardcoded CPU6**: fixed — use `find_gold_cpu()` portable
5. **Boot hang at crDroid logo** (Build 267-278): see Boot-Hang Root Cause below
6. **Local Termux build fails** (`modpost` / `elf.h`, no `bison`/`perl`): use the syntax-check recipe in *Local Build Notes (Termux)*, let CI do real builds
7. **USB `ep cmd timeout` / device not enumerating**: see *USB Debugging (Build 300-320, open)*

## Boot-Hang Root Cause (Build 267-278)
- **Symptom**: Boot hängt bei crDroid Boot-Logo (95%), intermittierend
- **Regression-Commit**: `be83f8e5` — aktiviert `ksu_execve_hook_ksud_common()` auf 4.14 (execve-Hook für init second_stage + zygote)
- **Root Cause**: **Zygisk Next + Magic Mount RS** Modul-Kombination verursacht den Hang
- **Beweis**: 
  - Build 273 + Zygisk Next + Magic Mount RS → hängt
  - Build 273 + Brezygisk + Hybrid Mount → bootet 100%
  - OrangeFox "Fix SELinux contexts" rettet den Boot (Workaround, nicht Fix)
- **Nicht der Kernel**: `apply_kernelsu_rules()` im execve-Hook ist NICHT der Auslöser — der Hook ist seit `be83f8e5` aktiv und funktioniert korrekt mit den richtigen Modulen
- **Workqueue-Fix (Build 277/278) nicht nötig**: War ein Umweg, verursachte zusätzlich EACCES in der Allowlist

## Module-Kompatibilität

### Getestet und funktioniert
| Modul | Status | Anmerkung |
|---|---|---|
| **Brezygisk** | ✅ | Zygisk-Alternative, kompatibel mit 4.14 KernelSU-Next |
| **Hybrid Mount** | ✅ | Systemless Mount, kompatibel |
| **k6a-ctl** | ✅ | Kernel-eigen, immer kompatibel |
| **nfc_sweet2_fix** | ✅ | Kernel-Modul |
| **FastCharging** | ✅ | Kernel-Modul |
| **padazz89** | ✅ | Kernel-Modul |
| **tricky_store** | ✅ | Key Attestation |

### Inkompatibel / Verursacht Boot-Hang
| Modul | Status | Anmerkung |
|---|---|---|
| **Zygisk Next** | ❌ | Verursacht Boot-Hang in Kombination mit Magic Mount RS |
| **Magic Mount RS** | ❌ | Verursacht Boot-Hang in Kombination mit Zygisk Next |

## NFC sweet2 PN557 (bewusst so)
- `CONFIG_NFC=n` + `CONFIG_NFC_NQ=n` bewusst — `net/nfc` (pn544/pn533) ungenutzt, NCI liegt in userspace
- `CONFIG_NFC_NQ_PN80T=y` bewusst — hängt nur an `I2C` (`drivers/nfc/Kconfig`), liefert `/dev/nq-nci`, kein `CONFIG_NFC` nötig
- Fix liegt **nicht** im Kernel: `Sweet2NfcFIX v1.3-lottery` Modul (`github.com/vandalsquad187/Sweet2NfcFIX`) — `0x85` + `RF` + `/dev/nxp-nci` + `chcon`
- `CONFIG_NFC=y` als Test bringt 0 Nutzen (~200K toter Code) — nicht aktivieren

## TODO Kernel Tunings (backlog, nicht gepusht)

- [ ] **1. Sched+VM Tune** (`f5aaa8b` DarkKiller28): `init.qcom.post_boot.sh` — `sched_down/upmigrate 45/65 + 65/85`, BORE `sched_boost/latency 6ms/min 1ms/wakeup 0.5ms/migration 0.25ms/nr_migrate 64/burst_*`, VM `watermark_scale 35/dirty_ratio 30/expire 1500/writeback 150/extra_free 131072/min_free 32768` — *Meinung: `65/85` + `watermark/dirty` sinnvoll, `swappiness 160` + `extra_free 131k` akku-kritisch, selektiv testen (HZ1000+KSM bleiben)*
- [ ] **2. Reflex Governor** (`80a346a`): `schedutil 500/20000 → reflex` — braucht `CONFIG_CPU_FREQ_GOV_REFLEX` Port (~500 LOC), schedutil bleibt via `k6a-ctl` + `JUMP_LABEL` (Meinung: erst bei Reflex-Port, nicht jetzt)
- [x] **3. Audio BT v7** (`182eb86`): `audio_policy_configuration.xml` −15 BT A2DP + `sm6150.mk` +1 `bluetooth_audio_policy_configuration_7.0.xml` — *Device, Low-Risk, geplant als 4-Step Cherry-Pick (21 Zeilen)*

## Device Cherry-Pick
- **Quelle**: `DarkKiller28/android_device_xiaomi_sm6150-common@182eb86`
- **Ziel**: `device/xiaomi/sm6150-common` — `configs/audio/audio_policy_configuration.xml` (remove 3× `BT A2DP` ports + 3× `route`) + `sm6150.mk` (`PRODUCT_COPY_FILES` `bluetooth_audio_policy_configuration.xml → /vendor/etc/bluetooth_audio_policy_configuration_7.0.xml`) + `xi:include` nach `r_submix`
- **Steps**: `git fetch DarkKiller28 182eb86 && git cherry-pick -x 182eb86` → `mka` → `adb ls /vendor/etc/bluetooth*.xml` + BT Pair
