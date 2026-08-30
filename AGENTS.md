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
- `LOCALVERSION="-OurKernel-sweet-v4.2.9"` (legacy string, kernel is BadazzKernel)

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
