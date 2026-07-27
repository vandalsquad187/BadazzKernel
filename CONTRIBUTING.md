# Contributing to K6A Kernel

## Build Instructions

### Prerequisites
- Ubuntu 22.04+ (or equivalent)
- GCC 13+ or Clang 15+
- Python 3.8+
- Perl
- ccache (optional, for faster incremental builds)

### Local Build
```bash
# Clone the repository
git clone https://github.com/vandalsquad187/kernel_xiaomi_sm6150.git
cd kernel_xiaomi_sm6150

# Initialize KernelSU-Next submodule
git submodule update --init --recursive

# Configure for sweet device
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- sweet_defconfig

# Optional: Fix GCC compatibility issues
sed -i 's/CONFIG_LLVM_POLLY=y/# CONFIG_LLVM_POLLY is not set/' .config
sed -i 's/CONFIG_CC_STACKPROTECTOR_STRONG=y/# CONFIG_CC_STACKPROTECTOR_STRONG is not set/' .config
sed -i 's/.*CONFIG_CC_STACKPROTECTOR_NONE.*/CONFIG_CC_STACKPROTECTOR_NONE=y/' .config
grep -q 'CONFIG_CC_STACKPROTECTOR_NONE=y' .config || \
  echo 'CONFIG_CC_STACKPROTECTOR_NONE=y' >> .config
sed -i '/CONFIG_CC_STACKPROTECTOR_REGULAR/d' .config
make olddefconfig

# Build with ccache for faster incremental builds
export USE_CCACHE=1
export CCACHE_DIR=/tmp/ccache
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) Image.gz dtb.img dtbo.img
```

### CI Build
Push to branch `23.2` to trigger CI automatically.

## Patch Submission

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes
4. Test build locally
5. Commit with a descriptive message
6. Push to your fork
7. Submit a Pull Request

## Code Style
- Follow kernel coding style (K&R, 8-space indentation)
- Max 80 characters per line
- Use `scripts/checkpatch.pl` to verify style

## Architecture Overview

### Kernel Components
- **Thermal Management**: `drivers/thermal/cpu_cooling.c` — dynamic thermal floor
- **CPUFreq**: `drivers/cpufreq/cpufreq.c` — libperfmgr validation layer
- **GPU**: `drivers/gpu/msm/kgsl_pwrctrl.c` — dynamic GPU thermal floor
- **SUSFS**: `fs/susfs/` — modular root hiding framework

### SUSFS Modules
```
fs/susfs/
├── susfs.h              # Shared header
├── susfs_core.c         # Core logic + logging
├── susfs_path.c         # SUS_PATH feature
├── susfs_kstat.c        # SUS_KSTAT feature
├── susfs_mount.c        # SUS_MOUNT feature
├── susfs_spoof.c        # SPOOF_UNAME / CMDLINE
├── susfs_open_redirect.c # OPEN_REDIRECT + SUS_MAP
└── susfs_features.c     # AVC spoofing, version info
```

### Sysfs Interfaces
- **CPU Thermal Floor**: `/sys/kernel/k6a_thermal/thermal_floor_temp{1,2,3}`
- **GPU Thermal Floor**: `/sys/class/kgsl/kgsl-3d0/thermal_floor/`
- **Invalid Writes Counter**: `/proc/cpufreq_invalid_writes`

### Thermal Zones (SM7150)
- Zone 0: pm6150-tz (active)
- Zone 3-5: pm6150-vbat-lvl0/1/2 (disabled)
- Zone 6-8: pm6150-bcl-lvl0/1/2 (disabled)
- Zone 14-16: pm6150l-bcl-lvl0/1/2 (disabled)
- Zone 24: cpu-0-0-usr (Silver Temp)
- Zone 26: cpu-1-0-usr (Gold Temp)
- Zone 74: quiet_therm (active)

## KernelSU-Next
- Version: 33300
- Manager compatibility: ≥ 33188
- Submodule: `KernelSU-Next` (branch: k6a-sweet)

## Known Issues
1. Build from fresh GitHub clone fails without `CONFIG_KSU_SUSFS=y` in defconfig
2. KernelSU-Next submodule must be initialized
3. GCC 13 requires stack protector workaround
