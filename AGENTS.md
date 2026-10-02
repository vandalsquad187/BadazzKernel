# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/BadazzKernel` branch `main`
- **Kernel**: `4.14.369` — `K6A_GOV v1.3.1` built-in, `LOCALVERSION=-BadazzKernel-sweet-v1.3.2`
- **Local**: clean, `main` @ `708e94da9` (Build 336: Fault 4 fixed — `ffs_func_eps_enable` now
  assigns `ffs` before `ffs_log`)
- **GitHub**: CI builds on every push to `main` (release) and `miui/test` (artifacts only);
  latest release `v4.14.369-badazz-build338` (CI run `36971228512`, success). **337/338 are
  docs-only** — the kernel binary is identical to Build 336.
- **KSU-Next Submodule**: `b100bd28` (`v3.3.0-95-gb100bd28`, dev-4.14-prctl-fix, UAPIv4)
  - `KernelSU-Next/kernel/Makefile:10` has `-DKSU_VERSION=33300`, **but that is only the
    non-git fallback**. The real value comes from `kernel/Kbuild:112` →
    `KSU_VERSION = 35000 + KSU_GIT_VERSION`, so the **running kernel reports `38309`** and
    `uapi=4`. Do not "fix" this — see below.
  - Why base 35000: submodule commit `045e4fb6` changed 30000 → 35000 because the **spoofed
    Manager sets `MINIMAL_SUPPORTED_KERNEL = 34634`**. With the old base the kernel would report
    `30000 + 3309 = 33309`, which is below 34634 and makes the manager show "update required".
    Verified on device 2026-10-02: `KernelSU: ksu GET_INFO: version=38309 uapi=4 flags=0x0` —
    clears the floor, and `uapi=4` matches ksud's `uapi: 4`.
- **Installed KSU userspace (2026-10-02)** — **not** the pin older notes mention:
  - ksud `/data/adb/ksud` → `ksud 3.4.0-19-g2b31f718 (uapi: 4)`, from
    KernelSU-Next CI run [`36750739326`](https://github.com/KernelSU-Next/KernelSU-Next/actions/runs/36750739326)
    (head `2b31f718`, *manager: Check secure screen lock when enabling App lock*, success).
  - Manager: `versionName=v3.4.0-19-g2b31f718-spoofed`, installed 2026-10-02 08:11.
    **The spoofed build has a randomised applicationId — currently `gojcms.hgelex.jabkht`.**
    `pm list packages | grep ksu` therefore finds **nothing**; look it up by `versionName` via
    `dumpsys package <pkg> | grep versionName`, or hunt the newest entry in `ls -1td /data/app/*/*`.
  - `kernel/Makefile:10`'s `33300` and any doc quoting it as "the version" are stale.
- **Open blockers**: none. Fault 4 (whole-SoC reset on USB-C attach) **closed in Build 336 and
  verified on device** — `USB_STATE=CONFIGURED`, no oops (see *USB Debugging*). Fault 2 (OTG host)
  **no longer reproduces on Build 336** (stick enumerates, `usb-storage` + vold mount work). Only
  Fault 3 (charger `APSD=OCP` rerun loop) is still open, and it is cosmetic.
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

## USB Debugging (Build 300–336)

Status: **All four faults are now explained.** Fault 4 — the whole-SoC reset on USB-C attach —
was a one-line uninitialized local and is **fixed in Build 336, verified on device**. Fault 1 was
closed by Builds 320/331; **Fault 2 no longer reproduces on Build 336** (OTG stick enumerates and
mounts). Fault 3 (charger OCP rerun) is the only one still open, and it is cosmetic.

Key files: `drivers/usb/gadget/function/f_fs.c`, `drivers/usb/dwc3/gadget.c`,
`drivers/usb/dwc3/dwc3-msm.c`, `drivers/usb/dwc3/core.c`,
`drivers/power/supply/qcom/smb5-lib.c`, `drivers/power/supply/qcom/qpnp-smb5.c`,
`arch/arm64/boot/dts/qcom/sdmmagpie-*.dts(i)`.
Known-good reference commits: `096e0a0c9`, `d8cf6ae19` (high-speed DTS), `79b3b4147` (bus_aggr).

### Fault 4 — instant reset on USB-C attach (FIXED in Build 336)

**Symptom**: *S21 an sweet stecken → 5 s Vibration + Reboot*, i.e. a `panic=5` window followed by
a PS_HOLD cold power-cycle. `panic_on_oops=1` meant the oops that killed the gadget never made it
to a durable log.

**Root cause** (`drivers/usb/gadget/function/f_fs.c`, `ffs_func_eps_enable()`):

```c
static int ffs_func_eps_enable(struct ffs_function *func)
{
	struct ffs_data *ffs;              /* uninitialised local */
	...
	int ret = 0;

	ffs_log("enter: state %d ...", ...);   /* <- reads ffs->ipc_log */
	...
	ffs = func->ffs;                       /* <- assigned only afterwards */
```

`ffs_log` (f_fs.c:47) expands to
`ipc_log_string(ffs->ipc_log, "%s: " fmt, __func__, …)`, so it dereferences an
uninitialised `ffs`. Everything else on that path was healthy — this ran on **every**
SET_CONFIGURATION, so the fault was host-independent and the S21/PC A/B was a red herring.

**Evidence** (Build 335, with `panic_on_oops=0` so the system survived to be read):

```
Build335: set_alt enter intf=0 alt=0 func=…44200 ffs=…e981800 gadget=…ab15348
Build335: set_alt ffs:   func=(null) epfiles=…c705f00 state=2 setup=0 eps=2 ifaces=1
Build335: set_alt revmap intf=0
Build335: set_alt state=2 alt=0 gadget=…
Unable to handle kernel NULL pointer dereference at virtual address 000001a8
Internal error: Oops: 96000006 [#1] PREEMPT SMP
Process kworker/u16:1 (pid: 68)   Workqueue: dwc_wq dwc3_bh_work
pc : ffs_func_set_alt+0x298/0x59c   x19 = ffs   x27 = func   x0 = 0
Code: … 910702fa 900033a1 f9400b65 aa1a03e2 (f940d400)
```

- `Build335: eps_enable enter` **never printed** → the fault sits between the last breadcrumb and
  it, i.e. inside the `ffs_log`.
- `(f940d400)` = `ldr x0,[x0,#0x1a8]` with `x0 = 0`; `ipc_log` is at `+0x1a8` because LOCKDEP,
  DEBUG_LOCK_ALLOC and INIT_STACK_ALL are all off (`work_struct` = 32 B) and the oops pins
  `func` at `ffs+0xb0` via `str x27,[x19,#0xb0]`.
- `ipc_log_string` itself is NULL-safe (`kernel/trace/ipc_logging.c:512 if (!ilctxt) return
  -EINVAL;`) — only the uninitialised local faults.
- A scan of every `ffs_log(` call site found **exactly one** other place where a local
  `struct ffs_data *ffs` is read before assignment, and it was this one.

**Fix (Build 336, `708e94da9`)**: `ffs = func->ffs;` moved above the `ffs_log`, plus a
`Build336: eps_enable ffs=%px ipc_log=%px …` marker. Verified on device:

```
Build336: eps_enable ffs=ffffffde9e981800 ipc_log=ffffffdea3516200 func->ffs=… eps=… count=2
Build335: set_alt eps_enable ret=0
Build335: set_alt done ok
android_work: sent uevent USB_STATE=CONFIGURED
```

Zero `Unable to handle` / `Oops` / `Kernel panic` lines; `/sys/class/udc/a600000.dwc3/state`
= `configured`; `usb/online=1`, `real_type=USB_PD`.

**Why the log always looked empty** — three compounding causes, all now understood:

1. `panic_on_oops=1` + `panic=5` → oops → 5 s (the "vibration") → PS_HOLD reboot. Drop to
   `echo 0 > /proc/sys/kernel/panic_on_oops` while diagnosing, restore to `1` afterwards.
2. **pstore/ramoops is a dead end here**: `CONFIG_PSTORE=y`, `console [pstore-1] enabled`,
   `ramoops attached 0x400000@0xb0000000`, yet after every crash `/sys/fs/pstore` is empty and
   `/proc/last_kmsg` is 0 bytes. The PMIC logs `Power-off: PS_HOLD (MSM Controlled Shutdown)` +
   `Power-on: Hard Reset and 'cold' boot` → DRAM lost. Real-time capture is the only channel.
3. The capture wrote with too slow a flush — see *On-device Capture*.

**What the early evidence wrongly suggested**: Build 332 saw *no* Type-C IRQ, no `usb/online`
flip, no vibrator call before the reset, which pointed at PMIC/hardware. That was a symptom of
#1/#3 (the oops happened before the charger path could be observed), not a hardware fault.

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

### Fault 2 — host mode (NOT reproducible on Build 336)

Original evidence (`dm313otg.txt`): `usb1-port1: Cannot enable. Maybe the USB cable is bad?` ×4 +
`attempt power cycle`, then `unable to enumerate`. Not touched since Build 320.

**Retested 2026-10-02 on Build 336 with a SanDisk 3.2Gen1 stick on a USB-C OTG adapter — the
fault does not occur.** Everything works end to end:

```
extcon4: USB-HOST=1
xhci-hcd xhci-hcd.0.auto: new USB bus registered, assigned bus number 1   (and 2 for USB3)
hub 1-0:1.0: 1 port detected
usb 1-1: new high-speed USB device number 2 using xhci-hcd
usb 1-1: New USB device found, idVendor=0781, idProduct=5581
usb-storage 1-1:1.0: USB Mass Storage device detected
scsi host1: usb-storage 1-1:1.0
scsi 1:0:0:0: Direct-Access      USB      SanDisk 3.2Gen1 1.00 PQ: 0 ANSI: 6
sd 1:0:0:0: [sdg] Write Protect is off
sdg: sdg1
FAT-fs (sdg1): Volume was not properly unmounted. Some data may be corrupt. Please run fsck.
```

`blkid /dev/block/sdg1` → `LABEL="ALG_XFCE_20" UUID="7C3A-3FFD" TYPE="vfat"`; vold mounts it on
`/mnt/media_rw/7C3A-3FFD` and `dumpsys usb` reports the volume `state mounted`. Zero
`Cannot enable`, zero `power cycle`, zero `unable to enumerate`, zero oopses. Whether Builds
320/331/336 fixed it incidentally cannot be attributed retroactively — only that it no longer
happens.

**Gotcha found while testing — the stick can vanish with no kernel log at all.** Termux ships
`com.github.mjdev.libaums.storageprovider.UsbDocumentProvider` (libaums, a *userspace* USB mass
storage driver). When the SAF DocumentsProvider probes the stick it calls
`UsbManager.openDevice()`, which issues `USBDEVFS_DISCONNECT`, so `usb-storage` unbinds,
`scsi host1` and `sdg` disappear, vold ejects the volume — and **`dmesg` prints nothing**, because
a userspace driver claim is silent. Symptoms and how to tell them apart:

| Observation | Meaning |
|---|---|
| `1-1` present, `1-1:1.0/driver -> …/usbfs`, `sdg` gone, no kernel line | libaums took the device. Not a kernel fault |
| `1-1` absent, `usb 1-1: USB disconnect` | real unplug |
| `usb1-port1: Cannot enable` / `power cycle` | the actual Fault 2 |

Fix from the outside: `echo 1-1:1.0 > /sys/bus/usb/drivers/usbfs/unbind` then
`echo 1-1:1.0 > /sys/bus/usb/drivers/usb-storage/bind` — `sdg` returns in ~1 s and vold
remounts. (It succeeded without `EBUSY` even while libaums looked like the holder, because
libaums also leaks: `A resource failed to call UsbDeviceConnection.close`.)

Root-cause search for that claim failed the obvious way: scanning `/proc/<pid>/fd` for
`/dev/bus/usb/*` finds nothing, because root here has **`CapBnd=0`** and therefore no
`CAP_SYS_PTRACE`, so `readlink` on another UID's fds (uid 10339) fails silently. Identity came
from `dumpsys package` instead — grep for the provider class name, then `cmd package path`.

Secondary observation, also userspace: on one occasion `fsck_msdos -p -f -y` exited **8** and
vold reported `public:8,97 failed filesystem check` / `state unmountable`, while a manual
inspection via `blkid` was fine. A later plug passed. Cannot be mounted manually for testing —
root has no `CAP_SYS_ADMIN`, so `mount` returns `EPERM`.

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
| `~/tmp/cap334.sh` | `start\|stop\|pstore\|poll\|scan` — the working driver (source lives in `~/tmp`, pushed to `/data/local/tmp/`) |
| `~/tmp/cap_scan.sh` | `kill\|count` — matches `readlink /proc/<pid>/fd/1` against `*/data/local/tmp/live/*` |
| `~/tmp/pm_poll.sh` | root poller ~14 Hz: `runtime_status\|online\|real_type\|type\|status\|capacity`, prints `CHANGE` only on a real state change |

`cap334.sh start` spawns under `setsid` + root:

1. **`cat /dev/kmsg`** → `/data/local/tmp/live/kmsg334.log`. This is the proven loss-free channel
   (gaps = 0 across 44 MB / 1.09 M lines). **`dd if=/dev/kmsg` does NOT work** — toybox `dd` dies
   with `read error: Invalid argument`, and toybox `dd` has no `conv=fsync` anyway
   (`oflag` only supports `append|direct|seek_bytes`).
2. logcat `main`/`system`/`events`/`crash` with `stdbuf -oL`.
3. `pm_poll.sh` (a `poll()` loop writing `CHANGE` lines), then a `BuildNNN: capture_start` banner
   marker (printk, `T0`, banner, heartbeat, control/runtime state, battery, `usb/online` +
   `real_type`).
4. The poll loop calls **`sync` on every iteration (every ~70 ms)**, not every Nth — a run that
   synced only every ~1 s lost the whole tail of the crash, because the oops/panic lands in the
   unflushed window. `sync` costs ~12 ms (global) / ~13 ms (`sync -f FILE`, GNU coreutils; the
   toybox `sync` accepts **no** file argument), i.e. ~17 % of a 70 ms cycle.

`/dev/kmsg` record format is `pri,seq,ts_us_since_boot,-;msg` — **field 1 is the syslog priority,
field 2 is the sequence number**, field 3 the timestamp. Seq deltas give the gap check;
`ts + T0epoch` from the banner converts to wall clock (`cap334.sh start` prints `T0=`, `T0epoch=`).

Gotchas learned the hard way:
- Capture files end **NUL-padded** → `tr -d '\0'` before grepping (NULs also break `grep -c` on
  some toybox builds).
- **Orphan hazard**: any worker whose stdout is still the tool's pipe hangs the tool forever.
  Always redirect worker stdout to a file. `cat /dev/kmsg` runs forever — that is expected, not a
  hang; `cap334.sh stop` kills it by `readlink /proc/<pid>/fd/1` target, never by pattern.
- Kill via `cap_scan.sh kill` (fd-target matching). **Never** `pkill -f` — the pattern matches the
  scanning shell's own cmdline. **Never** `ps | grep` — `hidepid` hides root procs from Termux.
- Full root path is **`/system/bin/su`** (KSU `sucompat.c` intercepts `execve` on `su`), and use
  full paths **inside** `su -c` (`/system/bin/dmesg`, `/system/bin/cat`, …) — the root PATH is
  broken there. Note `su` only exists in Termux's mount namespace (KSU magic-mounts it for granted
  apps): `adb shell '/system/bin/su …'` fails with `inaccessible or not found`. Termux cannot write
  `/data/local/tmp` (owner root) — use `adb push` to `/sdcard/Download/`, which Termux *can*
  overwrite.
- Root has **no capabilities** (`CapEff=0`, `CapBnd=0`), but the standard sysctls still write.
- Re-apply after **every reboot**: `echo 8 4 1 7 > /proc/sys/kernel/printk` (otherwise `4 6 1 7`),
  `echo 144 > /proc/sys/kernel/sysrq` (resets to 0), and — only while diagnosing —
  `echo 0 > /proc/sys/kernel/panic_on_oops`. Restore `panic_on_oops=1` when done.
- SUSFS spoofs `uname` → `6.12.0-android16-6.12-perf-g1d01cd293`; the real banner is
  `Linux version 4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-buildNNN`. `BuildNNN: hb` lines
  are the reliable build proof.

```bash
bash ~/tmp/cap334.sh start      # prints T0/T0epoch + worker pids, then reproduce the fault
bash ~/tmp/cap334.sh pstore     # /sys/fs/pstore + PMIC/LMK/tombstone dump (expect 0 files)
bash ~/tmp/cap334.sh stop
adb pull /data/local/tmp/live/kmsg334.log ~/tmp/bNNN/
tr -d '\0' < ~/tmp/bNNN/kmsg334.log > ~/tmp/bNNN/clean.log
grep -E 'BuildNNN|set_alt|USB_STATE|Unable to handle|Oops|Kernel panic' ~/tmp/bNNN/clean.log
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
- `CONFIG_KSU=y` **runtime `38309`** (`35000 + git`, `Kbuild:112`) UAPIv4, `CONFIG_KSU_SUSFS=y` +
  all sub-options + `TAMPER_SYSCALL_TABLE`. `Makefile`'s `-DKSU_VERSION=33300` is the fallback
  for non-git builds and never wins when the submodule is a git checkout.
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
708e94da9 Build 336: initialise ffs before ffs_log in ffs_func_eps_enable
17465e65f Build 335: pin the NULL pointer behind ffs_func_set_alt on SET_CONFIGURATION
054b9fc2f docs: refresh README.md + AGENTS.md from Build 320 to Build 333
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
- Submodule `KernelSU-Next` @ `b100bd28` (`v3.3.0-95-gb100bd28`, runtime `KSU_VERSION=38309`, UAPIv4)
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
7. **USB `ep cmd timeout` / device not enumerating**: see *USB Debugging (Build 300–336)*
8. **Whole SoC resets on USB-C plug, log empty**: that was Fault 4 — fixed in Build 336. If it ever
   recurs, do not trust logcat or `/proc/last_kmsg`; capture with `~/tmp/cap334.sh` and set
   `panic_on_oops=0` first so the oops does not reboot the phone (see *On-device Capture*)
9. **A tool call hangs forever**: an orphaned worker is still holding the pipe — run
   `$SU -c "sh $HOME/tmp/cap_scan.sh kill"`
10. **OTG stick / `sdg` vanishes with no kernel log**: Termux's libaums
    (`com.github.mjdev.libaums.storageprovider.UsbDocumentProvider`) claims the interface via
    `UsbManager.openDevice()`, unbinding `usb-storage`. Restore with `usbfs/unbind` +
    `usb-storage/bind` — see *Fault 2* in *USB Debugging*. Do not chase it as a kernel bug; you
    will not find the holder in `/proc/*/fd` either, because root has `CapBnd=0` (no ptrace).

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
