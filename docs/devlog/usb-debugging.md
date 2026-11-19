# usb debugging (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> USB-Faults 1-4, On-device-Capture-Rezepte (Build 300-336)

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

