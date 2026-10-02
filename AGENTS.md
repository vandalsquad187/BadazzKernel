# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/BadazzKernel` branch `main`
- **Kernel**: `4.14.369` — `K6A_GOV v1.4.0` built-in, `LOCALVERSION=-BadazzKernel-sweet-v1.3.2`
- **Local**: `main` @ `e0e8d0d9f` — `341f6f5b0` (k6a_gov v1.4.0 / BW-floor release) + two docs
  commits (`27d7c7299` Fault 3 / k6a_gov docs, `e0e8d0d9f` k6a-ctl v1.2.0 record). Working tree clean.
- **GitHub**: CI builds on every push to `main` (release) and `miui/test` (artifacts only);
  latest release `v4.14.369-badazz-build340` (2026-10-02). **337/338/339 are docs-only** —
  their binary equals Build 336. The user is **running build340** (flashed 2026-10-02) together
  with **k6a-ctl v1.2.0** — see *Phase-4 acceptance*, passed.
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
- **Open blockers**: none. All four USB faults are closed. Fault 4 (whole-SoC reset on USB-C
  attach) **closed in Build 336 and verified on device** — `USB_STATE=CONFIGURED`, no oops.
  Fault 2 (OTG host) **no longer reproduces on Build 336** (stick enumerates, `usb-storage` +
  vold mount work). Fault 3 (charger `APSD=OCP` rerun loop) **closed 2026-10-02** by a 500 s
  capture on Build 338 — 0× `APSD=OCP` (see *USB Debugging*). Fault 1's storm stopped in
  Build 320/331. **Build 340 governor fixes landed, flashed and accepted 2026-10-02**;
  **k6a-ctl v1.2.0 flashed the same day**. Remaining: **Build 341** —
  `CONFIG_K6A_GOV=m` + the version lock, plus the two findings in *Build 341 scope*.
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

Status: **All four faults are closed.** Fault 4 — the whole-SoC reset on USB-C attach —
was a one-line uninitialized local and is **fixed in Build 336, verified on device**. Fault 1 was
closed by Builds 320/331; **Fault 2 no longer reproduces on Build 336** (OTG stick enumerates and
mounts); **Fault 3 does not reproduce any more either** — closed 2026-10-02 by a 500 s capture
(see below).

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

### Fault 3 — charger `APSD=OCP` (CLOSED 2026-10-02, does not reproduce)

**Symptom**: `APSD=OCP` logged repeatedly, with `smblib_rerun_apsd: re-running APSD` every ~5 s
while a charger was connected.

**Where the string comes from**: `APSD=` is not a `pr_info` — it is the debug print
`smb5-lib.c:1462` (`smblib_dbg(chg, PR_MISC, "APSD=%s PD=%d QC3P5=%d\n", …)`), so it only shows
with the debug mask on. The value is `apsd_result->name`, i.e. the fault is literally
`apsd_result->name == "OCP"`. The rerun itself is `smblib_rerun_apsd_if_required()` called from
the `usb_source_change` ISR (`smb5-lib.c:8072`), `typec`/`attach` IRQs (`:9345`, `:9468`) and
init (`:10297`).

**The dwc3 side is deliberate, not a leak** (`smb5-lib.c:357-360`, comment added in Build 320):

```c
/* smblib_handle_apsd_done() only calls it for SDP/CDP/FLOAT, so
 * DCP/OCP chargers never reach the dwc3 core - seeing no
 * vbus_notifier line next to an APSD line is expected, not a bug. */
```

`smblib_handle_apsd_done()` is at `smb5-lib.c:8003`; the SDP/CDP/FLOAT branch (marker
`Build320: apsd=%s -> will notify dwc3`, `:8018`) is `:8014-8027`, `OCP_CHARGER_BIT` +
`DCP_CHARGER_BIT` fall into `:8029-8033` with the marker `Build320: apsd=%s -> no dwc3
notification (expected)`.

**Evidence — 500 s capture on Build 338, 2026-10-02, `~/tmp/f3/clean.log`
(40 491 lines / 3 208 295 B, `printk 8 4 1 7`, `panic_on_oops=0`, `cap334.sh start` → `stop`):**

| Grep | Hits | Meaning |
|---|---|---|
| `APSD=OCP` | **0** | the fault itself is absent |
| `APSD=` (all values) | 49 | `HVDCP2` 32, `UNKNOWN` 11, `DCP` 4, `FLOAT` 2 |
| `smblib_rerun_apsd` | **5** | one per plug event at 12064.77 / 12289.35 / 12339.31 / 12409.06 / 12564.48 s — **no 5 s loop** |
| `Build320: apsd=` | 6 | 4× `DCP -> no dwc3 notification (expected)`, 2× `FLOAT -> will notify dwc3` |
| `vbus_notifier` | 9 | 3 bursts of 3 lines, `msm-dwc3 a600000.ssusb`, only after `APSD=FLOAT` connect (12291.3, 12566.5) and its matching detach (`event=0`, 12291.7) — never for DCP/HVDCP2 |
| `Oops` / `Unable to handle` / `Kernel panic` / `power cycle` / `Cannot enable` | 0 | no crash, no storm |

Side observation from the same capture, **userspace, not a kernel bug**: after the 4th transient
event (20:32:26) the charger stayed on `real_type=USB_FLOAT`, `current_max=1000000`,
`voltage_max=5000000` (5 V/1 A, QC lost), battery 88 % charging ~203 mA. One clean replug
restores `HVDCP2`. Also `Build320: notify_device_mode extcon_usb=` printed **0** times while
`vbus_notifier` fired — the extcon state change comes in over the PD/typec path
(`usbpd usbpd0: typec mode:6` sits right in front of it), not through the charger's
`smblib_notify_device_mode()`.

**Re-test** (charger connected, adb over TCP 5555 so a replug does not drop the session):

```bash
echo 8 4 1 7 > /proc/sys/kernel/printk        # root; restore 4 6 1 7 afterwards
bash ~/tmp/cap334.sh start                     # prints T0/T0epoch + worker pids
# now plug/unplug the charger 4-5 times over ~8 min
bash ~/tmp/cap334.sh stop
adb pull /data/local/tmp/live/kmsg334.log ~/tmp/f3/
tr -d '\0' < ~/tmp/f3/kmsg334.log > ~/tmp/f3/clean.log
grep -c 'APSD=OCP'  ~/tmp/f3/clean.log         # expected 0
grep -c 'smblib_rerun_apsd' ~/tmp/f3/clean.log # expected = number of plug events
```

Capture evidence lives in `~/tmp/f3/` (`clean.log`, `kmsg334.log`, `pm334.log`,
`banner334.log`) and on the device under `/data/local/tmp/live/`.

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

For the governor replace the include set with `-Idrivers/thermal/k6a_gov` and the file argument
with `drivers/thermal/k6a_gov/k6a_gov.c`; for `devfreq.c` there is no extra include path. **Both
currently pass with 0 errors** (as does pristine `k6a_gov.c`), and `devfreq.c` reports exactly
one pre-existing clang-only error in both versions (`a parameter list without types`, line 1389
pristine / 1388 after the Build 340 edit) — CI uses GCC, so it never sees it. Compare
`drivers/devfreq/devfreq_pristine.c` **inside the tree** (write it to `drivers/devfreq/`), not to
`~/tmp/`: a `#include "governor.h"` resolves relative to the file, so a copy outside the tree
fails with a bogus "file not found".

## k6a_gov v1.4.0

### Location
- `drivers/thermal/k6a_gov/k6a_gov.c` (1143 lines, `CONFIG_K6A_GOV=y`) — commit `341f6f5b0`
- `drivers/thermal/k6a_gov/Kconfig` / `Makefile`
- `drivers/gpu/msm/kgsl_pwrctrl.c` — `kgsl_k6a_get_levels()` + `kgsl_k6a_set_max_level_idx()`
- `drivers/devfreq/devfreq.c` — `k6a_devfreq_get_bw()` + `k6a_devfreq_set_bw()` (**the `min` half
  of `set_bw` used to be `if (min) …`, which silently ignored a release — it is unconditional now**)

### Features
- State Machine: OFF→GAMING→CD_L2/L3/L4, `dwell_in`/`dwell_out` (`fast` for a ≥5 °C rise, `fast`
  for a ≤−5 °C fall, `normal` otherwise), an **entry dwell** before GAMING gives way to CD_L2/L3
  (fast applies to the entry), and **immediate escalation** L2→L3, L2→L4, L3→L4
  (`Build340: escalate …`, counted in `throttle_events`); only recovery is dwell-gated. Three zero
  thresholds (reachable from sysfs) short-circuit the whole switch instead of pinning `CD_L4`
  at `t >= 0`.
- Temp: max over 4 Gold zones `cpu-1-0..3-usr`, then `cpu-0-0-usr` → `xo-therm` → `soc-therm` →
  `thermal_zone0`. Failure returns 0 and clears `temp_valid` → `state_machine()` and the battery
  guard are skipped and the state is held (`Build340: no temperature source, holding state %u`).
  v1.3.1 returned a hard-coded `40` and throttled on fake data. `temp_src` says which zone answered.
- CPU: `find_gold_cpu()` portable, `clamp_freq` order-independent, `enforce_max_freq()` tracks
  `gov->enforced_max` and releases through `cpufreq_update_policy()`, which resets min/max from
  `policy->user_policy` (never touched by us) and re-runs `CPUFREQ_ADJUST` — `cpufreq_notify()`
  re-clamps only while the state is ≥ CD_L2. Released on GAMING recovery, `enable=0`, `legacy=0`.
  `cpufreq_notify()` does not take `gov->lock`, so there is no recursion and no lock inversion.
- GPU: native enforcement via KGSL pwrlevels, caps per CD state
- BW: floors `gpubw` + `cpu-llcc-ddr-bw` per profile/CD state, `bw_floors` sysfs, written **every
  tick including 0** so a floor really drops again. Before this the `if (gpubw)` guard plus
  `k6a_devfreq_set_bw()`'s `if (min)` made every release path a no-op — live proof on Build 338:
  `state=gaming` while `bw_gpubw min=4000`.
- Battery: `battery_guard` @`battery_guard_temp` (35..60, default 45) → CD_L2 via `power_supply`,
  now counted as a throttle event
- Poll: `poll_ms` 100..5000 (default 250)
- Profiles: 0 off, 1 gaming, 2 battery, 3 badazz, 4 custom (**keeps** the current thresholds),
  5 badazz_safe. Profile is validated at init — an out-of-range module parameter used to index
  past `profiles[]`; an unconfigured profile falls back to the gaming thresholds
- Safety: `verify_build_hash` vs `k6a_features/git_hash`, `hash_verified` in status
- History: `K6A_HIST_N=16` ringbuffer, `hist=` in status; `enable=0` now goes through
  `set_state_locked()` so the OFF transition is recorded too
- Sysfs validation: `cd_thresholds` gold caps all-or-none zero + non-increasing (`clamp_freq()`
  maps a requested 0 to the **lowest** available frequency, i.e. the opposite of "no cap"),
  `gpu_caps` all-or-none zero + non-increasing + ≤2 000 000 000, `bw_floors` each ≤30000,
  `hysteresis` 1..1000 / 1..5000, `poll_ms` 100..5000, `battery_guard_temp` 35..60
- Sysfs: `enable/profile/status/hysteresis/cd_thresholds/gpu_caps/bw_floors/battery_guard/
  battery_guard_temp/poll_ms/legacy/game_pid`
- Status keys added in v1.4.0: `policy_max` (live `policy->max`, proves the cap is applied),
  `state_age_ms`, `temp_src`, `temp_valid`
- `game_pid` is stored and echoed in `status` only — the kernel never acts on it
- Hardening: `get_cd_max_freq` lock-free (caller holds lock), `cool_cur`/`status_show` locked,
  `freq_init_worker` mutex, ticks fix

### Pre-fix evidence on Build 338 (v1.3.1, read 2026-10-02)

Both leaks are measurable on the device *right now*, with the governor reporting
`state=gaming`, `legacy=1 enabled=1 profile=1(gaming)`, `gold_max=0`, `throttle_events=184`:

```
cpu6 scaling_max=1708800  cpuinfo_max=2304000   # K6A_PROFILE_GAMING cd_l2_gold_max = 1708800
cpu7 scaling_max=1708800  cpuinfo_max=2304000
soc:qcom,gpubw            min=4000 max=6881 cur=5161    # gaming cd_l2_bw_gpubw = 4000
soc:qcom,cpu-llcc-ddr-bw  min=762  max=6881 cur=6881    # gaming cd_l2_bw_llcc = 0 -> `if (llcc)`
                                                         # skipped the write, 762 = table min
```

`gold_max=0` (i.e. `get_cd_max_freq()` says "no cap in GAMING") while `scaling_max_freq` is
still `1708800` is exactly the raw `policy->max` poke that `cpufreq_update_policy()` now undoes.
Devfreq nodes live under `/sys/class/devfreq/soc:qcom,gpubw` (colon in the name, so always
`/system/bin/cat` with an absolute path — the root PATH is broken inside `su -c`).

**Phase-4 acceptance for Build 340 — PASSED on device 2026-10-02** (all seven while `state=gaming`):

| Check | Expected | Read |
|---|---|---|
| `cat …/cpu6/cpufreq/scaling_max_freq` | `2304000` (= `cpuinfo_max_freq`) | **2304000** (cpu7 too) ✅ |
| `cat /sys/class/devfreq/soc:qcom,gpubw/min_freq` | `0` | **0** ✅ |
| `cat /sys/class/devfreq/soc:qcom,cpu-llcc-ddr-bw/min_freq` | `0` | **0** ✅ |
| `grep version /sys/kernel/k6a_gov/status` | `version=1.4.0` | **1.4.0** ✅ |
| `grep -E 'policy_max\|temp_src\|temp_valid\|state_age_ms' …/status` | all four present, `temp_valid=1`, `policy_max` tracking `scaling_max_freq` | `policy_max=2304000`, `state_age_ms`, `temp_src=1`, `temp_valid=1` ✅ |
| `dmesg \| grep Build340` | `k6a_gov v1.4.0 loaded`, `gold cap … Hz state=…` on entry and `0 Hz` on recovery | `loaded` + `1555200/1708800/1209600/1094400 Hz` + **`gold cap 0 Hz state=1`** ✅ |
| heat / `hist=` | `gaming>cd_l3` reachable without a `gaming>cd_l2` step, `throttle_events` increments | 4× direct `gaming>cd_l3`, plus `cd_l3>cd_l2` and `cd_l2>gaming` ✅ |

**Release proof over one heat cycle** (18 samples / 3 s — this is the bug ① + ⑨ evidence, not a
single snapshot): at `cd_l2` → `cpu6_max=1708800`, `gpubw_min=4000`; back to `gaming` →
**2304000 / 0 / 0**. At `cd_l3` → `1094400 / 1500 / 4000`; back to `gaming` → **2304000 / 0 / 0**.
`throttle_events` 6 → 13 across the run, 0× `Unable to handle`/`Oops`/`Kernel panic`/`APSD=OCP`.

Note: `uname`/`/proc/version` report `6.12.0-android16-6.12-perf-g1d01cd293` — that is the
SUSFS spoof (it patches `init_uts_ns`, which `/proc/version` also reads). The real banner proof
is the `Build340:` markers, and `k6a_features: … version=265983` shows the genuine
`LINUX_VERSION_CODE` (see *Build 341 scope*).

`Build340: escalate …` correctly printed **zero** times: every observed transition was either
direct `gaming>cd_l3` (the GAMING branch has no marker by design) or a de-escalation
(`cd_l3>cd_l2`, `cd_l2>gaming`), and neither escalates. The markers fire only on L2→L3,
L2→L4, L3→L4.

Reproduce a CD transition with a CPU load (e.g. `sha256sum` on 8 cores) and watch `state` and
`policy_max` move together; after the load stops, `scaling_max_freq` must return to `2304000`.

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

### Build 341 (planned): `CONFIG_K6A_GOV=m`

The governor was *written* as a module — `module_init(k6a_gov_init)` / `module_exit(k6a_gov_exit)`
and `MODULE_LICENSE/AUTHOR/DESCRIPTION/VERSION` are the last five lines of the file, and
`Kconfig` is `tristate` with `default m` and the help line *"Build as module for version-lock
with kernel."* Only `arch/arm64/configs/sweet_defconfig:208` forces `CONFIG_K6A_GOV=y`.

Every symbol it needs outside its own file is already exported — this was checked symbol by
symbol (only `filp_open`/`kernel_read`/`filp_close`/`msleep` are non-obvious, all
`EXPORT_SYMBOL`; `pr_*`, `IS_ERR`, `kthread_run` are macros):

| Symbol | Export |
|---|---|
| `kgsl_k6a_get_levels` / `kgsl_k6a_set_max_level_idx` | `kgsl_pwrctrl.c:3462` / `:3488` `EXPORT_SYMBOL` |
| `k6a_devfreq_get_bw` / `k6a_devfreq_set_bw` | `devfreq.c:838` / `:802` `EXPORT_SYMBOL_GPL` |
| `cpufreq_update_policy` | `cpufreq.c:2474` `EXPORT_SYMBOL` |
| `cpufreq_cpu_get` / `cpufreq_cpu_put` | `cpufreq.c:258` / `:272` `EXPORT_SYMBOL_GPL` |
| `cpufreq_register_notifier` / `cpufreq_unregister_notifier` | `cpufreq.c:1910` / `:1950` `EXPORT_SYMBOL` |
| `thermal_cooling_device_register` / `_unregister` | `drivers/thermal/core.c:1144` / `:1232` `EXPORT_SYMBOL_GPL` |
| `thermal_zone_get_zone_by_name` / `thermal_zone_get_temp` | `core.c:1519` / `:117` `EXPORT_SYMBOL_GPL` |
| `power_supply_get_by_name` / `power_supply_put` | `power_supply_core.c:474` / `:490` `EXPORT_SYMBOL_GPL` |
| `filp_open` / `kernel_read` / `filp_close` / `msleep` / `kthread_stop` | `EXPORT_SYMBOL` |

`CONFIG_MODULES=y`, `CONFIG_MODULE_UNLOAD=y`, `# CONFIG_MODULE_SIG is not set` are already in
`sweet_defconfig:336/338/342`.

Steps:
1. `arch/arm64/configs/sweet_defconfig:208` `CONFIG_K6A_GOV=y` → `CONFIG_K6A_GOV=m`.
2. `.github/workflows/build-kernel.yml` — the `REQUIRED_CONFIGS` array contains
   `"CONFIG_K6A_GOV=y"` (change to `=m`) and the build step is
   `make -j$JOBS Image.gz dtb.img dtbo.img`, which compiles `obj-m` but does **not** run modpost.
   Add `modules` to that target list, then copy
   `drivers/thermal/k6a_gov/k6a_gov.ko` next to `Image.gz` in the *Build flashable ZIP* step
   (that step currently copies only `Image.gz`, `dtb.img`, `dtbo.img`).
3. `k6a-ctl`'s `service.sh` runs `insmod …/k6a_gov.ko` and falls back to the legacy userspace
   cooldown if it fails (the module already ships a full CD_L2/L3/L4 state machine, so this is
   a graceful degradation, not a brick).
4. Version lock (Kconfig help + `synergie2.txt` TEIL 3): refuse to load when the `.ko`
   `version=` differs from what k6a-ctl expects — that is the whole point of the module.

### Git History (main)
Full log: `git log --oneline -40`. Head is the k6a_gov fix, on top of the USB debug series:

```
341f6f5b0 Build 340: release the k6a_gov CPU cap and BW floors again, escalate, validate sysfs
75e8d0bee docs: the KSU version is 38309 at runtime, and userspace is now v3.4.0-19
2a32967d3 docs: Fault 2 does not reproduce on Build 336 + the libaums trap
df3908cb2 docs: Fault 4 root cause and corrected capture recipe (Build 336)
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

k6a_gov history: `341f6f5b0` v1.4.0 cap/BW release + escalation + validation, `81d69ae` v1.3.1 ticks
fix, `d4835b6` deadlock, `53bb809` v1.3.1 hardening, `967c134` v1.3.0 BW floors + profile 5,
`dfcb96b` v1.2.1, `602a281` `CONFIG_K6A_GOV=y`.

## k6a-ctl Companion
- **Repo**: `vandalsquad187/k6a-ctl` branch `main` @ **`426b516` (v1.2.0, versionCode 120)**,
  release `v1.2.0` → asset `k6a-ctl-v1.2.0.zip` (20 543 B), local clone `~/k6a-ctl`.
  Recent commits: `426b516` v1.2.0 (Phase 5), `a049fac` v1.1.6 version format,
  `a9170b6` v1.1.5 **removed the USB autosuspend workaround** (so userspace no longer masks a
  kernel bug), `de3c037` v1.1.4 dwc3 autosuspend off, `18cf392` v1.1.3 log rotation +
  `battery_guard_temp`, `a639e7c` v1.1.2 robustheit, `e43ccf6` v1.1.1 whitelisted handler.
- **Device is up to date and enabled** (read 2026-10-02): `module.prop` = **v1.2.0 /
  versionCode 120**, `/data/adb/modules/k6a-ctl/disable` **removed**, both daemons alive
  (pids in `run/*.pid`) and `webui-server.sh` listening on `127.0.0.1:8767`.
  `data.txt` carries the new keys: `gov_policy_max=2304000`, `gov_state_age`, `gov_temp_src=1`,
  `gov_temp_valid=1`, `gov_batt_guard=1`, `gov_game_pid=0`, `gov_version=1.4.0`.
  `gov_batt_guard=1` is the new sync working — the node read **0** the day before.
  Controller log shows the auto profile flip `gaming ↔ badazz_safe` at `auto_badazz_temp=85`.
- **`check_module.sh` is a build gate, not a shipped tool** — `build.sh:17` deliberately excludes
  it (with `build.sh` itself) from the ZIP. Earlier notes framed its absence on the device as a
  defect; that was wrong. Running it by hand is repo-side only:
  `sh <repo>/bin/check_module.sh <repo>`. Gate result 2026-10-02: **green, 0 warns**.
- **Phase 5 shipped (`426b516`, v1.2.0)**:
  - **BW-floor payload order bug found + fixed.** `applyBwFloors()` built the string interlaced
    `[gpubw_L2, llcc_L2, gpubw_L3, llcc_L3, gpubw_L4, llcc_L4]` while `bw_floors_store()` parses
    grouped `sscanf("%u %u %u %u %u %u", &gpubw_l2..llcc_l4)` → **4 of 6 values landed wrong**,
    silently (`bw_floors` has no monotonicity validation). Now `g.concat(l).join("_")`.
    `resetBwFloors()` defaults were the same interlaced nonsense → replaced by the gaming profile
    `dg=[4000,3000,1500] dl=[0,4000,3000]`, verified against the live node
    (`bw_floor_gpubw=4000 3000 1500`, `bw_floor_llcc=0 4000 3000`).
  - **Single authority**: k6a-ctl now writes **only** `legacy` (`delegated=1` → 1, else 0) and
    never races `scaling_max_freq`/`bw_floors` against the kernel. `_LEGACY_SYNCED` guards rewrites.
  - **`battery_guard` honoured**: config key `battery_guard=on|off` (default `on`) syncs the node;
    WebUI toggle via `/battguard?e=0|1` alongside the existing `?t=35..60`. Kernel default is
    **0** (`kzalloc`), so until now the threshold was stored but inert. Wire-up is
    `k6a_gov.c:576`, gated on `state == K6A_GAMING`.
  - **`game_pid`**: k6a-ctl writes `pidof $game_pkg` only on change (not per 1s tick) and `0` on
    leaving GAMING. Kernel side is store+echo only (`k6a_gov.c:808/:815`), never acted upon.
  - **Build-340 status surfaced**: `policy_max`, `state_age_ms`, `temp_src`, `temp_valid` parsed
    from `/status` into `data.txt` and the WebUI (Gold cap in MHz, State age in s, temp-source
    code table 0 none / 1 Gold / 2 Silver / 3 xo / 4 soc / 5 zone0, plus an explicit
    `temp_valid=0` → "keine Quelle, State gehalten" state). All four are empty on a pre-340
    kernel — extraction regexes checked against the live 1.3.1 `status`, no false matches.
  - `_startup` logs `k6a_gov <version> legacy=<n>` and warns unless the version matches `1.4.*`.
- **Mode**: `delegated=1` — Kernel handles thermal (CPU/GPU/BW), module handles game detection +
  sched tuning + auto `badazz_safe` @85°C
- **WebUI**: Gov-Status (+ policy_max/state_age/temp_src/temp_valid), BW-Floors, GPU-Caps,
  History timeline, Akku-Guard toggle
- **Do not re-enable before Build 340** *(historical — done)*: on v1.3.1 the Gold cap and the
  BW floors were never released (see *Pre-fix evidence*), so a userspace cooldown would have
  fought the kernel. The `disable` file was dropped after the Build 340 acceptance run.
- **Still open**: `synergie2.txt` TEIL 3 — governor version string burned into both sides with
  a mismatch refusing to load — which is Build 341's `k6a_gov.ko`.

## Build 341 scope

Two findings from the Build 340 acceptance run, both in the version-lock area, both **pre-existing**
(not introduced by Build 340) and both invisible until today:

### 1. `hash_verified=1` is reported even though the check never ran

`k6a_gov_init` starts `gov_thread`, whose first iteration calls `verify_build_hash()` and then
latches `hash_verified = true` **unconditionally** (`k6a_gov.c:565-568`):

```c
if (!hash_verified) {
    if (verify_build_hash() != 0)
        gov->legacy_mode = 0;
    hash_verified = true;          /* <- also set when the check was skipped */
}
```

On device the check **fails and is skipped**, at `[0.777182]`, i.e. 0.19 ms after
`k6a_features: initialized (… version=265983)` at `[0.776989]` had already created the node:

```
[0.776989] k6a_features: initialized (cpu_floor=1, gpu_floor=1, msm_perf=1, thermal_writable=1, susfs=1, version=265983)
[0.777182] k6a_gov: k6a_features/git_hash not found, skipping hash verify
```

The node exists (`/sys/kernel/k6a_features/git_hash` = `full-synergy` and is readable from
userspace) — so the failure is that **`filp_open("/sys/…")` runs from an initcall context, before
userspace `init` has mounted `/sys`**. kobject/kernfs creation does not need the mount, path
lookup does. Since it is attempted exactly once and then latched, the verify never happens.

Two consequences:
- `status` reports `hash_verified=1` = "checked", the WebUI renders **✓ verified** — a lie.
- `verify_build_hash()` distinguishes three outcomes but returns `0` for both *match* and
  *skipped*, so skip and match are indistinguishable, and a genuine mismatch would silently set
  `legacy_mode = 0` (hand control to userspace) while a skip leaves it at 1 (keep enforcing).
  Today the outcome happens to be correct, because `K6A_BUILD_HASH "full-synergy"` equals the
  runtime `git_hash` — but only by coincidence of the values, not by the check succeeding.

**Fix for Build 341**: do not latch on failure. Retry (bounded, e.g. every tick until it
succeeds or N attempts), and report a third state instead of a bool — `0 = mismatch/skipped,
1 = verified` must not be one bit. This is the same code path TEIL 3 wants for the version lock,
so fix it there rather than separately.

### 2. `LINUX_VERSION_CODE` is pinned to 4.14.255

`Makefile:1392-1396` does **not** use `$(SUBLEVEL)`:

```make
define filechk_version.h
	(echo \#define LINUX_VERSION_CODE $(shell                         \
	expr $(VERSION) \* 65536 + 0$(PATCHLEVEL) \* 256 + 255); \
	echo '#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))';)
endef
```

so `include/generated/uapi/linux/version.h` is always `265983` (4.14.**255**) while the top
Makefile says `SUBLEVEL = 369`. `k6a_gov.c:27` hardcodes
`K6A_GOV_KERNEL_VER KERNEL_VERSION(4,14,369)` = `266097`, hence on **every boot**:

```
[0.776990] k6a_gov: build/run version delta (266097 vs 265983) — continuing (built-in)
```

The `255` is deliberate, not an accident: `KERNEL_VERSION` stores `SUBLEVEL` in only 8 bits, so
369 would be emitted as `266097`, which decodes back as **4.15.113**. Capping at 255 keeps the
decoded version sane but makes every `#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,NNN)` with
`NNN > 255` evaluate false.

**Audit (2026-10-02)**: across the whole tree there is exactly **one** such gate —
`KERNEL_VERSION(4,14,369)` inside `k6a_gov.c` itself, i.e. the warning above. The only other
out-of-4.14 gate is `KERNEL_VERSION(5,0,0)`, which is false under either value. So option (a)
below costs nothing today, and option (b) would flip nothing but our own warning. The real risk
is forward-looking: any *future* `> 255` gate would silently stay false, and the version lock
compares `265983` against a `.ko` built with `266097`.

**Decision needed for Build 341** — either
(a) keep `+ 255` and make `K6A_GOV_KERNEL_VER` `KERNEL_VERSION(4,14,255)` so both sides agree,
    documenting that `LINUX_VERSION_CODE` is capped on this tree, or
(b) drop the cap to `0$(SUBLEVEL)` so `LINUX_VERSION_CODE = 266097` and make
    `K6A_GOV_KERNEL_VER` decode-safe.

The audit above says (b) flips only our own warning, so **(b) is the better option** — but it
changes a generated header for the whole tree, so it must ship with the audit evidence in the
commit body, not alone.

### Planned work (unchanged)

1. `arch/arm64/configs/sweet_defconfig:208` `CONFIG_K6A_GOV=y` → `=m`.
2. `.github/workflows/build-kernel.yml`: `REQUIRED_CONFIGS` `"CONFIG_K6A_GOV=y"` → `=m`; add
   `modules` to the `make -j$JOBS Image.gz dtb.img dtbo.img` target list (it compiles `obj-m`
   but does not run modpost); copy `drivers/thermal/k6a_gov/k6a_gov.ko` into `anykernel/`.
3. `k6a-ctl` `service.sh`: `insmod …/k6a_gov.ko`, falling back to the legacy userspace cooldown
   on failure; refuse to load when the `.ko` `version=` differs from what k6a-ctl expects (TEIL 3).
4. Fix finding 1 in the same commit — the lock depends on it.

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
