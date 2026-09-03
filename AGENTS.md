# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/BadazzKernel` branch `main`
- **Kernel**: `4.14.369` — `K6A_GOV v1.3.1` built-in
- **Local**: Clean, GH `main` @ `81d69ae` (v1.3.1 + deadlock fix)
- **GitHub**: Up to date, CI builds on `main`

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
- `CONFIG_KSU=y` `33300` UAPIv2, `CONFIG_KSU_SUSFS=y` + all sub-options + `TAMPER_SYSCALL_TABLE`
- `CONFIG_SCHED_TUNE=y` `CONFIG_KSM=y` `CONFIG_BOEFFLA_WL_BLOCKER=y`
- `CONFIG_MSM_PERFORMANCE=y` `CONFIG_CPU_FREQ_TIMES=y` `CONFIG_PSI=y`
- `LOCALVERSION="-BadazzKernel-sweet-v1.3.1"`

### Build
```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- sweet_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
# single object:
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- drivers/thermal/k6a_gov/k6a_gov.o
# ZIP:
cp arch/arm64/boot/Image.gz anykernel/ && cp arch/arm64/boot/dtb.img anykernel/ && cp arch/arm64/boot/dtbo.img anykernel/
cd anykernel && zip -r9 ../BadazzKernel-sweet-k6a-gov-v1.3.1.zip . -x "*.git*"
```

### Git History (main)
```
81d69ae k6a_gov v1.3.1: version bump + ticks format fix
d4835b6 fix: remove mutex from get_cd_max_freq to prevent deadlock
53bb809 v1.3.1: Fix build + 5 hardening points
967c134 v1.3.0: KB15 hash coupling, KB12 Phase 2 BW floors, profile 5
dfcb96b v1.2.1: KB14 badazz_safe, KB8 multi-zone temp, clamp_freq fix
602a281 KB11: CONFIG_K6A_GOV=y in sweet_defconfig
33ec2a1 k6a_gov v1.2.0: GPU enforcement, history, bw monitor
23aae0a k6a_gov v1.1.2: battery_guard, poll_ms, GPU levels RO
```

## k6a-ctl Companion
- **Repo**: `vandalsquad187/k6a-ctl` branch `main` @ `b130d68`
- **Mode**: `delegated=1` — Kernel handles thermal (CPU/GPU/BW), module handles game detection + sched + auto `badazz_safe` @85°C
- **WebUI**: Gov-Status, BW-Floors, GPU-Caps, History timeline
- **Check**: `bin/check_module.sh` delegation-aware gate
- **Synergy**: `k6a-ctl` writes `profile`/`enable` to `/sys/kernel/k6a_gov/`, reads `status`

## KernelSU-Next SUSFS
- SUSFS in `fs/susfs.c`, `include/linux/susfs.h`
- Hooks in `fs/stat.c` (`CONFIG_KSU_SUSFS_SUS_KSTAT`) and `kernel/sys.c` (`CONFIG_KSU_SUSFS_SPOOF_UNAME`)
- Submodule `KernelSU-Next` @ `a5ff54c` (v1.0.2-771)
- `git submodule update --init --recursive` required for fresh clone

## Common Issues
1. **Fresh clone build fails**: need `CONFIG_KSU_SUSFS=y` + sub-options, submodule init
2. **SUSFS implicit declaration**: guard calls with `#ifdef CONFIG_KSU_SUSFS_*`
3. **Deadlock on cat status**: fixed in d4835b6 — `get_cd_max_freq` must not take mutex
4. **Hardcoded CPU6**: fixed — use `find_gold_cpu()` portable

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
