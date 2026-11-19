# USB-Status + Fixed-Bugs (README-Archiv)

> Ausgelagert aus `README.md` beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> Fruehere README-Sektionen 'USB status' (Faults 1-4) und 'Fixed Bugs (v1.3.2)'.

## USB status

### Charger SNK-only tradeoff (Build 3xx, in-tree)

The Type-C role switch fix toggles **EN_SNK_ONLY_BIT** (Type-C off, 50 ms, on — current/voltage limits untouched). Side effect by design: **the device can no longer supply power over USB — USB-OTG devices and reverse charging are disabled.**

### Debug breadrumbs rallied (Build 332 heartbeat, cleanup 2026-10-31)

The `b332_hb_fn` heartbeat was a permanent 500 ms non-deferrable timer with one log line per tick (~170k wakeups/day; dmesg buffer overwritten in ~2 h). Now **off by default** and deferrable; hot-path `dev_err/pr_info` breadcrumbs are `dev_dbg/pr_debug` (compiled out with `CONFIG_DYNAMIC_DEBUG` off). Opt-in back temporarily on the cmdline: **`dwc3.b332_hb_ms=500`**. Idle target after flash: well below a few hundred dmesg lines/hour (`dmesg | wc -l` over half an hour must not grow).

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

Faults 1–3 detail and the full evidence tables live in `docs/devlog/usb-debugging.md`.

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

## Fixed Bugs (v1.3.2)

| Bug | Symptom | Root Cause | Fix |
|-----|---------|------------|-----|
| **USB PC Bootloop** | PC host → instant reboot/bootloop, 67W charger OK, `No data transfer` didn't help, `bq2597x` init cut at 0.73s, stock boot.img OK | DWC3 `WAIT_FOR_LPM` deadlock + EP0 `TRB timeout` (`bus_aggr`/`noc_aggr` clocks off, `GCTL RAMCLKSEL` on `rev 00000000` wrong, `is_a_peripheral=0`) → WDT bite | SM6150 clocks (`GCC/DISPCC/CAMCC/SCC`), `WAIT_FOR_LPM` clear on extcon, `bus_aggr` enable + IOMMU guard, `GCTL` 50ms delay, `is_a_peripheral` `pullup`/`vbus_connect` — verified S21 + PC `IRQ>0` `mtp,adb` |
| **USB attach → whole-SoC reset** (Fault 4) | USB-C into a Galaxy S21 → 5 s vibration + reboot, log empty, `/sys/fs/pstore` empty every time | `ffs_func_eps_enable()` used the local `struct ffs_data *ffs` in an `ffs_log()` **before** `ffs = func->ffs;` → `ipc_log_string(ffs->ipc_log, …)` dereferences an uninitialised pointer (`+0x1a8`, x0=0) → oops kills `dwc_wq` → `panic=5` → PS_HOLD `'cold' boot` wipes ramoops | **Build 336**: assign `ffs` before the `ffs_log`; **Build 335** breadcrumbs proved it. Verified: `USB_STATE=CONFIGURED`, UDC `configured`, zero oops |
| **Identical Release Names** | All GitHub Releases/ZIPs named identically (`v4.14.369`) | `build-kernel.yml` `name: "Badazz-kernel v${VERSION}"` + ZIP `v${VERSION}.zip` without `run_number` | CI now `name: "v${VERSION}-badazz-build${run_number}"` + ZIP `v${VERSION}-build${run_number}[ -miui].zip` + artifact `…-buildXX-variant` |
| **KSU Manager "update required"** | Manager CI `34252913097` (`88feb68` `e801e16` version matching) on kernel `a5ff54c` → red banner | `e801e16` new UAPI + `bundled_lkm` check, old kernel UAPI mismatch; `88feb68` needs `KPROBES` + 5.x APIs (`syscall_fn_t`, `pgtable.h`, `lsm_hook`) not 4.14 compatible | Fork `dev` → `88feb68` for PC, hotfix `CONFIG_KPROBES=y` + `syscall_fn_t`/`pgtable`/`ksys_close` guards, then **revert** to the current line (`b100bd28`, UAPIv4) until a proper 4.14 port exists. **Resolved 2026-10-02**: manager + ksud `v3.4.0-19-g2b31f718` (CI `36750739326`, uapi 4) run against our kernel `GET_INFO version=38309 ≥ 34634` with no red banner — the old `34142634591` pin is obsolete |
| **MIUI/HOS not booting** | Stock HyperOS `V14.0.1.0` `4.14.190-perf` needs `ARCH_SM6150/CAMCC/GCC/PDC` + `xiaomi/sweet` DTS (`GTX9896_K6`, `FPC1540`) | `sweet_defconfig` flattened to `atoll/sm6150`, `QUSB2` only | `sweet_miui.config` overlay (7 lines) + `xiaomi/sweet` DTS shim + CI matrix `aosp`/`miui` (`miui/test` artifacts, `main` release `…-miui.zip`) |

