# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/BadazzKernel` branch `main` @ **`f9b2e6aab`** (Build 388 scope:
  KernelSU-Next-Submodule `86773df0` — tote Monolith-TUs entfernt; Binary unverändert).
  CI run `39857009034` queued beim Verfassen. Davor `f9b2e6aab`→`046760b0e` (Submodule-Bump-
  Kette `9409a05f` su-shim-Cred-Fix → `ada691bf` Diag-Bits → `8082812b` Len-Fix →
  `34c3b8ea` pre-ksud-Escape → `786bcb00` TAMPER-Patch-Init, alles **geflasht & verifiziert**,
  Builds 383–387).
- **Kernel**: `4.14.369` — K6A-kompatibel, KSU-Next **built-in, 4.14 direct-patch mode**
  (14 Slots, `patch_ok=0x7f7f`, sys_call_table `0xffffff9a2d401000`). **k6a_gov v1.6.0**
  (Build 389-Scope): `gpu_floor`-Sysfs (Hz, 0=inaktiv) — GAMING-only KGSL-min_pwrlevel
  via neuem Export `kgsl_k6a_set_min_level_idx()` (+min_idx in get_levels). On-device-
  Proof build388-Kernel: `min_pwrlevel=2` pinnt `gpuclk=565 MHz`, restore 7 → 355.
  Release-Pfad erfasst den User-MIN einmal (`gpu_floor_saved_min`) und restauriert ihn;
  Status neu: `gpu_floor=`, `gpu_floor_last=`, `bt=` (Akkutemp, -1 unbekannt). GPU-
  Tabelle live: 800/650/565/430/355/267/180/140 (idx 0–7; k6a sieht 0–6). KGSL-Interplay:
  effektiver DCVS-Floor = `min_t(thermal_pwrlevel_floor=4, user_min)` → User-Min-Einträge
  älter idx 4 (tiefer als 355 MHz) werden vom thermal-floor-Vorgabewert zurückgeclampt,
  Einträge ≤ 4 greifen voll (live verifiziert). Gaming-Floors ≥ 355 (idx ≤ 4) sind der
  relevante Bereich; dmesg-Proofs: `Build389:`-Marker.
- **GitHub-Releases**: Releases entstehen automatisch pro Main-Bump (`v4.14.369-badazz-buildNNN`).
  **Name≠Scope-Regel bleibt**: nur `gh release list` + `build-info.txt (`BUILD_NUMBER`/
  `COMMIT_SHA`) zählen. Flashen ausdrücklich nur das **aosp-ZIP** (Gerät läuft crDroid).
- **Gerät (sweet, crDroid/AOSP, Android 16)**: Kernel **build387** (KSU ver `38523`, KSU
  `9409a05f`) — **Root läuft vollständig**: `su id` (Termux) → `uid=0(root) …
  context=u:r:ksu:s0`; Alternativen `grant_cmd`/`rs3` (Probes `~/tmp/ksu_probe/`). Diag:
  crown `0x780001bd`, open_errno 0. K6A/k6a-ctl-Bestand zuletzt verifiziert auf build352
  (Details unten, wichtig fürs r50–r59-Messarchiv) — seit dem crDroid-Umzug nicht neu
  kalibriert.
- **Sampler-Bestand auf Gerät**: `fps_samp2.sh` = **capped-Variante** (kein `echo 2304000 >
  policy6/scaling_max_freq` mehr — Original unter `fps_samp2_uncap.bak`, r55-Befund);
  `sf_probe.sh` v2 (pro Snapshot layer-fresh, schreibt eigenes `sf_probe.log`);
  `arm.sh` mit 1560-s-Fenstern (Backups `arm.sh.bak-10min`). **Nie `fps_samp2.sh stop`
  aufrufen** — „stop" ist `DUR`, laufende Instanz trunciert `fps.log` (r59a zerstört).
- **CODM-Messstand (r50–r59, alle build351/352, crDroid/A16 — alle Runs liefen crDroid,
  keine MIUI-Baseline; eine Runde pro Messung, gleiche Karte)**:
  Schwellen 96/99/102/93 sind der **+10…+13-med180-Hebel** (r52–r54 Ø 73,9 vs UC Ø 63,0);
  **Lüfter** hält die Spätrunde (r56 late-med 69,6 vs r57 45,6, bt 32 vs 41,7 °C —
  Kollaps = SoC-eigener CPU-Droop ab bt≈38 °C, k6a bleibt 100 % gaming); Trip95→98 °C
  verschlechtert (r58: med180 63,8, bt 45,2 °C, k6a 17 % cd_l2) — **Trips bleiben auf
  95** (Daemon-Pinning ist nicht sysfs-schraubbar); Gold-Cap-End-to-End-Test r59b:
  `p6=1708800 in 412/412 Samples` (capped-Sampler-Modus + ctl aktiv). Details im
  Abschnitt *Post-352-Messsituation*.
- **KSU-Next Submodule**: `72f8cc62` (kp_ksud.c-Removal, unpumped — kein eigener Build,
  Binary neutral), davor `86773df0`
  (Dead-Cleanup), `9409a05f`. **UAPI=6**, Runtime 35000+revcount (build387 = `38523`).
  - **su-Flow** (`feature/sucompat.c`, legacy <4.19-Adapter): exec `/system/bin/su` →
    sucompat-Redirect → `/data/adb/ksud` → preexec-Escape (`hook_aarch64_execve`,
    `ksu_preexec_ksud_escape` matcht **beide** Pfade: `/data/adb/ksud` +
    `/data/adb/ksu/bin/ksud`) → `escape_with_root_profile()`.     **Root-Cause** des Dagewesenen: `kern_path("/data/adb/ksud")` als untrusted cred →
    DAC-Durchlauf durch `/data/adb` (0700) verweigert → Lookup muss unter
    `override_creds(ksu_cred)` laufen (5.x-Pfad machte das schon).
  - **GRANT_ROOT**: `syscall(142 reboot-magic)` → ksu-fd → `ioctl(fd, _IO('K',1)
    /*KSU_IOCTL_GRANT_ROOT*/)` → uid 0 (Termux ist allowlisted). erlaubt über
    `allowed_for_su()` = manager-uid || allowlist-uid.
  - **Features** (build387, `ksud feature list`): su_compat(0)/kernel_umount(1)/sulog(2)/
    adb_root(3)/selinux_hide(4) **ENABLED**, shell_su_adb_only(6) DISABLED.
  - **ksud**: `/data/adb/ksud` md5 `f3eacfd0` (v3.4.2-50-gb427a3f9, identisch
    `libksud.so` App-Dir), Kontext `u:object_r:ksu_file:s0`; `libadbroot.so` (md5
    `92f8e84a`) liegt unter `/data/adb/ksu/lib/` (adb_root-Feature-Preload-Pfad).
    **ksud-Daemon läuft NICHT** (built-in-Modus, boot-behamster Spawn fehlt) — su/Root
    funktioniert trotzdem; `post-fs-data` NICHT manuell nachfahren (Modul-Scripts
    doppelt bearbeiten).
  - **Crown-Bits** (Kumulativ-Slot bis Reboot, `include/ksu.h` bits 26–30):
    `TAMPER_INIT(30)` `KSUD_EXEC(29)` `KSUD_ESCAPE(28)` `ESCAPE_RET0(27)`
    `ESCAPE_FAIL(26)`; dmesg `/sdcard/Download/dmesg_b385.txt` (build386/ver 38522).
- **On-device-Operativa (Wichtig für künftige Fix-Session)**:
  1. **`cp` unter escaped-root-cred schreibt NULLen**: jedes `cp <src> <dst>` wenn
     src in `/data/app/…/lib/arm64` bzw. bei großen Dateien — Inhalt Nullen bei
     korrekter Länge. Immer `cat src > dst` + `md5sum`-Verifizieren, dann `chmod 755`
     + `chcon u:object_r:ksu_file:s0 /data/adb/…`.
  2. **`ksud install` niemals aus `/data/adb/ksud` selbst** (killt DAEMON_PATH vor dem
     self-copy → ENOENT, Datei WEG). Die rust-Schritte bei Bedarf manuell replizieren.
- **USB-Faults**: alle vier geschlossen (Build 336/338/348-Kette).
- **User speaks German**; device reports go out in German; User flashen/rebootet selbst
  (aosp-ZIP), meldet Build-/CI-Status. Releases/Tags nur mit explizitem OK.


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

- Trigger: push to `main` (full dual-build + release) / push to `miui/**` (artifact only, no release) / `workflow_dispatch`.
  Erste Live-Erkenntnis (19.10.): **Pushes via GITHUB_TOKEN feuern kein CI** — deshalb
  gibt es vom Auto-Resync-Push des Runners nie eine Trigger-Kaskade (das `[skip ci]`
  im Merge-Message-Text ist nur eine zweite Versicherungsstufe).
- **Dual-Build (seit 2026-10-19): 2 Builds pro Push** — Matrix:
  - `aosp` → ref `main` — für Stock/HOS/AOSP-ROMs (custom-USB-Profil aktiv)
  - `miui` → ref `miui/test` — für MIUI/HyperOS (stock-USB-Profil via `sweet_miui.config`-Merge + Branch-Reverts)
- **miui auto-resync**: der miui-Job mergt `origin/main` mit `git merge -X ours --no-edit`
  in `miui/test` und pusht zurück. Besonderheit: der GITHUB_TOKEN darf **keine
  `.github/`-Diffs pushen** ("refusing to allow ... workflow ... without `workflows` security"),
  und ein `workflows: write`-Scope ist in der Workflow-Syntax nicht gültig — deshalb
  wird nach dem Merge `.github/` per `git checkout HEAD~1 -- .github/` auf den
  Branch-Stand zurückgesetzt und merget per amend; der Push-Diff enthält dann keine
  Workflow-Datei und geht durch. Konflikte in den stock-blockierten USB-Dateien
  folgen dem MIUI-Profil;unksollte main-Änderungen fließen normal ein. Harte
  Merge-Fails brechen den Job sichtbar (manueller merge erforderlich).
- ccache-Keys pro Variante (`-aosp` / `-miui`) — untereinander nicht verdrängend.
- Config-Pfad: `make sweet_defconfig` (aosp) / `merge_config.sh sweet_defconfig sweet_miui.config`
  (miui) → inject `CONFIG_LOCALVERSION=…-build${run_number}-${variant}` → `olddefconfig`
  → validate required configs (`KSU`, `KSU_SUSFS`, `KSU_TAMPER_SYSCALL_TABLE`, `K6A_GOV`,
  `NFC_NQ_PN80T`, `PHY_QCOM_QUSB2`) → `make Image.gz dtb.img dtbo.img`
  → verify `CONFIG_PHY_QCOM_QUSB2` survived + `phy-qcom-qusb2.o` was built
  → zips `Badazz-kernel-sweet-v${VERSION}-build${run_number}-aosp.zip` / `…-miui.zip`
  (inkl. `build-info.txt` mit `VARIANT=`) → beide als artifacts — ⚠️ Stolperstein:
  die *artifact-NAMES* müssen `${{ matrix.variant }}` enthalten, sonst haben beide
  Beine denselben Namen und `download-artifact@v4` lädt nur eins (Bug in build361,
  schief gefixt in build362).
- **Release** (separater Job `needs: [build]`, nur auf `main`): der `release`-Job
  lädt $# alle artifacts nach `dist/` (`merge-multiple: true`) und erzeugt via
  softprops **EIN Release** `v${VERSION}-badazz-build${run_number}` mit **beiden zips**
  (`files: dist/Badazz-kernel-sweet-*.zip`). Die Release-Body-Datei wird im Job
  selbst erzeugt (`body.md`, echter `git log`-Changelog) — nicht mehr als wörtliches
  `$(git log …)` im body-Template.
- Timings (warm): 15–20 min für beide Beine inkl. release; Build 353 (single) lag
  bei 18m9s. `gh run list` / `gh run watch <id>` to follow

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

## k6a_gov v1.5.0

### Build 347 scope — robust temperature signal, anti-thrash dwell, profile clobber fix — flashed as `build348` 2026-10-05, passed

The governor version stays **1.5.0**. Everything in this scope is verifiable **without a
gamepad**: `sha256sum` on 8 cores forces CD transitions, `status` readbacks prove the rest.
The fps rounds stay a separate, later acceptance step.

Measured on build346 **before** any code was written (2026-10-05):

| Evidence | Reading |
|---|---|
| idle Gold zones | 74.1 / 74.5 / **82.8** / 78.7 °C → `max()` spread **6.2 K**, median 76.6 |
| `cd_recover` | `83`, i.e. **above** the idle peak of 82.8 — CD_L2 could barely recover |
| CD time, identical thresholds | T5 **13.2 %** vs T7 **58.2 %** (only *which* core was hottest differs) |
| thrash | `hist` re-entered CD **764 ms** after the previous recovery; `throttle_events=2807` in 21.7 h |
| dwell arithmetic | every de-escalation lands at exactly +2048 ms = `hysteresis_normal` |
| sysfs bound | `hysteresis_store` rejected `fast > 1000`, so a sane entry dwell was **unexpressible** |
| F18 trigger is live | `bin/k6a-controller:145` → `gov_write "profile" "$idx"` on every mode switch and on auto_badazz |
| vendor engine | `thermal_message/sconfig` 0 → 13 mid-round while `state=gaming`; k6a could not see it |

- **Robust temperature.** `read_temp()` collects the four Gold zones, returns the **2nd-highest**
  for the state decision and keeps the **peak** in `temp_peak`. `state_machine()` now triggers the
  L4 emergency on `max(temp_peak, t)` only — one core at `cd_l4_temp` still caps immediately, so
  the strongest throttle keeps the old `max` semantics while L2/L3 decide on the cluster.
- **L3 never waits.** The GAMING branch enters `CD_L3` without a dwell as soon as
  `t >= cd_l3_temp`. Since `t` is the 2nd-highest, that means *two* Gold zones at 89 °C — a
  cluster condition, not a single-core spike. This is what makes the longer L2 dwell safe.
- **Entry dwell 500 → 3000 ms.** `hysteresis_store` lifts the `fast` bound 1000 → 5000 (both
  fields now 1..5000) and the init default becomes 3000. It kills the 4–6 s enter/leave cycles.
- **Profile no longer clobbers user thresholds.** A `cd_thresholds` write sets
  `thresholds_user`, after which `profile_store` only records the name and logs
  `Build347: profile=N keeps user thresholds …`. `-` releases them and restores the profile
  table (skipped for `custom`, whose table is all zeros and would otherwise disarm the machine).
  `thresholds_source=user|profile` in `status` makes the winner explicit.
- **Deadband warning.** `l2t - rec < 4` logs a warning — a 3 K band is narrower than the
  measured 6.2 K sensor spread.
- **Vendor thermal visible.** `read_one_node()` (`filp_open`/`kernel_read`, same pattern as
  `verify_build_hash`, polled every 64 ticks) exports `vendor_sconfig=` and `vendor_cpu_limits=`
  from `/sys/class/thermal/thermal_message/{sconfig,cpu_limits}`. No new SELinux grant is needed:
  k6a-ctl's `sepolicy.rule` already grants `kernel → sysfs {dir search read open}` /
  `{file read open getattr}` generically.

Marker: **`Build347:`** (the modified `Build345:` L4 line and `Build340:` custom-profile line
were re-marked so `dmesg | grep Build347` covers the whole scope).

### After the flash — build348 (passed 2026-10-05)

Release `Badazz-kernel v4.14.369-badazz-build348`, `build-info.txt` → `BUILD_NUMBER=348`,
`COMMIT_SHA=c0504a8fd943fdebe31afe0c1d02f40fcf71dac8`, vermagic
`4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-build348`, `k6a_gov.ko` **51 760 B**
(48 488 B on build346).

**Flash route — first time from Android instead of recovery.** The zip is stock AK3:
`META-INF/com/google/android/update-binary` is the real entry point. It unzips the release into
`$AKHOME`, runs `setup_bb` (installs `tools/busybox` into `$AKHOME/bin`), exports
`PATH="$AKHOME/tools:$AKHOME/bin:$PATH"` (that is where `magiskboot` comes from — invoking
`anykernel.sh` directly fails with `magiskboot: inaccessible or not found`), then runs
`ash anykernel.sh`. Two things matter:

- **`OUTFD` must be a regular file, never a pipe.** `ui_print` does
  `>> /proc/self/fd/$OUTFD`; on a pipe that returns `ENXIO` → `can't create /proc/self/fd/1`.
  So redirect stdout to a log file and pass `1` as the OUTFD argument.
- **Under Android `BOOTMODE=true`** (zygote running) `setup_env` returns immediately — no
  `mount_all`/`umount_all` juggling touches the live system.

```bash
# backups: 128 MB boot + 32 MB dtbo, md5-verified, also copied to /sdcard/Download/
dd if=/dev/block/bootdevice/by-name/boot of=/data/local/tmp/boot-build346.img bs=1048576
dd if=/dev/block/bootdevice/by-name/dtbo of=/data/local/tmp/dtbo-build346.img bs=1048576
adb push Badazz-kernel-sweet-v4.14.369-build348.zip /data/local/tmp/build348.zip
adb push ak3/ /data/local/tmp/ak3 && adb shell 'chmod -R 755 /data/local/tmp/ak3'
# the flash itself (AKHOME must be set: its default /tmp is not writable here)
adb shell 'export AKHOME=/data/local/tmp/ak3real; mkdir -p $AKHOME;
  sh /data/local/tmp/ak3/META-INF/com/google/android/update-binary 1 1 \
     /data/local/tmp/build348.zip > /data/local/tmp/flash348.log 2>&1'
```

**Prove it non-destructively first (dry run).** Copy the AK3 tree, patch only the two
`dd … of=$BLOCK` lines in `tools/ak3-core.sh` to point at a file, **and turn the
`dd if=/dev/zero of=$BLOCK` line into `:`** — it has no `count`, so against a file it would fill
the disk forever (against a block device it stops at ENOSPC, which is why the real run is fine).
Drop `dtbo.img` and `k6a_gov.ko` from the dry zip so `flash_generic` and the `.ko` copy skip,
zip the tree, run `update-binary` against that zip, and unpack the resulting
`boot-new-dry.img` — it must carry the new vermagic. Afterwards `md5sum` the real partitions
against the backups. Do **not** point `block=` at a file: `setup_ak` only resolves `/dev/*`
paths and otherwise aborts with `Unable to determine … partition`. `update-binary` finishes with
`do.cleanup=1` → it deletes `$AKHOME`, so read the output file from *outside* `$AKHOME`.

| Check | Result |
|---|---|
| `dmesg` | `Linux version …-build348`, `k6a_features: … git_hash=c0504a8f`, f2fs `build346 → build348`, **0×** `Oops`/`BUG`/`panic` |
| boot partition | `md5 025d43f6f2a1b266c6200aab5ae7f07a` = byte-identical to the dry-run repack, unpacks to `…-build348` |
| dtbo | partition prefix md5 = shipped `dtbo.img` (content unchanged, no dts change) |
| module | `hash_verified=1 hash_state=1 build_hash=c0504a8f`, `legacy_mode=1` (keep enforcing) |
| new status keys | `temp_peak=`, `thresholds_source=`, `vendor_sconfig=`, `vendor_cpu_limits=` all present; `vendor_sconfig=0` == `cat /sys/class/thermal/thermal_message/sconfig` |
| `hysteresis` init | `fast=3000 normal=2000` — the new default survives, k6a-ctl does not overwrite it |
| bound | `echo "4000 2000" > hysteresis` → rc=0 (previously rejected above 1000). Write format is **positional `"fast normal"`**; `fast=…` or a single value → rc=1. Restore with `echo "3000 2000"` |
| temp signal | paired samples: `temp_peak` tracks the **max** of the four Gold zones, `temp` the **2nd-highest** (status is up to `poll_ms=250` stale, so sample both in one shell). `echo 25 > poll_ms` does **not** take — it stays 250 |
| CD_L3 no dwell | `Build347: enter CD_L3 @86 without dwell` under 8× `sha256sum /dev/zero`; `policy_max` 2304000 → 1209600 → back to 2304000, de-escalations still exactly +2048 ms |
| deadband | `cd_thresholds 86 89 91 85` → `Build347: cd_recover band only 1 K (85 -> 86), gold zone spread runs wider than that` |
| F18 | `echo 1 > profile` → rc=0, thresholds untouched, `thresholds_source=user`, `Build347: profile=1 keeps user thresholds, write '-' to cd_thresholds to release`; `echo -` → `83 86 90 78` + `Build347: user thresholds released, profile defaults restored` |
| `profile` store | takes the **index**, not the name: `OFF=0 GAMING=1 BATTERY=2 BADAZZ=3 CUSTOM=4 SAFE=5`; `echo gaming` → rc=1 |
| k6a-ctl gate | `sh bin/check_module.sh <k6a-ctl repo>` → `GATE OK — 0 warn(s)`. Passing the kernel repo or `/data/adb/modules/k6a-ctl` fails — the gate expects the repo layout (`bin/`, `config/`, `webroot/`) |
| dmesg markers | `Build347:` ×8 on the edited lines; untouched lines keep `Build345:`/`Build340:` — markers mean *authoring build*, the load banner therefore still reads `Build345: k6a_gov v1.5.0 loaded` |

End state of the round: `profile=custom`, `cd_thresholds=86 89 91 83 1708800 1209600 1094400`,
`thresholds_source=user`, `hysteresis fast=3000 normal=2000`, `poll_ms=250`.
**`thresholds_user` resets to `false` on every reboot** (source back to `profile`, thresholds back
to the profile table `83 86 90 78`) and k6a-ctl never writes `cd_thresholds`, so the write has to
be repeated after each boot.

Not yet done: the T8 fps round (needs the user).

### Build 345 scope (K1–K7 + D) — flashed as `build346` 2026-10-03, passed
The governor version stays **1.5.0** (`K6A_GOV_VERSION` untouched: there is no
`GOV_KO_VER` migration window left to spend, and vermagic already locks the `.ko` to the
build). The build identity now comes from the **git hash**, not from the version string.

- **D — real build hash.** `drivers/thermal/Makefile` computes
  `K6A_H := $(shell git -C $(srctree) rev-parse --short=8 HEAD 2>/dev/null || echo unknown)` and
  exports it through `subdir-ccflags-y += -DK6A_GIT_HASH='"$(K6A_H)"' -DK6A_BUILD_HASH='"$(K6A_H)"'`.
  `scripts/Makefile.lib:11` accumulates `subdir-ccflags-y` into the exported
  `KBUILD_SUBDIR_CCFLAGS`, so the flag reaches both `drivers/thermal/k6a_features.c` and the
  `drivers/thermal/k6a_gov/` sub-directory (`orig_c_flags` at `scripts/Makefile.lib:105`).
  Both `.c` files now `#ifndef`/`#error` instead of falling back to `"full-synergy"` — a build
  without the flag fails loudly instead of shipping a lie.
  `verify_build_hash()` trims trailing whitespace and compares **length + `memcmp`** (the old
  `strncmp` accepted any runtime hash that merely *started* with the build one) and now returns
  `2 = unavailable` when `k6a_features/git_hash` is missing while `/sys/kernel` opens.
  `git_hash` and `build_hash` are both printed (`k6a_features: … git_hash=%s`, `status: build_hash=%s`).
  **Note when testing locally:** a bare `make drivers/thermal/k6a_gov/k6a_gov.o` short-circuits
  the recursion and skips `drivers/thermal/Makefile` entirely (the flag is then missing and the
  `#error` fires); build `drivers/thermal/` or the full image instead.
- **Fail-safe instead of fail-open.** The `gov->legacy_mode = 0;` on hash mismatch is gone —
  enforcement stays on, `pr_err("Build345: hash mismatch! … enforcement stays on")` fires and
  `hash_state=2` turns the WebUI row red (`✗ mismatch`, `webroot/app.js:152`).
- **K1 — gold cap is verified, not assumed.** `enforce_max_freq()` re-reads the gold policy after
  `cpufreq_update_policy()`: if `policy->max` is still above the requested cap, or a release did
  not bring it back up to `cpuinfo.max_freq`, the cached `enforced_max` is reset so the next tick
  retries, and the failure is logged `pr_err_ratelimited`. Success logs
  `Build345: gold cap %u Hz state=%u policy_max=%u`.
- **K2 — the gold cluster is identified properly.** `find_gold_cpu()` picks the policy with the
  highest `cpuinfo.max_freq` that does **not** contain CPU 0 (CPU 0 always sits in the silver
  policy) and returns **-1** when there is no distinct gold cluster; every caller guards it
  (the old fallback `? : 6` could hand `cpufreq_update_policy()`/`cpufreq_cpu_get()` a CPU that
  is not gold). `freq_init_worker()` copies `related_cpus` into `gov->gold_mask` under
  `gov->lock`; `cpufreq_notify()` uses `is_gold_policy()` instead of comparing `p->cpu == gold_cpu`,
  so the cap applies to every CPU of the gold policy, not just to one of them.
- **K3 — battery guard latches.** The trip sets `gov->batt_latched`, the release needs
  `battery_guard_temp - 5` (5 K hysteresis) and `batt_latched` also gates the `CD_L2 → GAMING`
  recovery, so a hot battery can no longer be undone by a `cd_recover` dip one tick later.
  The guard runs **before** `state_machine()` on purpose: it only ever trips *from* `GAMING`, so
  it can never downgrade a `CD_L3/L4` that the state machine just entered for the CPU, while the
  latch keeps the recovery from undoing it. `batt_latched` is exported in `status`.
- **K4 — dwell no longer depends on a noisy per-tick delta.** `prev_temp` is gone; escalation is
  evaluated first and is always immediate (`state < CD_L4 && t >= cd_l4`, `CD_L2 && t >= cd_l3`),
  de-escalation always uses `hysteresis_normal`, entering `CD_*` from `GAMING` always uses
  `hysteresis_fast`. The old `delta >= 5 / <= -5` ternary picked the dwell from a difference that
  flips sign every tick around ~770 ms of sensor noise. `entry_since` is reset inside
  `set_state_locked()` so a transition can never inherit a stale dwell start.
- **K5 — BW floors change-detect + readback.** `enforce_bw_floors()`/`reset_bw_floors()` write
  only when the target changed (`bw_last_gpubw`/`bw_last_llcc`, seeded `~0u`), verify the write
  through `k6a_devfreq_get_bw()` and only latch when the floor actually took effect, so a failed
  write retries on the next tick instead of going quiet. One `Build345: bw floors …` line per
  change instead of two writes per tick.
- **K6 — thermal zone lookup is cached.** `read_temp()` used to do four
  `thermal_zone_get_zone_by_name()` string lookups per tick; the pointers are now cached in the
  struct (the core takes no reference — `thermal_core.c:1493`, and the zones live for the whole
  uptime) and a failed `thermal_zone_get_temp()` invalidates the slot for the next tick.
- **K7 — boot default governor.** `sweet_defconfig`: `CPU_FREQ_DEFAULT_GOV_PERFORMANCE` →
  `CPU_FREQ_DEFAULT_GOV_SCHEDUTIL`, so the kernel comes up on schedutil instead of pinned at
  `performance` (k6a-ctl still force-writes `schedutil` afterwards — F1 of v1.3.2 — but the
  default no longer fights it).

Local build recipe used to gate this scope (Termux, no `as`, host tools of `scripts/` do not
build here, so the single-file shortcut does not work):

```bash
make ARCH=arm64 stackp-name= -o scripts -o vdso_prepare KCFLAGS=-fintegrated-as -k drivers/thermal/
```

### After the flash — build346 (passed 2026-10-03)

Release `v4.14.369-badazz-build346`, `build-info.txt` → `BUILD_NUMBER=346`,
`COMMIT_SHA=699eee6e897a48db5610843fc30c2705906a2d69` (the docs commit `699eee6e8` on top of the
scope), vermagic `4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-build346`, `k6a_gov.ko` 48 488 B.
`strings` on the shipped `.ko` **and** on `Image.gz` contain `699eee6e` and **no `full-synergy`**
— the `subdir-ccflags-y` hash flag reached both compilation units (D).

```
$ /system/bin/su -c 'dmesg | grep Build345'
[15.351586] Build345: gold cluster cpu 6 related=6-7 freqs=13
[15.353100] Build345: k6a_gov v1.5.0 loaded (legacy=1 profile=1 freq_init=deferred)
[15.354503] Build345: build hash verified (699eee6e) retries=0
[15.354544] Build345: OFF -> GAMING
[15.354565] Build345: bw floors gpubw=0 llcc=0 state=1
[76.764117] Build345: gold cap 1708800 Hz state=2 policy_max=1708800
[76.764142] Build345: bw floors gpubw=4000 llcc=0 state=2
[78.816317] Build345: gold cap 0 Hz state=1 policy_max=2304000
[89.595209] Build345: gold cap 1209600 Hz state=3 policy_max=1209600
[89.595245] Build345: bw floors gpubw=3000 llcc=4000 state=3

$ /system/bin/su -c 'cat /sys/kernel/k6a_gov/status'
version=1.5.0 state=gaming legacy_mode=1 hash_verified=1 hash_state=1 build_hash=699eee6e
batt_latched=0 gold_max=0 gold_max_tbl=2304000 policy_max=2304000 state_age_ms=332938
hist=15354:72:off>gaming,76764:83:gaming>cd_l2,78816:73:cd_l2>gaming,85472:85:gaming>cd_l2,
     87528:73:cd_l2>gaming,89595:89:gaming>cd_l3,91640:69:cd_l3>cd_l2,93688:71:cd_l2>gaming, …

$ /system/bin/su -c 'cat /sys/kernel/k6a_features/git_hash'   → 699eee6e
$ /system/bin/su -c 'grep -E "^(version|versionCode)=" /data/adb/modules/k6a-ctl/module.prop'
version=1.3.2  versionCode=132                                  (controller pid 3935 running)
policy0 gov=schedutil max=1804800    policy6 gov=schedutil max=2304000
```

| Check | Expected | Read |
|---|---|---|
| D — `build_hash` == `git_hash` | the real 8-char SHA, never `full-synergy` | `699eee6e` == `699eee6e` ✅ (+ dmesg `build hash verified (699eee6e) retries=0`) |
| Fail-safe arm | `legacy_mode=1`, `hash_state=1`, **no** `hash mismatch` line | `legacy_mode=1 hash_state=1`, 0× `hash mismatch` ✅ |
| K1 — gold cap applied | `gold cap … policy_max=…` equal, no retry lines | `1708800/1708800`, `1209600/1209600`, `0/2304000`; **0×** `not applied` / `release stuck` ✅ |
| K1/K5 — sampling | 70 samples @1 s: `policy_max == scaling_max_freq`, `gold_max=0` in GAMING | **70/70 match, 0 mismatch** ✅ (the window stayed in `gaming`; the capped side is covered by the K1 dmesg lines above) |
| K2 — gold cluster | a real mask, CPU 0 excluded | `gold cluster cpu 6 related=6-7 freqs=13` (gold = cpu6/7, `-1` guard would have logged) ✅ |
| K4 — dwell | de-escalation always `hysteresis_normal` (2 s), escalation immediate | every `cd_*>gaming` / `cd_l3>cd_l2` follows its entry at **+2.0 s** (76.7→78.8, 85.5→87.5, 89.6→91.6, 93.7→95.5); `state_age_ms=332938` ≫ 2000 ✅ |
| K5 — BW change-detect | one line pair per transition, nothing per tick | exactly 2 lines per state change; **0×** `Build345: set_bw … not reflected` ✅ |
| K3 — battery guard | `guard=1`, `temp_thresh=45`, `batt=39.5 °C` → `batt_latched=0` | as read, counter `throttle_events=4` only from CPU CD entries ✅ |
| K6 — temp source | `temp_src=1`, `temp_valid=1`, idle 75 °C | `75` °C, inside/below the 74–87 °C build344 baseline ✅ |
| K7 — boot default | `schedutil`, not `performance` | `p0`/`p6` both `schedutil` (with k6a-ctl v1.3.2 also forcing it, so this is *not* independent proof — see the caveat below) ⚠️ |
| k6a-ctl | v1.3.2 still installed | `version=1.3.2` / `versionCode=132`, controller pid 3935 ✅ |

**Two caveats, both read-only findings, neither a defect of this build:**

1. **Never use `/proc/config.gz` as config evidence on this device.** It reports
   `CONFIG_LOCALVERSION="-perf"` and `CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE=y`, which cannot
   belong to the running kernel: the boot banner is built from `LOCALVERSION` and says
   `4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-build346`. It is part of the SUSFS identity
   spoof (same source as `uname`/`/proc/version`, see *On-device Capture*). **K7's proof is
   therefore `arch/arm64/configs/sweet_defconfig`** (`DEFAULT_GOV_SCHEDUTIL=y`,
   `PERFORMANCE is not set`, re-checked with `make sweet_defconfig && make olddefconfig`) plus
   the fact that CI builds from exactly that defconfig.
2. `uname -a`, `/proc/version` and `/proc/sys/kernel/osrelease` report
   `6.12.0-android16-6.12-perf-g1d01cd293` (SUSFS, patches `init_uts_ns`). The only trustworthy
   build proof in a live shell is `dmesg | head -2` (`Linux version 4.14.369-…-build346`) plus the
   `Build345:` markers.

### Location
- `drivers/thermal/k6a_gov/k6a_gov.c` (1284 lines, `CONFIG_K6A_GOV=m`) — v1.4.0 `341f6f5b0`,
  v1.5.0 `c42a67d07`
- `drivers/thermal/k6a_gov/Kconfig` / `Makefile`
- `drivers/gpu/msm/kgsl_pwrctrl.c` — `kgsl_k6a_get_levels()` + `kgsl_k6a_set_max_level_idx()`
- `drivers/devfreq/devfreq.c` — `k6a_devfreq_get_bw()` + `k6a_devfreq_set_bw()` (**the `min` half
  of `set_bw` used to be `if (min) …`, which silently ignored a release — it is unconditional now**)

### What v1.5.0 changed (Build 341)
- **Loadable module** — `CONFIG_K6A_GOV=m`; the kernel zip ships `k6a_gov.ko`, k6a-ctl's
  `service.sh` insmods it. Everything else about the file is unchanged: `module_init/exit`,
  `MODULE_*`, `module_param(legacy_mode/profile)` were already there.
- **`hash_state` instead of a latch** — `verify_build_hash()` returns 1 match / 0 retry /
  -1 mismatch / **2 unavailable** (Build 345: `k6a_features` missing while `/sys/kernel`
  opens — skips straight to `3` instead of burning the 60 retries), the governor thread retries
  every 250 ms up to 60× (15 s) and only then gives
  up. `hash_state` 0 pending / 1 verified / 2 mismatch / 3 gave up or unavailable; `hash_verified` is now
  `hash_state == 1` so k6a-ctl and the WebUI keep working.
- **No `LINUX_VERSION_CODE` change** — see *Build 341 scope, finding 2*: the Makefile cap at
  `255` stays. The boot-time `build/run version delta` warning is gone because the check itself
  was deleted (`#include <linux/version.h>`, `K6A_GOV_KERNEL_VER`, the `pr_warn`). Vermagic
  (`4.14.369-…-buildNNN`) is the real mismatch guard and does not use `LINUX_VERSION_CODE`.
- Markers are `Build341:` (`k6a_gov v%s loaded`, `build hash verified`,
  `k6a_features/git_hash not readable yet, retrying`, `hash verify gave up`) and — since
  Build 345 — `Build345:` for everything the scope touched (`k6a_gov v%s loaded`,
  `build hash verified`, `hash mismatch`, `gold cluster/cpu`, `gold cap`, `escalate`,
  `bw floors`, `battery guard`, `OFF -> GAMING`). Older `Build340:` lines in untouched paths
  are left alone; dmesg only ever holds the current build anyway.

### Features
- State Machine: OFF→GAMING→CD_L2/L3/L4, an **entry dwell** before GAMING gives way to CD_L2,
  but **no dwell at all** once the cluster already sits at `cd_l3_temp`
  (`Build347: enter CD_L3 … without dwell`) and **immediate escalation** L2→L3, L2→L4
  (`Build345: escalate …`) plus the peak-triggered L4 (`Build347: escalate -> L4 … (peak)`),
  all counted in `throttle_events`; only recovery is dwell-gated — `hysteresis_fast` on the way
  in (default **3000 ms** since Build 347, store bound now 1..5000), `hysteresis_normal` on the
  way out (Build 345 dropped the per-tick `delta >= 5` ternary that picked the dwell from a
  noise-flipping difference). Three zero thresholds (reachable from sysfs) short-circuit the
  whole switch instead of pinning `CD_L4` at `t >= 0`.
- Temp: the **2nd-highest of the 4 Gold zones** `cpu-1-0..3-usr` drives every state decision,
  the **peak is kept in `temp_peak` and arms the L4 emergency only** (Build 347); fallbacks
  `cpu-0-0-usr` → `xo-therm` → `soc-therm` → `thermal_zone0` set both values identically.
  Zone **pointers are cached** since Build 345 (K6), so a tick costs five
  `thermal_zone_get_temp()` calls instead of nine lookups + reads. Failure returns 0 and clears
  `temp_valid` → `state_machine()` and the battery
  guard are skipped and the state is held (`Build340: no temperature source, holding state %u`).
  v1.3.1 returned a hard-coded `40` and throttled on fake data. `temp_src` says which zone answered.
  Why 2nd-highest: at idle the four Gold zones were measured at 74.1/74.5/**82.8**/78.7 °C, a
  6.2 K spread that sits *inside* the `86` entry / `83` recover band — so `max()` keyed CD off
  whichever core happened to be hottest, could barely recover, and made two runs with identical
  thresholds differ by 13 % vs 58 % CD time.
- CPU: `find_gold_cpu()` returns the highest-`cpuinfo.max_freq` policy **without CPU 0**, or -1
  (Build 345; the old `?: 6` fallback could name a CPU outside the gold cluster), `clamp_freq`
  order-independent, `enforce_max_freq()` tracks `gov->enforced_max`, **re-reads the policy to
  prove the cap took** (K1) and releases through `cpufreq_update_policy()`, which resets min/max
  from `policy->user_policy` (never touched by us) and re-runs `CPUFREQ_ADJUST` —
  `cpufreq_notify()` clamps **every CPU of `gov->gold_mask`** (K2) and only while the state is
  ≥ CD_L2. Released on GAMING recovery, `enable=0`, `legacy=0`.
  `cpufreq_notify()` does not take `gov->lock`, so there is no recursion and no lock inversion.
- GPU: native enforcement via KGSL pwrlevels, caps per CD state
- BW: floors `gpubw` + `cpu-llcc-ddr-bw` per profile/CD state, `bw_floors` sysfs — written
  **only when the target changed and only latched after a successful readback** (Build 345 K5),
  which keeps the release paths honest *and* stops the per-tick write storm. Before Build 338
  the `if (gpubw)` guard plus `k6a_devfreq_set_bw()`'s `if (min)` made every release path a no-op
  — live proof on Build 338: `state=gaming` while `bw_gpubw min=4000`.
- Battery: `battery_guard` @`battery_guard_temp` (35..60, default 45) → CD_L2 via `power_supply`,
  **latched** until the battery is 5 K below the threshold (Build 345 K3), counted as a
  throttle event
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
  `hysteresis` 1..**5000** / 1..5000 (the `fast` cap was 1000 until Build 347, which made a
  sane entry dwell unexpressible), `poll_ms` 100..5000, `battery_guard_temp` 35..60
- `cd_thresholds`: a 7-value write marks the values **user-owned** (`thresholds_source=user`);
  from then on `profile_store` only changes the profile *name* and logs
  `Build347: profile=N keeps user thresholds …` instead of clobbering them — k6a-ctl's
  `gov_write "profile"` (`bin/k6a-controller:145`, auto_badazz @85 °C and every mode switch)
  used to silently replace a tuned config mid-round. Writing a bare `-` releases them again and
  restores the profile table unless the profile is `custom`. A `cd_recover` band narrower than
  4 K logs a warning (the Gold zone spread runs wider than that).
- Sysfs: `enable/profile/status/hysteresis/cd_thresholds/gpu_caps/bw_floors/battery_guard/
  battery_guard_temp/poll_ms/legacy/game_pid`
- Status keys added in v1.4.0: `policy_max` (live `policy->max`, proves the cap is applied),
  `state_age_ms`, `temp_src`, `temp_valid`; in Build 347: `temp_peak`,
  `thresholds_source` (`user`|`profile`), `vendor_sconfig`, `vendor_cpu_limits` (the vendor
  `mi_thermald` engine, polled every 64 ticks via `filp_open`/`kernel_read` on
  `/sys/class/thermal/thermal_message/{sconfig,cpu_limits}` — k6a was blind to it while
  `sconfig` jumped 0→13 mid-round; the read needs no new SELinux grant because k6a-ctl's
  `sepolicy.rule` already grants `kernel → sysfs` read/open/getattr generically)
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
`LINUX_VERSION_CODE` — capped at 4.14.255 on this tree on purpose (see *Build 341 scope*).

`Build340: escalate …` correctly printed **zero** times: every observed transition was either
direct `gaming>cd_l3` (the GAMING branch has no marker by design) or a de-escalation
(`cd_l3>cd_l2`, `cd_l2>gaming`), and neither escalates. The markers fire only on L2→L3,
L2→L4, L3→L4.

Reproduce a CD transition with a CPU load (e.g. `sha256sum` on 8 cores) and watch `state` and
`policy_max` move together; after the load stops, `scaling_max_freq` must return to `2304000`.

### sweet_defconfig
- `CONFIG_K6A_GOV=m` (was `=y` until Build 341)
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
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_COMPAT=arm-linux-gnueabihf- -j$(nproc) modules
# single object / single module:
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- drivers/thermal/k6a_gov/k6a_gov.o
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- drivers/thermal/k6a_gov/k6a_gov.ko
# ZIP (CI does this — see .github/workflows/build-kernel.yml):
cp arch/arm64/boot/Image.gz arch/arm64/boot/dtb.img arch/arm64/boot/dtbo.img \
   drivers/thermal/k6a_gov/k6a_gov.ko anykernel/
cd anykernel && zip -r9 ../BadazzKernel-sweet-k6a-gov-v1.3.2.zip . -x "*.git*"
```
Local full builds are blocked in Termux — see **Local Build Notes (Termux)** above.

### Build 341: `CONFIG_K6A_GOV=m` — landed in `c42a67d07`

The governor was *written* as a module — `module_init(k6a_gov_init)` / `module_exit(k6a_gov_exit)`
and `MODULE_LICENSE/AUTHOR/DESCRIPTION/VERSION` are the last five lines of the file, and
`Kconfig` is `tristate` with `default m` and the help line *"Build as module for version-lock
with kernel."* Until Build 341 only `arch/arm64/configs/sweet_defconfig:208` forced it
back to `CONFIG_K6A_GOV=y`.

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

Landed (`c42a67d07`):

1. `arch/arm64/configs/sweet_defconfig:208` `CONFIG_K6A_GOV=y` → `=m`.
2. `.github/workflows/build-kernel.yml` — `REQUIRED_CONFIGS` `"CONFIG_K6A_GOV=m"`; the build
   step runs `make Image.gz dtb.img dtbo.img` **then a second `make modules`** (separate
   invocation so `Module.symvers` from the vmlinux link already exists); the *Verify
   post-build config* step now also asserts `CONFIG_K6A_GOV=m` and the existence of
   `drivers/thermal/k6a_gov/k6a_gov.ko`; the ZIP step copies it into `anykernel/` and writes
   `K6A_GOV_VERSION=` (read out of the `.ko` `.modinfo`) into `build-info.txt`.
   Side effect: `make modules` also builds `CONFIG_MMC_TEST` → `mmc_test.ko`. It is never
   copied into the ZIP.
3. `anykernel/anykernel.sh` copies `k6a_gov.ko` to **`/data/adb/k6a_gov.ko`** (and to
   `/data/adb/modules/k6a-ctl/k6a_gov.ko` if that dir exists), with a `ui_print` so the flash
   log shows whether it landed. It does **not** use AK3's `do.modules` — that writes to
   `/system/lib/modules`, and this device is `ro.boot.veritymode=enforcing` with `/` mounted
   `ro` on `/dev/block/dm-0`, so the rw remount would fail. `setup_env` mounts `/data` before
   `ash anykernel.sh`, and the script now ends with `exit $AK_RC` so the flash result is still
   the one from `flash_dtbo`.
4. `k6a-ctl` v1.3.0 `service.sh` does the `insmod` (search order: module dir → `/data/adb` →
   `/system/lib/modules` → `/vendor/lib/modules`), the version lock and the fallback.

#### Proof that `insmod` works on this device (measured 2026-10-02, no flash needed)

| Question | Answer |
|---|---|
| Which domain runs `service.sh`? | `u:r:ksu:s0` (pid 1932) — same as every k6a-ctl process |
| Capabilities there? | `CapPrm=CapEff=CapBnd=0000003fffffffff` → `CAP_SYS_MODULE` (bit 16) **present** |
| `su -c` capabilities? | `CapEff=0` **and `CapBnd=0`** — a `su -c` probe always fails with `EPERM` and proves nothing about SELinux |
| `kernel.modules_disabled` | `0` |
| SELinux | `enforcing=1`, but `u:r:ksu:s0` already has `system module_load` |

The SELinux question was settled by running a probe *inside* `u:r:ksu:s0` **with full
capabilities** (a temporary byte-revertible swap of the installed `webui-handler.sh`, which
`nc` respawns per request from a full-cap parent) against a deliberately wrong-vermagic
module. It passed `may_init_module()` and `security_kernel_read_file(NULL, READING_MODULE)`
— i.e. the AV `allow ksu ksu:s0:system module_load` granted by the policy — and failed only
at `check_modinfo`:

```
insmod: failed to load .../hybridmount-android12-5.10.ko: Exec format error
[ 3866.282482] hybridmount: version magic '5.10.252-dirty SMP preempt mod_unload modversions aarch64'
                                 should be '4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-build340 SMP preempt mod_unload aarch64'
```

**No `sepolicy.rule` is needed for *loading*** — `module_load` is granted by KSU's own policy.
The same output gives the second lock for free: vermagic ties a `.ko` to exactly one build, so
build344's `k6a_gov.ko` refuses to load anywhere else. A `sepolicy.rule` *is* required for
something else entirely, the build-hash read; see finding 3 below.

Hook chain, for reference (`kernel/module.c` + `security/selinux/hooks.c`):
`init_module()` → `may_init_module()` (`capable(CAP_SYS_MODULE)`, returns `-EPERM`) →
`copy_module_from_user()` → `security_kernel_read_file(NULL, READING_MODULE)` →
`selinux_kernel_module_from_file(file == NULL)` →
`avc_has_perm(current_sid(), current_sid(), SECCLASS_SYSTEM, SYSTEM__MODULE_LOAD)` →
`elf_validity()` → `check_modinfo()` (vermagic, `-ENOEXEC`).

If the load ever *is* denied in future builds, the fix is a one-line
`sepolicy.rule` in k6a-ctl: `allow ksu self:system module_load` (KSU applies
`sepolicy.rule` — three installed modules already ship one).

### Git History (main)
Full log: `git log --oneline -40`. Head is the Build-341 revert, on top of the scope itself:

```
11c2ae479 Build 341: revert the LINUX_VERSION_CODE change that broke CI
786aae15a docs: record Build 341 and the measured insmod path
c42a67d07 Build 341: turn k6a_gov into a module and close the version-lock findings
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

k6a_gov history: `11bc62c9a` Build 345 K1-K7 + D (real git hash, fail-safe hash check, Gold-cap
readback, `gold_mask`, battery latch, fixed dwell, BW change-detect, zone cache, schedutil
default), `699eee6e8` docs (pins that hash + the post-flash record; **shipped as `build346`**),
`11c2ae479` LINUX_VERSION_CODE revert, `c42a67d07` v1.5.0 module + hash_state, `341f6f5b0`
v1.4.0 cap/BW release + escalation + validation, `81d69ae` v1.3.1 ticks
fix, `d4835b6` deadlock, `53bb809` v1.3.1 hardening, `967c134` v1.3.0 BW floors + profile 5,
`dfcb96b` v1.2.1, `602a281` `CONFIG_K6A_GOV=y`.

## k6a-ctl Companion
- **Repo**: `vandalsquad187/k6a-ctl` branch `main` @ **`768d1bc` (v1.3.2, versionCode 132)**,
  release `v1.3.2` → asset `k6a-ctl-v1.3.2.zip` (23 456 B), local clone `~/k6a-ctl`.
  Previous: `eec3d96` (v1.3.1, sepolicy.rule + gate `[5c]`), `6317a27` (drop the stale
  `LINUX_VERSION_CODE` hint), `1513fd8` v1.3.0 (130),
  `426b516` v1.2.0 (120) → `k6a-ctl-v1.2.0.zip` (20 543 B).
  Recent commits: `768d1bc` **v1.3.2** (F1 force `schedutil` + `_ensure_gov` drift re-assert,
  F8 `w_checked`, F9 `cfg_load` single pass, F10 `read_temp` real return code, F11 F12 F13
  per-tick cost cuts), `eec3d96` v1.3.1 (sepolicy.rule + gate `[5c]`), `6317a27` (warning text),
  `1513fd8` v1.3.0 (Build 341: insmod + version lock), `426b516` v1.2.0 (Phase 5),
  `a049fac` v1.1.6 version format,
  `a9170b6` v1.1.5 **removed the USB autosuspend workaround** (so userspace no longer masks a
  kernel bug), `de3c037` v1.1.4 dwc3 autosuspend off, `18cf392` v1.1.3 log rotation +
  `battery_guard_temp`, `a639e7c` v1.1.2 robustheit, `e43ccf6` v1.1.1 whitelisted handler.
- **v1.3.2 is on the device** (deployed 2026-10-03, six files piped through
  `su -c "cat > …"` so SELinux contexts survive; backup `/data/adb/k6a-ctl-v1.3.1.bak`):
  `module.prop` `version=1.3.2` / `versionCode=132`, controller logged `k6a-ctl v1.3.2 start`,
  gate `[1]`–`[6]` all OK (incl. the new governor checks), `scaling_governor → schedutil` on
  `p0`/`p6`, a manual flip to `performance` was back on `schedutil` within one tick, and the
  WebUI mode switch (cfg hot-reload) works end to end.
- **v1.3.1 is on the device** (read 2026-10-03): `module.prop` `version=1.3.1` /
  `versionCode=131`, `sepolicy.rule` root-owned 94 B, controller logged
  `k6a-ctl v1.3.1 start`, `/ping` → `OK`.
  **ksud's `module install` is unusable here** — `su -c` runs with `CapBnd=0`, so it fails with
  `Error: Permission denied (os error 13)`; upgrades are done by unpacking to `/data/local/tmp`
  and **`mv`-ing each file over** (root-owned, so `chmod` works), which also leaves the running
  scripts' inodes intact. Backup of v1.2.0: `/data/local/tmp/k6a-ctl-v1.2.0.bak`.
  `data.txt` carries `gov_policy_max`, `gov_state_age`, `gov_temp_src=1`, `gov_temp_valid=1`,
  `gov_batt_guard=1`, `gov_game_pid=0`, `gov_version=1.5.0`, `gov_hash_state=1`.
- **`check_module.sh` is a build gate, not a shipped tool** — `build.sh:17` deliberately excludes
  it (with `build.sh` itself) from the ZIP. Earlier notes framed its absence on the device as a
  defect; that was wrong. Running it by hand is repo-side only:
  `sh <repo>/bin/check_module.sh <repo>`. Gate result 2026-10-03 (v1.3.1): **green, 0 warns**
  including `[5b] Version-Lock k6a_gov` and the new `[5c] sepolicy.rule` check (file present,
  `allow kernel sysfs file {read open getattr}` present, no `:` anywhere in it).
  **v1.3.2 renumbered the gates to `[1]`–`[6]`** (the old `[5*]` numbering had gaps and the
  new governor checks needed room): result 2026-10-03 (v1.3.2) **green, 0 warns**, `[6]` being
  the governor block (`_ensure_gov()` in `k6a-lib.sh` *and* `k6a-controller`, the
  `scaling_governor` write, `w_checked()`, no `echo 40` in `read_temp`). Mentions of `[5b]`/
  `[5c]` further down describe the v1.3.0/v1.3.1 numbering and are kept as written.
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
  - `_startup` logs `k6a_gov <version> legacy=<n>` and warns unless the version matches `1.5.*`.
- **Phase 6 shipped (`1513fd8`, v1.3.0)**:
  - `service.sh` runs `_load_gov()` before the boot_completed wait: `insmod` from
    `$MODDIR` / `/data/adb` / `/system/lib/modules` / `/vendor/lib/modules`, then a
    three-stage version lock (`.ko` `.modinfo` → runtime `status` → kernel vermagic).
    `GOV_KO_VER=1.5.0`; every failure logs one WARN and returns, so the controller's own
    CD_L2/L3/L4 state machine is the permanent fallback — never a brick.
  - `gov_hash_state` (0/1/2/3) surfaced in `data.txt` and in the WebUI "Build Hash" row,
    which can now say "nicht geprüft" instead of pretending `hash_verified=1` means checked.
  - `bin/check_module.sh` gate `[5b]` fails the build when `service.sh`'s `GOV_KO_VER` and
    the controller's `1.5.*` expectation drift apart.
- **Mode**: `delegated=1` — Kernel handles thermal (CPU/GPU/BW), module handles game detection +
  sched tuning + auto `badazz_safe` @85°C
- **WebUI**: Gov-Status (+ policy_max/state_age/temp_src/temp_valid), BW-Floors, GPU-Caps,
  History timeline, Akku-Guard toggle
- **Do not re-enable before Build 340** *(historical — done)*: on v1.3.1 the Gold cap and the
  BW floors were never released (see *Pre-fix evidence*), so a userspace cooldown would have
  fought the kernel. The `disable` file was dropped after the Build 340 acceptance run.
- **TEIL 3 done**: the governor version string is now burned into both sides —
  `GOV_KO_VER=1.5.0` in `service.sh`, `1.5.*` in the controller, `K6A_GOV_VERSION "1.5.0"`
  in the kernel — with `check_module.sh` gate `[5b]` keeping the two shells in sync, plus
  vermagic as the kernel-side lock.

## Build 341 scope (finding 1 FIXED in `c42a67d07`; finding 2 investigated and dropped; finding 3 fixed in k6a-ctl v1.3.1)

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

### 2. `LINUX_VERSION_CODE` is pinned to 4.14.255 — **tried, failed CI, reverted**

`Makefile:1392-1396` does **not** use `$(SUBLEVEL)`:

```make
define filechk_version.h
	(echo \#define LINUX_VERSION_CODE $(shell                         \
	expr $(VERSION) \* 65536 + 0$(PATCHLEVEL) \* 256 + 255); \
	echo '#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))';)
endef
```

so `include/generated/uapi/linux/version.h` is always `265983` (4.14.**255**) while the top
Makefile says `SUBLEVEL = 369`, and `k6a_gov` warned on every boot:

```
[0.776990] k6a_gov: build/run version delta (266097 vs 265983) — continuing (built-in)
```

The `255` is not an accident: `KERNEL_VERSION` stores `SUBLEVEL` in 8 bits, so 369 would be
emitted as `266097`, which decodes back as **4.15.113** — `KERNEL_VERSION(4,14,369)` and
`KERNEL_VERSION(4,15,113)` are the *same number*.

**Attempted in `c42a67d07` as `+ 0$(SUBLEVEL)`, CI run `37067437853` failed after 14m37s:**

```
drivers/staging/qca-wifi-host-cmn/qdf/linux/src/i_qdf_timer.h:76:17: error: implicit
  declaration of function 'timer_setup_on_stack'; did you mean 'hrtimer_init_on_stack'?
  [-Werror=implicit-function-declaration]
make[3]: *** [scripts/Makefile.build:364:
  drivers/staging/qcacld-3.0/core/hdd/src/wlan_hdd_assoc.o] Error 1
make: *** [Makefile:1238: drivers] Error 2
```

**The first audit was wrong.** It only searched for `KERNEL_VERSION(4,14,N)` with `N > 255`
(exactly one hit, our own file) and never enumerated the **alias range**. Moving
`265983 → 266097` flips every gate in `[265984 .. 266097]`, and that range is not empty —
measured over the whole tree (`*.c`/`*.h`/`*.S`, `.git` excluded):

| Gate | Hits | File |
|------|-----:|------|
| `KERNEL_VERSION(4,15,0)` = 265984 | 17 | `drivers/staging/qca-wifi-host-cmn/qdf/linux/src/i_qdf_timer.h` ×2, `qdf_mc_timer.c`, `drivers/staging/qcacld-3.0/.../wlan_hdd_cfg80211.c`, `net/wireguard/compat/compat.h` ×10, `net/wireguard/compat/ptr_ring/include/linux/ptr_ring.h`, **`KernelSU-Next/kernel/kernel_compat.h:288`**, **`KernelSU-Next/kernel/selinux/sepolicy.c:30`** |
| `KERNEL_VERSION(4,15,8)` = 265992 | 1 | `net/wireguard/compat/compat.h:600` |
| `KERNEL_VERSION(4,15,29)` = 266013 | 3 | `net/wireguard/compat/compat.h` ×3 |
| `KERNEL_VERSION(4,14,369)` = 266097 | 1 | `drivers/thermal/k6a_gov/k6a_gov.c` (the `!=` that printed the warning) |
| **Total** | **22** | 21 of them outside our own file |

How each one breaks — read from the source, not guessed:

- `i_qdf_timer.h:54` — `#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)` selects the
  `timer_setup_on_stack()` code path, which **does not exist in 4.14** → the CI error above.
- `KernelSU-Next/kernel/selinux/sepolicy.c:30` — defines `CONFIG_IS_HW_HISI` when
  `>= 4.14.0 && < 4.15.0`. Flipping it would **silently** stop defining it and change the
  SELinux ebitmap path on a rooted device. Not a trade worth one `pr_warn`.
- `KernelSU-Next/kernel/kernel_compat.h:288` — `#if < KERNEL_VERSION(4,15,0)` provides a
  `__weak groups_sort()` no-op; flipping removes it.
- `net/wireguard/compat/compat.h:27` — `#elif >= 4.15 && < 4.16` selects `ISUBUNTU1804`
  (wrong compat profile for a 4.14 tree); `:600/:326/:727/:898` activate shims keyed on
  `4.15.0..4.15.8` / `4.15.29`.
- `wlan_hdd_cfg80211.c:15846` — `>= 4.15.0` compiles the OCE scan-flag helper.

`KernelSU-Next` is a **git submodule**; two of those sites cannot even be patched in this repo.

**Decision: the `+ 255` cap stays.** `Makefile` reverted to `+ 255`, so
`LINUX_VERSION_CODE = 265983` again and CI sees byte-identical preprocessor behaviour to the
last green build (re-audit of `[265984..265983]` → **0 gates**, i.e. no change at all).

The warning is removed on the consumer side instead: `k6a_gov.c` lost
`#include <linux/version.h>`, `#define K6A_GOV_KERNEL_VER KERNEL_VERSION(4,14,369)` and the
`if (LINUX_VERSION_CODE != …) pr_warn(...)` block. That check compared two constants compiled
from the same headers, so inside this tree it could only ever report the known cap; it had no
action attached. The mismatch guard that actually matters for the version lock is **vermagic**
(`4.14.369-openela-rc1-BadazzKernel-sweet-v1.3.2-buildNNN`), which is built from `UTS_RELEASE`
and rejects a stale `.ko` at `insmod` time — it does not consult `LINUX_VERSION_CODE`.

Regenerate with `make ARCH=arm64 include/generated/uapi/linux/version.h` (confirmed → `265983`);
`make sweet_defconfig` alone does **not** touch it.

### Landed — `c42a67d07` (2026-10-02)

1. `arch/arm64/configs/sweet_defconfig:208` `CONFIG_K6A_GOV=y` → `=m`.
2. CI: `REQUIRED_CONFIGS` `"CONFIG_K6A_GOV=m"`; `make Image.gz dtb.img dtbo.img` followed by a
   second `make modules`; `.ko` existence asserted in *Verify post-build config*; copied into
   `anykernel/` and `K6A_GOV_VERSION` recorded in `build-info.txt`.
3. `k6a-ctl` `service.sh` (v1.3.0): `insmod` + `GOV_KO_VER=1.5.0` version lock (`.ko`
   `.modinfo` pre-check, runtime `status` post-check with `rmmod` on mismatch), falling back
   to the legacy userspace cooldown on every failure. `bin/check_module.sh` gained gate `[5b]`
   which fails the build if `service.sh`'s `GOV_KO_VER` and the controller's `1.5.*`
   expectation drift apart.
4. Finding 1 fixed in the same commit: `hash_state` (0/1/2/3) + bounded retry replace the
   unconditional `hash_verified = true` latch.
5. Finding 2 **attempted and reverted in a follow-up commit**: `+ 0$(SUBLEVEL)` broke CI on
   `qca-wifi-host-cmn` (see the finding-2 section). `Makefile` is back to `+ 255` and the
   `build/run version delta` check is gone from `k6a_gov.c` instead.
6. Finding 3 fixed in k6a-ctl `eec3d96` (v1.3.1): `sepolicy.rule` grants the governor thread
   read access to sysfs — without it `hash_state` stays `3` forever (see below).

### 3. The hash check never had sysfs access (found on the build344 flash, fixed in k6a-ctl v1.3.1)

build344 booted with everything green — `Build341: k6a_gov v1.5.0 loaded`, `.ko` present,
`version=1.5.0` — except:

```
[10.940687] Build341: k6a_features/git_hash not readable yet, retrying
[26.040150] Build341: hash verify gave up after 60 tries
status: hash_verified=0 hash_state=3
```

`k6a_features` was fine all along: `cat /sys/kernel/k6a_features/git_hash` → `full-synergy`,
and the node existed since `[0.772956]`. The governor's own `filp_open()` did not run as a
normal user though — `verify_build_hash()` executes inside `gov_thread`, the kthread at
`/proc/<pid>/comm == [k6a_gov]`, whose SELinux domain is **`u:r:kernel:s0`**.

Measured with the world-writable selinuxfs oracle (write both SIDs and the class index, read
the `allowed` mask back from the *same* fd — `exec 3<>/sys/fs/selinux/access`):

| query | before | after the rule |
|---|---|---|
| `kernel → sysfs:file` (class 6) | **`0`** | **`40012`** (read+open+getattr) |
| `kernel → sysfs:dir` (class 8) | `10000010` (search+getattr) | `10040012` (+read +open) |
| `ksu → sysfs:file` (control) | `ffffffff` | — |

Permission bit index is **1-based** (`perms/<name>`), so the AV bit is `1 << (n-1)`:
`file.read`=2 → bit 1, `file.getattr`=5 → bit 4, `file.open`=19 → bit 18 → `0x40012`.

**Why nobody ever saw this in `dmesg`:** `auditdeny` for `kernel/sysfs/file` has bit 2 (`read`)
clear, i.e. the denial is `dontaudit`ed while still being enforced. The fix lives in
k6a-ctl's `sepolicy.rule` (colon-less syntax — ksud rejects `allow x y:z …` with
`Failed to parse policy statement` and `ksud sepolicy apply` then silently skips it):

```
allow kernel sysfs dir {search read open getattr}
allow kernel sysfs file {read open getattr}
```

ksud applies `/data/adb/modules/*/sepolicy.rule` at boot; proven on this device by the two
modules that already shipped one — rezygisk's `allow zygote ksu dir search` reads back as
`zygote → ksu:dir = 10000000`, brene's as `netd → adb_data_file = 10000000`. k6a-ctl gate
`[5c]` now fails the build if `sepolicy.rule` is missing, lacks the grant, or contains a colon.

#### After the flash — measured on build344 (passed 2026-10-03, after one reboot)

```
$ /system/bin/su -c 'cat /sys/kernel/k6a_gov/status'
version=1.5.0 state=cd_l3 legacy_mode=1 hash_verified=1 hash_state=1

$ dmesg | grep -E 'Build341|hash'
[10.961695] Build341: k6a_gov v1.5.0 loaded (legacy=1 profile=1 freq_init=deferred)
[10.961816] Build341: build hash verified (full-synergy) retries=0

/proc/modules            → k6a_gov 32768 0 - Live
/data/adb/k6a_gov.ko     → 44560 B, delivered by anykernel.sh
k6a_features version     → 265983        (revert confirmed: no "version delta" warning)
sepolicy AV kernel→sysfs:file → 40012    (rule applied by ksud at boot)
k6a-ctl                  → version=1.3.1 versionCode=131
```

Failure modes and what they mean: no `k6a_gov.ko` file → the flash copy failed (check the
`ui_print` lines); `insmod ... rc=1` with `Exec format error` → vermagic/`.ko` is from another
build; `insmod ... Permission denied` → SELinux for `module_load` (add
`allow ksu self:system module_load`); nothing at all in `service.log` → k6a-ctl is not
installed; `hash_state=3` → the `sepolicy.rule` of finding 3 is missing. In every one of those
cases k6a-ctl falls back to its own CD_L2/L3/L4 state machine, so thermal protection stays
active — it is a degradation, never a brick.

> **`su` in Termux**: the `su` on the PATH is Termux's stub ("No su program found").
> Always use the absolute path **`/system/bin/su -c '…'`**, and absolute paths inside the
> command — the root shell's PATH is broken (only the `echo` builtin is usable bare).

## KernelSU-Next SUSFS
- SUSFS in `fs/susfs.c`, `include/linux/susfs.h`
- Hooks in `fs/stat.c` (`CONFIG_KSU_SUSFS_SUS_KSTAT`) and `kernel/sys.c` (`CONFIG_KSU_SUSFS_SPOOF_UNAME`)
- Submodule `KernelSU-Next` @ `b100bd28` (`v3.3.0-95-gb100bd28`, runtime `KSU_VERSION=38309`, UAPIv4)
- `git submodule update --init --recursive` required for fresh clone

## CODM-Fps-Messmethodik (r24–r40, gemessen 2026-10-11/12)

Ziel ist **„gesicherte 60 fps"** — entscheidend sind die Täler, nicht der Mittelwert.
Auswertung: `~/tmp/cmp6.py` (Varianz), `cmp7.py` (Regression), `cmp9.py` (Tal-Analyse).
Alle Läufe auf `build348`, gleiche Karte, Lüfter aus, Sampler `fps_samp2.sh`,
Filter `t0+120 s` (`SKIP=120`, **keine** fps-Schwelle).

### Varianz der laufgleichen Messung — 4× identisch

| Run | AkkuTemp | Ø fps | Täler <40 fps | tp-Start |
|---|---|---|---|---|
| r33 | 21,9 °C | **65,8** | 0 | 53 °C |
| r35 | 26,1 °C | **61,8** | 1 | 54 °C |
| r36 | 27,5 °C | **62,3** | 3 | 52 °C |
| r37 | 29,4 °C | **60,3** | 8 | 56 °C |

Ø 62,54 · **s = 2,36 fps** · Spannweite 5,6 fps · 95 %-Band 57,9…67,2

### Haupttreiber: Akkutemperatur (nicht Streuung)

```
fps = 81,22 − 0,71 · AkkuTemp_C        r = −0,96   r² = 0,926
```

**−0,71 fps pro Grad Akkutemperatur.** Nach Temperatur-Korrektur sinkt die
Streuung von **s = 2,36 auf s = 0,64 fps** (3,7×). Tälerzahl steigt
monoton mit der Starttemperatur (0→1→3→8).

**Messbarkeitsgrenze (80 % Power, α = 0,05, zwei Gruppen):**

| Aufbau | n=4 | n=6 |
|---|---|---|
| ohne Starttemperatur-Kontrolle | **4,7 fps** | 3,8 fps |
| mit Starttemperatur-Kontrolle | **1,3 fps** | 1,0 fps |

### DDR-Overclock: kein Effekt nachweisbar

`r34` (min_freq 6881 erzwungen, 264/264 Samples) = **60,1 fps** → −2,4 fps zum
Kontrollmittel = **1,0 σ → innerhalb der Varianz**. Fairster Paarvergleich bei
gleicher Temperatur: r34 (26,0 °C) 60,1 vs. r35 (26,1 °C) 61,8 → **−1,66 fps**,
unterhalb der Nachweisgrenze. DDR lag ohnehin bei 89–96 % am Deckel.

**Ableitung:** ohne Starttemperatur-Kontrolle ist die Methode **unfähig, DDR-OC
zu bewerten** — sie löst nur Effekte ≥ ~5 fps auf. Vor jeder weiteren
A/B-Aussage muss die Starttemperatur fixiert werden.

### Was die Täler sind — und was nicht

Tal-Fenster (10 s, <45 fps, n=19) gegen Hoch-Fenster (n=44):

| Größe | Tal | Hoch | Befund |
|---|---|---|---|
| `th` (Throttle-Ticks) | 0 | 0 | **kein CPU-Throttle** |
| `ql` (cpu_limits) | leer | leer | **Thermal-Framework greift nicht** |
| `p6` / `pm` | 1708800 | 1708800 | Kappe unverändert |
| `f6` | 1 632 000 (min) | 1 698 327 | Kappe fast durchgehend gehalten |
| `gb` (GPU busy) | 50 % | 71 % | **GPU unterfordert** |
| `gc` (GPU-Takt) | 360 MHz | 497 MHz | GPU-Governor rampet nicht hoch |
| `psi` (/proc/pressure/cpu) | 37,6 | 29,9 | **+26 % CPU-Druck** |

Die GPU bekommt keinen Feed — der Stall liegt **vor** der GPU, bei
CPU-Queue/Scheduler oder seitenständig im Spiel. Tiefster Einzelwert
r36 510 s = **7,2 fps** bei `gb=52 %`, `gpuclk=355 MHz`.

⚠️ **`qx` ist kumulativ** (`utime+stime` des `app.shanwan.gamepad`-Pinnern)
und damit reines Zeitartefakt — Täler liegen später im Lauf. **Nicht
auswerten.**

### Verbindliche Methode ab dem nächsten Lauf

1. **Ausschließlich Discharging.** Kabel ziehen, dann starten. r43 (Charging
   11 %) 58,6 fps gegen r44 (Discharging) 79,9 fps bei sonst identischen
   Settings — das ist der größte Konfound, kein Merkposten.
2. **Kein Starttemperatur-Gate.** Das Gate senkt die Streuung nicht (s = 2,34
   gemanagt vs 2,36 frei, r33/r39/r40) und kostet nur Wartezeit. Start-`bt`
   und `zone26` protokollieren, fertig.
3. **Auswertung über `validate.py`:** `med180` (Median Fenster rel. 180–570 s,
   v>0-Filter gegen Surface-Resets) + `p60` (%≥60) + Tälerzahl. Das
   180–570-Fenster schlitzt die frühen Drop-in-Täler (120–160 s) ab, die
   r41/r42/r43/r45 zeigen und r44 nie — Roh-Ø vergleicht Match-Fußabdruck,
   nicht Behandlung.
4. **Nur Rundenfenster 0–600 s** der io-Spur auswerten; danach verfälscht die
   eigene Analyselast (Termux läuft auf dem Gerät) die Thermik.
5. **≥ 3 Läufe pro Bedingung**, nie Einzelpaare — selbst die Robustmetrik
   streute r44↔r45 um 5,2 fps bei identischen Settings. Effekte unter ~5 fps
   brauchen mehr Läufe oder konsistente Block-/Tälermuster.

### Gated Läufe (r39, r40) — das Gate hilft NICHT

Mit `arm.sh` (Gate: AkkuTemp ≤ 23,5 °C **und** zone26 ≤ 52 °C, danach sofortiger
Spielstart) entstanden drei **saubere** Kontrollläufe gleicher Behandlung
(`nice-10+trips93-96-99-90+NOFAN+p6cap1708`, build348, gleiche Karte, Lüfter aus):

| Run | bt-Start | Ø fps | med | Täler | n |
|---|---|---|---|---|---|
| r33 | 21,9 °C | **65,84** | 62,0 | **0** | 207 |
| r39 | 23,6 °C | 62,56 | 58,3 | 2 | 177 |
| r40 | 23,0 °C | 61,31 | 59,0 | **6** | 179 |

Ø 63,24 · **s = 2,34 fps** · Spannweite 4,53

**s = 2,34 ist praktisch identisch mit dem unkontrollierten Wert (2,36).** Das
Temperatur-Gate senkt die Lauf-zu-Lauf-Streuung **nicht**; Akkutemperatur war nie
der Haupttreiber. Daraus folgt: **MDE ≈ 4,6 fps** (n=4/4 gepaart) — jede
Mikro-Optimierung (Pinner, `cdt`, Taktkurven, DDR) liegt unterhalb der
Nachweisgrenze und ist mit diesem Aufwand **nicht beweisbar**.

Das Rauschen ist zudem **nicht** eine gleichmäßige Absenkung, sondern
**sporadische Stalls**: r33/r39/r40 hatten 0/2/6 Täler, ohne Korrelation zur
Starttemperatur (23,0 °C → 6 Täler, 23,6 °C → 2).

### Stall-Ursache: spielseitig — CD_L2 war ein Nachrunden-Artefakt (r41–r45, 2026-10-17)

Die Live-Auslesung nach r40 (`state=cd_l2`, `batt_latched=1`, `hist`-Alter 728 s) war
**kein Runden-Zustand**, sondern zeigt den Nachrunden-Lade- und Befehlszustand: Termux
läuft **auf dem Gerät**, unsere eigene Auswertungs-Last hebt `zone26` nach der Runde auf
~94 °C. Gemessen in den Runden selbst (rel. 0–600 s) gilt — `k6a_gov` bleibt **in allen
fünf Läufen zu 100 % in `GAMING`**:

| Run | Bedingung | k6a-Zustand (Runde) | fps Ø | Täler |
|---|---|---|---|---|
| r41 | Kappe, kein Lüfter, Discharging | **gaming 133/133** | 64,2 | 13 |
| r42 | Kappe, Lüfter, Discharging | **gaming 152/152** | 60,6 | 8 |
| r43 | Volle Freq., Lüfter, **Charging 11 %** | **gaming 290/290** | 58,6 | 29 |
| r44 | Volle Freq., Lüfter, Discharging | **gaming 354/354** | **79,9** | **0** |
| r45 | Volle Freq., Lüfter, Discharging | **gaming 416/416** | 59,7 (63,0 bereinigt) | 35 |

Das einzige `cd_l2`-Ereignis der Serie (r45, rel. 638–889 s) liegt **komplett nach dem
600-s-Fenster**. **Regel: nur das Runden-Fenster 0–600 s auswerten.**

Drei Bedingungsvergleiche mit jeweils genau einer geänderten Variable:

| Vergleich | ΔØ fps | Befund |
|---|---|---|
| Charging → Discharging (r43→r44) | **+21,3** | Einzelpaar, eindeutige Richtung: **Laden während der Runde ist der größte Konfound.** |
| kein Lüfter → Lüfter (r41→r42) | −3,7 (−5 Täler) | **Lüfter-Hypothese tot.** SoC-Temp praktisch identisch (78,2/83 vs 79,5/85 °C) — der Lüfter kühlt nur den Akku (−6,7 °C), der nie engpass war. r32 (67,5/0) war ein Glückslauf. |
| Kappe 1708800 → 2304000 (r42→r44/r45) | r44 +19,4 · r45 −0,8 | **unbestätigt.** r44 ist der Ausreißer (0 Täler, 91 % ≥60); der Kontrolllauf r45 unter **identischen** Settings liefert das gekappte Niveau (59,7). **Entscheidung: Kappe bleibt 1708800.** |

Wo die Kappe real wirkt: **Temperatur und Schwellenabstand** —
ktemp Ø 74,9–79,5 °C gekappt vs 81,2–86,7 °C ungekappt (max 94 °C in r45), also
7–12 K Abstand zur 93 °C-Schwelle gespart, ohne messbaren Median-Gewinn
(med 180–570: 64,4/55,0 gekappt (r41/r42) vs 65,2/75,7/69,2 ungekappt
(r43/r44/r45) — innerhalb der Lauf-zu-Lauf-Streuung).

**Täler-Anatomie (bleibt: spielseitig).** Die GPU ist in den Stalls **nicht idled**,
sondern bei 50–75 % busy — sie bekommt zu wenig Arbeit. r45s 34 Täler entfallen ohne
jede Korrelation zu `cd_l2` (0/34 innerhalb ±3 s). Die frühen Täler bei 120–160 s
(Drop-in) tauchen in r41/r42/r43/r45 auf, in r44 nie — Match-Fußabdruck statt
Behandlungseffekt. k6a braucht für CD_L2 t≥93 °C **durchgehend 3000 ms** (`entry_since`
wird bei jedem Unterschreiten zurückgesetzt) — bei 24 erneuten ≥93-Beobachtungen in
r45 reichte das nie: beabsichtigte Anti-Thrash-Hysterese, kein Bug.

### Mess-Werkzeuge und ihre Fallen (r40–r45)

`io_samp.sh` zeichnet PSI, Block-I/O-Deltas, `pgmajfault`, GPU-Busy/-Takt,
DDR-Bandbreite **und den k6a_gov-Status pro Iteration** mit
(`kstate ktemp kpeak kpm kth kbl ksage`, 18 Spalten). Für r41–r45 lief damit die
Zuordnung Stall ↔ Zustand direkt.

⚠️ Drei Fallen, erst deutlich an r44/r45 gemessen:

1. **Kadenz ist NICHT 1 s.** Der Status-Read (`timeout 2` + restliche Pipeline)
   macht pro Iteration real **1,4–2,1 s** — die `t`-Spalte ist Timestamp, keine
   Linien-Nummer. Deltas per Nearest-Match zuordnen, nicht per Zeilenindex.
2. **Frame-Zähler kann zurückgesetzt werden.** r43 meldet `min 0,0 fps`, r45
   `min −530,8 fps` — in beiden Fällen wurde die SurfaceView des Spiels neu
   erstellt (Spiel-/Surface-Recreation). **Alle `v∉(0,∞)`-Intervalle vor Auswerten
   verwerfen** (macht `validate.py` seit dem selbst).
3. **Nachrunden-Samples sind vergiftet.** Die letzte ~1/3 der io-Spur kann
   Nachrunden-Werte enthalten (Analyselast). **Nur rel. 0–600 s bewerten.**

## Build 350 Scope (Hygiene, 2026-10-12)

Bewusst **kein** funktionaler Kernel-Eingriff: es gibt für die Stalls keinen
belegbaren Config-Defekt (siehe Stall-Ursache oben). Scope:

| Änderung | Datei | Begründung |
|---|---|---|
| Tote `thermal_floor`-Sysfs entfernt (105 Z.) | `drivers/gpu/msm/kgsl_pwrctrl.c` | Schnittstelle `/sys/kernel/k6a_gpu_thermal/thermal_floor/{temp_low,temp_high,levels}` hatte **0 Konsumenten** im Tree — Schreiben dorthin tat nichts. Tuner-Falle. Der echte GPU-Cap-Pfad ist ausschließlich `kgsl_k6a_set_max_level_idx`. Bonus: entfernt einen vorbestehenden clang-only Fehler (`device_initcall`). |
| `CONFIG_TASK_XACCT=n`, `CONFIG_TASK_IO_ACCOUNTING=n` | `sweet_defconfig` | Rest-Accounting nach build349 (`TASK_DELAY_ACCT` aus). `/proc/uid_stat` ist auf modernem Android tot; `/proc/pid/io` (BatteryStats-IO) geht verloren. `TASKSTATS` bleibt an. Nichts `select`et die Optionen. |
| `CONFIG_KSM=n` | `sweet_defconfig` | `ksmd` scannt dauerhaft (`KSM_RUN_MERGE` ist 4.14-Default) — Hintergrund-CPU ohne Messgewinn. Nichts `select`et/`depends on` KSM; Verbraucher sind `#ifdef`-geschützt. |

Nicht angefasst (bewusst): `HZ_1000`, `PREEMPT`, `NO_HZ_IDLE`, `PSI`,
`KPROBES`, IO-Scheduler/`SCSI_MQ_DEFAULT`/`BFQ`/`BLK_WBT`, `MEMCG`, `THP`,
Governor-Wechsel, ZRAM-Writeback, `IKCONFIG_PROC` (SUSFS-Ziel), `PERF_EVENTS`,
und `cd_l2_gpu_max` (kein belegter Nutzen, siehe oben).

Syntax-Check (pristine-verglichen, clang 21.1.8): Pristine 1 Warnung + 1 Fehler
(`device_initcall`), geändert 1 Warnung (identisch, Zeile 1970) + **0 Fehler**.
Nach `sweet_defconfig` + `olddefconfig` bleiben alle drei Optionen draußen;
  `PSI=y K6A_GOV=m KSU=y TASKSTATS=y` unverändert.

## Build 351 Scope (bw_apply-Bugfix, 2026-10-17)

Externer Code-Review (§1–§4) wurde vollständig verifiziert; umgesetzt nur der
einzige belegte funktionale Defekt — ein Einzeiler:

| Änderung | Datei | Begründung |
|---|---|---|
| `bw_apply`: `if (!k6a_devfreq_get_bw(...) && mn != want)` → `if (k6a_devfreq_get_bw(...) \|\| mn != want)` | `drivers/thermal/k6a_gov/k6a_gov.c` | Vorher wurde die Korrektur **übersprungen, wenn der Read-Back fehlschlug** — exakt in der Situation, in der der tatsächliche Wert vom Soll abweichen kann. Mit `\|\|` wird bei Read-Fehler **oder** abweichendem Wert jederzeit re-applied. Marker `Build351:` auf den gezeigten Zeilen; `K6A_GOV_VERSION` bleibt 1.5.0. |

Nicht umgesetzt (bewusst, mit Begründung im Review-Diskurs):
- CI-Hygiene (UAPIv2-Doppel-Trigger, `LOCALVERSION`-sed, Trigger-Branches) —
  verifiziert, aber ohne Kernel-Funktion; als nachgelagerter
  P1-Hygiene-Punkt notiert.
- `thresholds_user`-Persistenz (§2, Reset bei `insmod`) — Designfrage,
  separat zu entwerfen; `k6a-ctl` bleibt bis dahin der Setter.
- `CONFIG_ENERGY_MODEL` (§5) — ohne Beleg, dass eine nicht existierende
  Energie-zu-Takt-Zuordnung die Stalls ändert; bleibt offen.

Auswertungswerkzeug (nicht Repo-Inhalt): `~/tmp/validate.py` implementiert die
Robustmetrik aus „Verbindliche Methode" oben — `med180` + `p60`, v>0-Filter,
Fenster 180–570 s; verifiziert an r29–r45.

Syntax-Check (pristine-verglichen, clang 21.1.8, beide Dateien mit
`-DK6A_GIT_HASH/-DK6A_BUILD_HASH` versorgt — ohne diesen Define bricht der
Header mit `#error`): **beide rc=0, 0 Fehler, 0 Warnungen.**

## Build 352 Scope (GAMING-Schwellen 96/99/102, CI-Hygiene, 2026-10-18)

Vorgeschichte (r46–r54, alle `build351`, NOFAN, Discharging, eine Runde
pro Messung über `arm.sh`): Re-Baseline gekappt r46/r47/r48 → med180
62,5/58,3/63,0 (Ø 61,3, s≈2,47 ≙ 348er-Cluster 2,25) = **build351
verhaltensidentisch zu build348**. Block I Kappe AUS (stock thresholds
93/96/99/90) r49/r50/r51 (UC-A/B/C) → med180 60,5/66,5/62,0 (Ø 63,0),
SoC max **95 °C** in allen drei, k6a eskaliert selbst in `cd_l2`
(20–39 % der Samples, p6 fällt auf 1708800) — „Kappe aus" ist in vivo
nur halb aus, der Governor reimplementiert die Kappe über die Schwellen.
Block II: ein `sysfs`-Write `echo "96 99 102 93 1708800 1555200
1324800" > /sys/kernel/k6a_gov/cd_thresholds` (Kappe bleibt AUS),
r52/r53/r54 (TH-A/B/C) → med180 **70,2/74,5/77,0** (Ø 73,9), Täler **0**,
k6a **100 % gaming**, p6 **100 % auf 2304000**, tp max identische 95 °C.
Der FPS-Gewinn (+10 bis +13 med180, konsistent in 3 Läufen) kommt
ausschließlich daher, dass die Runde `gaming` nie verlässt — die alte
93-°C-Schwelle (l2) war die eigentliche Bremse, nicht die p6cap-Kappe.
r44 (75,7) ist damit entschlüsselt: erster „TH-artiger" Lauf, in dem das
CD-Fenster nie antrat.

| Änderung | Datei | Begründung |
|---|---|---|
| GAMING-Profil `cd_l2_temp,cd_l3_temp,cd_l4_temp,cd_recover` `83,86,90,78` → **`96,99,102,93`** | `drivers/thermal/k6a_gov/k6a_gov.c` (profiles[]) | Bloßes heben der l2-Schwelle von 93 auf 96 hält k6a die ganze Runde in `gaming` (r52–r54: Krone aus 3× 100 % `gaming`, 0 Täler). Gold-Caps der l-Schwellen (1708800/…) unverändert — sie werden nun schlicht nie scharf. `K6A_GOV_VERSION` 1.5.0 → **1.5.1**. |
| Trigger-Branches `[main, 23.2, ksu-next-fix-100]` → `[main]` | `.github/workflows/build-kernel.yml` | `23.2`/`ksu-next-fix-100` sind tote Test-Branches; Doppel-Builds/Release-Rennen. |
| `LOCALVERSION`-sed robuster: `sed -E 's/^CONFIG_LOCALVERSION=.*/…buildNNN"/'` + `grep -qF buildNNN`-Verifizierung (fehl → `::error::`) | `.github/workflows/build-kernel.yml` | Vorher schlugen beide sed-Steps stumm fehl (`\|\| true`), wenn `olddefconfig` die Zeile schon verändert hatte; Verify erst durch Hint-`grep`. |
| `main.yml` (Legacy-Telegram-Build) mit `if: github.ref == 'refs/heads/main'` versehen | `.github/workflows/main.yml` | Jobs gegen Arbeits-Branches feuern sonst bei jedem Push (UAPIv2-Doppeltrigger-Befund Z246/264). |

k6a-ctl bleibt Persistenz-Layer: `/data/adb/modules/k6a-ctl/config/
settings.conf` schreibt `cd_thresholds=…` beim Boot — bisher `93 96 99
90…`. Die new GAMING-Defaults greifen nur, wenn der ctl-Write entfällt
oder auf die selben Werte gesetzt wird; **ctl-Config wird separat
angereicht** (auf Gerät: `96 99 102 93 1708800 1555200 1324800`).

Nicht umgesetzt: `thresholds_user`-Persistenz im Kernel (Designfrage,
k6a-ctl bleibt der Setter), `CONFIG_ENERGY_MODEL` (unbelegt), CI-
Doppeltrigger der UAPIv2-Sektion (siehe oben, nun mit `main`-Gate).

Syntax-Check (pristine-verglichen, clang 21.1.8, beide Dateien mit
`-DK6A_GIT_HASH/-DK6A_BUILD_HASH` versorgt): **beide rc=0, 0 Fehler,
0 Warnungen.**

## Build 353 Scope (Exit-BW-Floor-Fix, cool_set-Clamp, 2026-10-18)

Anlass: externe Strukturanalyse von `main @ 8adaa6a0f` (Build 352) meldete
4 Befunde; 3 verifiziert, 1 veraltet.

| Befund der Analyse | Verifizierung | Status |
|---|---|---|
| 1. `k6a_gov_exit` gibt BW-Floors nicht frei (`k6a_gov.c:~1370` — `kgsl_k6a_set_max_level_idx(0)` + `apply_policy()` ja, aber kein `reset_bw_floors()`; auf rmmod (Version-Lock-Pfad von k6a-ctl) bleiben `gpubw`/`cpu-llcc-ddr-bw` gelatcht) | **bestätigt** — gleicher Bug-Typ wie v1.3.1 „Cap wird nie freigegeben", nur Exit-Achse | **Fix umgesetzt:** `reset_bw_floors();` vor `cancel_delayed_work_sync()` |
| 2. k6a-ctl schreibt alte Schwellen `93 96 99 90` beim Boot und überschreibt die neuen Kernel-Defaults | **veraltet** — seit k6a-ctl v1.4.1 (`0d6dece`) ist `settings.conf` auf `96 99 102 93 …` und der Boot-Write idempotent; auf Gerät live verifiziert (`cd_thresholds=96 99 102 93`, `thresholds_source=user`, einziger Boot-Write in service.log) | keiner |
| 3. `cool_set` akzeptiert Zustand 0 (OFF) — `k6a_gov.c:804` clamp nur Obergrenze (`s > K6A_CD_L4`); ein Thermal-Core-Write von 0 hat keinen legitimen Absender (k6a-ctl nutzt `enable=0`, der einzige OFF-Pfad mit voller Freigabe), würde aber Schutz stilllegen | **bestätigt** — `enum k6a_state { K6A_OFF=0, … }`, kein legitimer `cool=0`-Schreiber im ctl | **Fix umgesetzt:** `if (s < K6A_GAMING) s = K6A_GAMING;` — OFF nur noch via `enable=0` |
| 4. Kleinigkeiten (sprintf Tätigkeit in status_show ohne PAGE_SIZE-Check, l2t-rec-Deadband-Warnung feuert nur auf sysfs-Pfad, `poll_ms`-Read in msleep ohne Lock) | notiert, alles benign/kosmetisch; das 3-K-Band (96−93) ist die AGENTS.md-dokumentierte bewusste Abweichung | keine Aktion |

Änderungen: `k6a_gov_exit` + `cool_set` (jeweils 1 Zeile, `Build353:`-Marker
im Kommentar), `K6A_GOV_VERSION` **1.5.1 → 1.5.2** (k6a-ctl `kver_ok`
akzeptiert `1.5.*`, Gate bleibt grün; distinguierbar für Volume-Lock).

Syntax-Check (clang 21.1.8, Rezept aus Local Build Notes,
`-DK6A_GIT_HASH/-DK6A_BUILD_HASH`): **rc=0, 0 Fehler, 0 Warnungen.**

## Post-352-Messsituation (Thermik, Deckel, FAN — r50–r59, 2026-10-18)

Alle Runden: gleiche Karte, Discharging, `arm.sh`-Gate (1560-s-Fenster, med180
via `validate.py`, Minute-Buckets + late-med 600–Ende separat; build351/352-Kernel,
Schwellen wie angegeben, 1 Runde = 1 Messung).

| Runde | Bedingung | med180 | late-med | bt max | k6a / p6-max |
|---|---|---|---|---|---|
| r50/r51 | uncapped, stock-Schwellen | 66,5 / 62,0 | — | — | k6a ctx: 20–39 % `cd_l2` |
| r52–r54 | uncapped, **96/99/102/93** | 70,2 / 74,5 / 77,0 (Ø 73,9) | — | — | **100 % gaming**, p6 100 % @2304 |
| r55 | „K-A" (Kappe vermutet) | 65,2 | — | — | **eigl. uncapped** — Sampler-uncap-Write (hp_stop `echo 2304000 > p6/scaling_max_freq`) hebt die ctl-Kappe lautlos auf |
| r56 | **FAN**, uncapped | **82,5 (Best)** | **69,6** | 32,3 °C | 100 % gaming |
| r57 | **NoFan**, uncapped | 72,0 | **45,6** | 41,7 °C | 100 % gaming; **Kollaps min 11→**: f6 2,30→2,25 GHz, bt>38 °C |
| r58 | NoFan, Trip95→**98 °C** | 63,8 ↓↓ | 38,6 ↓ | **45,2 °C ↑** | 17 % `cd_l2` (p6@1708800) — **TripWrite blieb 98 stehen, Pinning trotzdem ≤95,7** |
| r59b/r59c | **FAN, Gold-Cap 1708800** (capped-Sampler) | **fehlt** (SF `frame=` friert ein — `lay`-Counter statisch; sf_probe v1 bricht nach ~5 s ab) | — | 28,7 / 30,1 °C | **p6=1708800 in 412/412** — Cap end-to-end für erste Zeit schwarz auf weiß |

Befunde aus dieser Serie:
1. **Die 96-°C-l2-Schwelle ist der FPS-Hebel** (r52–r54 vs r50/r51), nicht die Gold-Kappe.
2. **Lüfter = die NoFan-Alternative**: SoC-CPU-Droop ab bt≈38 °C ist der Kollaps-Mechanismus
   (nicht k6a-CD, nicht Trip-Pinning); TRIP98 macht es schlimmer (Akkuhebel). Trips bleiben
   auf 95 — Daemon-Pinning ist **nicht per sysfs-Trip schraubbar** (Write bleibt, Pin bleibt).
3. **Gold-Cap end-to-end klappt seit v1.4.3/capped-Sampler** (r59b: 100 % @1708800, dt.
   Cap-Enforcement bestätigt); med180-Zahl der FAN+Cap-Zelle fehlt → **r59d** (gleiche
   Bedingung, `sf_probe.sh` **v2** pro-Snapshot-layer-fresh) ist noch offen, um den
   Gold-Cap-Vergleich mit Zahlen (u. a. r56 82,5 UNCAPPED) abzuschließen.
4. Instrumente: `fps.log`-`lay`-Frame-Counter friert seit >r59a ein (SF `--latency` /
   BLAST) — **fps-Quelle ist `sf_probe.log`** (Timestamps). `sflat_all`-Fragmente ≈60 fps
  -Avg über 306 frames (r59b), med180≈60 – **indikativ, nicht normierbar** (n-per-Window-
   Festival). Die Datensätze r59a (abgebrochen/truncated, „Runde nie geführt") und r59b/c
   bleiben ohne med180-Zahl — Messung gilt als **nicht referenzierbar**.

Messmethode-Änderungen:
- `fps_samp2.sh` (Gerät) = capped-Variante; `fps_samp2_uncap.bak` = Original. Sobald ein
  Gold-Cap-uncapped-R Durchlauf gewünscht ist, Rückkehr über `cp fps_samp2_uncap.bak
  fps_samp2.sh` **vor** dem Arm — **nie während des Samplings**.
- `sf_probe.sh` v2 (Gerät): `fg=COD` → pro Snapshot Layername frisch bestimmen + `--latency
  <LM>` mit `lmlen/rc/rows`-Markern → robust gegen zerstörte SurfaceView-Layernamen.
- **NIEMALS** `sh fps_samp2.sh …` manuell aufrufen („stop" = `DUR`-Argument!): trunciert
  `fps.log` (r59a-Befund). Stop via `kill -INT <pid>` (Trap → `hp_cont` sauber), im Zweifel
  `arm.sh`-Neustart, oder `pgrep -f fps_samp2 && kill -INT`.

## Build 394 Scope (SUSFS v2.3.0 kernel-core sync, 2026-10-23)

Upstruktur-Aufnahme: susfs v2.3.0 lebt upstream (sidex15/pershoot/WildKernels, `kernel_patches/v2.3.0`
und KernelSU-Next `legacy-susfs-v2`); KernelSU-Next 72f8cc62 trägt die SUS-Funktionen
**im Submodul** (sus_mount/sus_path/susfs_kstat/susfs_opath/btf_layout/lsm_hook/susfs_main = LKM-
Einstieg mit „refuse loading if in-built SuSFS is present"). Unser Kernel-Tree ist die
Thin-Core/Variante (`fs/susfs/`-Split: core/features/kstat/mount/open_redirect/path/spoof,
494 Z.) + Hooks nur in fs/stat.c (`susfs_sus_kstat_spoof_generic_fillattr`), fs/namei.c
(SUS_PATH ×2), kernel/sys.c (uname), selinux/avc.c (avc-log). Der Bump ist deshalb
**ABI-/Version-Sync, kein Feature-Portat**:

| Änderung | Datei | Grund |
|---|---|---|
| `SUSFS_VERSION "v2.2.0" → "v2.3.0"` | `include/linux/susfs.h:13` | Version-String-Konsistenz mit Submodul-ABI (supercall meldet v2.3.0 aus `susfs_abi.h:62`) |
| `DEFAULT_KSU_MNT_MINOR_DEV (1<<12)`, `STATX_SUS_KSTAT*` Bits, `susfs_is_current_app_uid()` | `include/linux/susfs_def.h` | upstream-True-Bit: Constants, die v2.3.0 ABI definieren; in Thin-Tree inert (keine super/world/state-Hooks), aber def.h #kein ABI-Drift |
| `strcat` → gebundenes `strlcat`-Makro (SUSFS_APPEND_FEATURE) | `fs/susfs/susfs_features.c` | defensiv (8192-Puffer macht Overflow theoretisch unmöglich, aber verbiete Concat-Umbruch von v2.3.0-Muster) |
| `Build394: susfs core v%s synced` pr_info | `fs/susfs/susfs_core.c:susfs_init` | post-flash dmesg-Beweis für die Version |
| `.v210`-Leichen gelöscht (fs/susfs.c.v210 + 2 Header) | verworfene v2.1.0-Single-File-Backups | dead weight |

Bewusst NICHT portiert: vfs_statfs/fdinfo/proc-fd-Spoof-Kanäle des 2.3-Refactors
(61e9e48) und `fs/super.c`-MinorDev-Gap-Fix — die knallen an unsere Architektur
(SUS-MOUNT = Submodul-Aufgabe, nicht Kernel-Blob); `result_mask`-Parameter des
Upstream-Fills ist im 4.14-`generic_fillattr`-Site bedeutungslos (kein statx-Mask-Filter).
SUSFS-Konfig (defconfig §5229–5237) unverändert.

Syntax-Check (clang 21.1.8, Kern-Recipe aus Local Build Notes ohne `__ASSEMBLY__`):
fs/susfs/susfs_core.c + susfs_features.c → beide rc=0, nur die (kern-wide bekannte)
GNU-Variable-Sized-Warning aus `cgroup-defs.h`.

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
