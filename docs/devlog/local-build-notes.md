# local build notes (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> Local Build Notes (Termux): Syntax-Check-Rezept vor jedem Push

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

