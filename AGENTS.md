# Kernel Build Status

## Current State
- **Repo**: `vandalsquad187/kernel_xiaomi_sm6150` branch `23.2`
- **Branch**: `23.2`
- **Local**: Booted and stable (SUSFS works)
- **GitHub**: Build successful (after SUSFS defconfig fix)

## Changes Made

### KernelSU-Next SUSFS (IMPORTANT - must be present in tree)
- SUSFS source in `fs/susfs.c`, `include/linux/susfs.h`, `include/linux/susfs_def.h`
- SUSFS hooks in `fs/stat.c` (guarded by `#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT`) and `kernel/sys.c` (guarded by `#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME`)
- `include/linux/susfs.h` passes `struct seq_file` as forward decl for `susfs_spoof_cmdline_or_bootconfig()`

### sweet_defconfig
- `CONFIG_KSU_SUSFS=y` and all sub-options enabled
- `CONFIG_KSU_TAMPER_SYSCALL_TABLE=y`
- `CONFIG_SCHED_TUNE=y`
- `CONFIG_KSM=y`
- `CONFIG_BOEFFLA_WL_BLOCKER=y`
- LOCALVERSION: `-OurKernel-sweet-v4.2.4`

### Local Build Method
```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- sweet_defconfig
# then edit .config if needed
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)
```

### Git History (23.2 branch)
```
fbd15c9d1 Fix SUSFS: enable in defconfig, guard calls with ifdef
b8eae3a16 Update: add SUSFS sus_mount support
```

## k6a-tune Module
- **Repo**: `vandalsquad187/k6a-tune` (master)
- **Location**: `/home/x/Downloads/k6a-tune/`
- **Build**: `/home/x/Downloads/k6a-tune.zip` (17.5KB)
- **Architektur**: 2 Profile (Daily/Gaming), 3 Themes (Dark/Light/AMOLED), Console
- **Kernel**: OurKernel-sweet-v4.2.4 — SM7150 (Silver 1804MHz, Gold 2208MHz)
- **Features**: `UCLAMP_TASK`, `SCHED_CASS`, `BOEFFLA_WL`, `LRU_GEN`, `ZRAM(lz4)`, `BBR`, `THERMAL_WRITABLE_TRIPS`
- **Design**: COPG-inspirierte WebUI mit Glas-Effekten, Bottom-Nav, Shell-Console

## Common Issues
1. **Build from fresh GitHub clone fails**: Need `CONFIG_KSU_SUSFS=y` in defconfig + all sub-options
2. **KernelSU-Next submodule**: Must be initialized (`git submodule update --init --recursive`)
3. **implicit declaration errors**: Source files must guard SUSFS calls with `#ifdef CONFIG_KSU_SUSFS_*`
