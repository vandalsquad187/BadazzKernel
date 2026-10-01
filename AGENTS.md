# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/BadazzKernel` branch `main`
- **Kernel**: `4.14.369` — `K6A_GOV v1.3.1` built-in, `LOCALVERSION=-BadazzKernel-sweet-v1.3.2`
- **Local**: clean, `main` @ `4dcd00df3` (Build 333: every SMB5 IRQ now logs its name + markers on the
  previously blind Type-C handlers)
- **GitHub**: CI builds on every push to `main` (release) and `miui/test` (artifacts only);
  latest release `v4.14.369-badazz-build333` (CI run `36892664057`, success)
- **KSU-Next Submodule**: `b100bd28` (`v3.3.0-95-gb100bd28`, dev-4.14-prctl-fix, UAPIv4, `KSU_VERSION=33300`
  in `KernelSU-Next/kernel/Makefile`)
- **Open blocker**: Fault 4 — instant whole-SoC reset on USB-C attach, **zero software trace**
  (see *USB Debugging* below)
- **User speaks German**; device reports go out in German

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

## USB Debugging (Build 300–333, open)

Status: **Faults 1–3 addressed in Builds 320/331/325. The blocker is Fault 4 — an instant
whole-SoC reset on USB-C attach with no kernel trace at all. Build 333 is the current attempt.**

Key files: `drivers/usb/dwc3/gadget.c`, `drivers/usb/dwc3/dwc3-msm.c`, `drivers/usb/dwc3/core.c`,
`drivers/power/supply/qcom/smb5-lib.c`, `drivers/power/supply/qcom/qpnp-smb5.c`,
`arch/arm64/boot/dts/qcom/sdmmagpie-*.dts(i)`.
Known-good reference commits: `096e0a0c9`, `d8cf6ae19` (high-speed DTS), `79b3b4147` (bus_aggr).

### Fault 4 — instant reset on USB-C attach (BLOCKER, open)

Repro (user): *S21 mit Bildschirm aus an sweet stecken → sofort 5 s Vibration + Reboot.*

Build 332 evidence, captured with the direct `/dev/kmsg` reader (see *On-device Capture*) and
**seq gaps = 0**, i.e. the log is provably loss-free:

| | run 1 (`control=auto`) | run 2 (`control=on`) |
|---|---|---|
| death | 17:26:50.84 | 18:20:25.9 |
| last heartbeat | `hb n=4403` `dj=504` | `hb n=2453` `dj=504` |
| lines after `capture_start` | 368 | 159 |
| `typec_irq`/`vbus_nb`/`usbin-plugin`/`notify_device_mode` | 0/0/0/0 | 0/0/0/0 |
| runtime PM | `suspended` ×499 polls | `active` the whole time |
| `usb/online` at 14 Hz | — | stayed `0`, only 1 `CHANGE` (initial line) |
| last Type-C IRQ of the boot | 17:22:20 (4½ min early) | none in window |

What this proves:
- **Suspend/runtime-PM is not the cause.** With `control=on` and `runtime_status=active` the device
  still dies, so Build 330/331 were not it.
- The attach reaches **no software observer whatsoever** — no charger IRQ, no VBUS notifier, no
  `usb/online` flip, no USB intent, no Android vibrator call (last `vendor.qti.vibratorOL` = 17:21:45).
- Reset signature is identical every time:
  `PMIC@SID0 Power-off: Triggered from PS_HOLD (PS_HOLD/MSM Controlled Shutdown)` +
  `PMIC@SID0 Power-on: Hard Reset and 'cold' boot` + `PMIC@SID4 Power-off: SOFT (Software)`
  → SoC-initiated reset, not UVLO/SMPL/overcurrent.
- The 5 s vibration is **not** Android — the haptic rail latches during the reset. Duration matches
  `panic=5`.
- **No lockup detectors are compiled**: `CONFIG_SOFTLOCKUP_DETECTOR=n`, `CONFIG_DETECT_HUNG_TASK=n`,
  `CONFIG_HARDLOCKUP_DETECTOR=n`, `CONFIG_WQ_WATCHDOG=n` → a freeze produces nothing but the
  heartbeat/poller going silent. `CONFIG_PREEMPT=y`, `CONFIG_PANIC_ON_OOPS=y`, `CONFIG_DEBUG_BUGVERBOSE=y`.

**pstore/ramoops cannot record this fault**: `CONFIG_PSTORE=y`, `console [pstore-1] enabled`,
`ramoops attached 0x400000@0xb0000000`, yet after *every* crash `/sys/fs/pstore` holds 0 files and
`/proc/last_kmsg` is 0 bytes (no `SYSTEM_LAST_KMSG` either). The PMIC reports a `'cold'` boot, which
wipes the reserved region. Real-time capture is the only channel that exists.

Two separate failures caused the blind spot:
1. Handlers logged only via `smblib_dbg`, which is compiled out (Build 332's marker sat *after* the
   micro-USB early return).
2. Each SMB5 IRQ registered its own handler directly, so there was no single place that could prove
   an IRQ fired.

**Build 333** closes both: `b333_irq()` in `qpnp-smb5.c` is registered for *every* SMB5 IRQ, prints
`Build333: irq <name>` first, then forwards to the original handler (`smb5_irqs[i].irq_data` is now
assigned *before* `devm_request_threaded_irq`, cleared again on failure). `smb5-lib.c` gained entry
markers in `typec_state_change_irq_handler` (before the micro-USB return),
`typec_attach_detach_irq_handler`, `typec_or_rid_detection_change_irq_handler` and
`usb_plugin_irq_handler`.

### Fault 1 — ep-cmd timeout storm (storm stopped in Build 320)

- Chain: `run_stop(0)` does not see end-of-frame → core left `COREIDLE=0/HLT=0`
  → next `__dwc3_gadget_start()` programs EP0, `SETEPCFG` hangs 5 s → `RESTART_USB_SESSION`
  → `dwc3_restart_usb_work` tears down + restarts → same failure every ~2 s until unplug
- Build 320 adds `dwc3_gadget_ensure_quiescent()` (≤50 ms wait + one `DCTL.CSFTRST`, process context
  in `dwc3_otg_start_peripheral`) and `start_fail_streak >= 3` give-up in `run_stop(is_on=1)`
  so no ep-cmd is issued and the storm stops. Streak clears on successful start or a *real*
  disconnect (`!mdwc->in_restart`).
- Markers: `Build320:`, `Build313 STARTFAIL`, `Build319: prestart not quiescent`, `STARTGIVEUP`

Empirical rule across **all** logs (Build 319):

| Condition | Result |
|---|---|
| `DSTS.COREIDLE=1` && `USBLNKST!=3` | 9/9 ep-cmd OK |
| `DSTS.COREIDLE=0` | 692/692 ep-cmd timeout |
| `DSTS.COREIDLE=1` && `USBLNKST=3` (U3/Suspend) | 1/1 timeout (host never bus-reset) |

### Fault 2 — host mode
`usb1-port1: Cannot enable. Maybe the USB cable is bad?` ×4 + `attempt power cycle` then
`unable to enumerate` (see `dm313otg.txt`). Not touched since Build 320.

### Fault 3 — charger
`APSD=OCP` rerun loop every 5 s. Expected **no** `vbus_notifier` line —
`smblib_handle_apsd_done()` only calls `smblib_notify_device_mode()` for SDP/CDP/FLOAT
(smb5-lib.c:8021).

### Ring buffer
`CONFIG_LOG_BUF_SHIFT=20` (1 MB, raised from 17 in Build 320) **and** `log_buf_len=2M loglevel=6`
on the kernel command line. Even so the buffer empties in seconds because of haptic + fuel-gauge
log spam — it does **not** hold the fault window. Use the capture recipe below.


## On-device Capture (Termux) — logcat is useless for kernel faults

Scripts live in `~/tmp/`. Do **not** use `/tmp` (not writable in Termux) and do not try to write
`/data/local/tmp` from Termux (only root can).

| Script | What |
|---|---|
| `~/tmp/cap332.sh` | `start\|stop\|pstore\|scan` — the working driver |
| `~/tmp/cap_scan.sh` | `kill\|count` — matches `readlink /proc/<pid>/fd/1` against `*/tmp/live/*` |
| `~/tmp/pm_poll.sh` | root poller ~14 Hz: `runtime_status\|online\|real_type\|type\|status\|capacity`, prints `CHANGE` only on a real state change |

`cap332.sh start` spawns under `setsid` + root:

1. **`dd if=/dev/kmsg bs=65536`** → `~/tmp/live/kmsg332.log`. This is the only proven loss-free
   channel — gaps = 0 over the last 159 and 368 lines of two separate crashes. `dd` writes one
   block per read, so it is unbuffered. **`cat /dev/kmsg` fails with `EINVAL`** (buffer too small).
2. logcat `main`/`system`/`events` with `stdbuf -oL`.
3. `pm_poll.sh`, then a `BuildNNN: capture_start` banner marker (printk, `T0`, banner, heartbeat,
   control/runtime state, battery, `usb/online` + `real_type`).

`/dev/kmsg` line format is `seq,ts_us_since_boot,-;msg`. Seq deltas give the gap check;
`ts_us_since_boot` + `T0` from the banner converts to wall clock.

Gotchas learned the hard way:
- Capture files end **NUL-padded** → `tr -d '\0'` before grepping.
- **Orphan hazard**: any worker whose stdout is still the tool's pipe hangs the tool forever.
  Always redirect worker stdout to a file.
- Kill via `cap_scan.sh kill` (fd-target matching). **Never** `pkill -f` — the pattern matches the
  scanning shell's own cmdline. **Never** `ps | grep` — `hidepid` hides root procs from Termux.
- Full root path is **`/system/bin/su`** (KSU `sucompat.c` intercepts `execve` on `su`), and use
  full paths inside `su -c` (`/system/bin/dumpsys`, `tr`, …). SELinux is Enforcing.
- SUSFS spoofs `uname` → `6.12.0-android16-6.12-perf-g1d01cd293`; the real banner is
  `Linux version 4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-buildNNN`. It leaves the ring
  within ~1 h, so prove the build with `BuildNNN: hb` lines, not the banner.

```bash
bash ~/tmp/cap332.sh start      # then reproduce the fault
bash ~/tmp/cap332.sh pstore     # IMMEDIATELY after the reboot: /sys/fs/pstore + PMIC/LMK/tombstone dump
bash ~/tmp/cap332.sh stop
tr -d '\0' < ~/tmp/live/kmsg332.log | grep -E "Build333|Build332"
```

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
  -Idrivers/power/supply -Idrivers/power/supply/qcom \
  -include ./include/linux/kconfig.h -D__KERNEL__ -DMODULE -mlittle-endian \
  -std=gnu89 -Werror=implicit-function-declaration -Werror=format \
  -fsyntax-only drivers/power/supply/qcom/qpnp-smb5.c
```

**Accepting a non-zero RC**: the CI toolchain is **GCC**, so clang `-Werror=format` hits pre-existing
bugs that never break the release. Always compare against the pristine file:

```bash
git show HEAD:drivers/power/supply/qcom/smb5-lib.c > ~/tmp/smb5_pristine.c
# same command on both files — accept only if the error set is identical
```

`smb5-lib.c` currently reports 4 such errors (`1364`, `7798`, `8247`, `9904` pristine / `1364`,
`7799`, `8248`, `9908` after the Build 333 markers shifted the lines).

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
4dcd00df3 Build 333: cover every SMB5 IRQ + markers on the blind Type-C handlers
c576e8ab5 Build 332: global kernel heartbeat + Type-C/VBUS path markers
524ca3253 Build 331: hold the dwc3 core PM reference across a connection
4279f358b Build 330: block dwc3 runtime-suspend while the gadget is attached
039dbbf34 Build 329: bracket the post-unmask window in dwc3_process_event_buf
88df6a2a8 Build 328: breadcrumb the blind window after the EP0 STATUS-phase STARTTRANSFER
a74daf999 Build 327: breadcrumb the EP0 setup path to pin the enumeration freeze
8c24c0d63 Build 326: stall instead of oopsing when an ffs function has no ffs_data
efbc602fb Build 325: drop the debug serial console that was stalling the system
32a3e8e6b Build 324: clock the QUSB2 PHY before the Build-323 re-init + make clock enable idempotent
66002baa6 Build 323: re-init the QUSB2 PHY after the DPDM reset when dwc3 is awake
5613540bf Build 321: trust DALEPENA over a stale EP_ENABLED flag so EP0 actually gets enabled
2879f0389 docs: refresh README.md + AGENTS.md at Build 320, document repo layout
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

Note: **Build 331 (`524ca3253`) was released but never flashed** — testing jumped from 330 to 332.

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

## Working Agreements

- **No code comments** — rationale lives in the commit body and in `pr_info` markers.
- **Marker style**: `pr_info("BuildNNN: …\n", …)`; grep target must be unique per build.
- **Commits**: title + evidence body, **no `Co-Authored-By`**. The Write tool is broken on this box
  (emits NULs) → always commit with a heredoc:
  `git commit -q -F - <<'EOF' … EOF`
- **CI**: push to `main` → release `v4.14.369-badazz-buildNNN`, ~13–17 min. Poll with
  `gh run view <id> --repo vandalsquad187/BadazzKernel --json status,conclusion` in `sleep 55`
  loops, **timeout ≥ 1700000 ms**. Releases: `gh release list --repo vandalsquad187/BadazzKernel --limit 3`.
- **Syntax-check before push** (see *Local Build Notes*), always pristine-compared.
- User-facing reports are written in **German**.

## Common Issues
1. **Fresh clone build fails**: need `CONFIG_KSU_SUSFS=y` + sub-options, submodule init
2. **SUSFS implicit declaration**: guard calls with `#ifdef CONFIG_KSU_SUSFS_*`
3. **Deadlock on cat status**: fixed in d4835b6 — `get_cd_max_freq` must not take mutex
4. **Hardcoded CPU6**: fixed — use `find_gold_cpu()` portable
5. **Boot hang at crDroid logo** (Build 267-278): see Boot-Hang Root Cause below
6. **Local Termux build fails** (`modpost` / `elf.h`, no `bison`/`perl`): use the syntax-check recipe in *Local Build Notes (Termux)*, let CI do real builds
7. **USB `ep cmd timeout` / device not enumerating**: see *USB Debugging (Build 300-333, open)*
8. **Whole SoC resets on USB-C plug, log empty**: that is Fault 4 — do not trust logcat or
   `/proc/last_kmsg`, capture with `~/tmp/cap332.sh` (see *On-device Capture*)
9. **A tool call hangs forever**: an orphaned worker is still holding the pipe — run
   `$SU -c "sh $HOME/tmp/cap_scan.sh kill"`

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
