# k6a gov history (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> k6a_gov v1.6.0-Hardening + Build-341-scope (K6A_GOV=m, insmod-Proof)

## k6a_gov v1.6.0

> **Update 2026-10-27 (Build 397/398):** Fixes 1-10 sind in
> `build397`/`build398` gelandet (Details in *Current State*). Die Abschnitte unten
> stammen aus der v1.5.0-aeRa und bleiben als Feature-/Debug-Doku gueltig (Nodes,
> Anker, Marker-Logik unveraendert).

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

