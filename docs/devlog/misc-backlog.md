# misc backlog (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> TODO-Tunings-Backlog + Device-Cherry-Picks

## TODO Kernel Tunings (backlog, nicht gepusht)

- [x] **SUSFS Split-Build-Nachzüge (Befunde Build 478-Audit, Details
  `susfs-priority-audit.md`) — beide geschlossen**: (a) **B6 Renew-Trigger**
  (geschlossen Build 481): Observer via `KSU_LSM_HOOK_INIT(inode_rename,
  "selinux_inode_rename")` gegen die aktive `hook/lsm_hook.c`-Replace-Impl
  gekettet + **`CONFIG_KALLSYMS_ALL=y`** (ohne war `security_hook_heads`
  als Data-Symbol unauflösbar — v1 INSERT aus `susfs/lsm_hook.h` war
  tot-gebaut, nicht im Kbuild); Events + `susfs_parse_priority_app_names`-
  Worker live (install UND uninstall). (b) **B7 Marker** (geschlossen
  Build 479): `late_initcall(susfs_init_late)`-Wire; `Build481: susfs
  core …` @t=1.16 s.

- [x] **`del_priority_uid_filter` im Device-CLI fehlt** (Build484): Kernel-CMD
  `CMD_SUSFS_DEL_PRIORITY_UID_FILTER 0x60031` + `CMD_SUSFS_DEL_SUS_PATH_UID_FILTER
  0x55556` mit Clear-Handler (Bitmap NULL, app_strings kvfree); BRENE-CLI hat
  weiterhin kein `del`-Verb — stattdessen helper `susfs_del` (freestanding
  aarch64, Raw-Syscalls, Android-`__NR_reboot=142`) nach `/data/adb/ksu/bin/`:
  `susfs_del priority|sus_path|path </path>`. Achtung: `0xFAFAFAFA` darf nicht
  als C-Argument an register-asm-Variablen übergeben werden (-Os miscompiles
  High-Half zu `0x0FAFAF` → SUSFS-Block matcht nicht); movz/movk inline.

- [x] **Doppel-Dispatch Supercall** (Root-Cause SUSFS-Symptome 485) —
  **Build486 abgeschlossen** (`96b6b840e`, Submodul `507203ee`):
  `hook_aarch64_reboot`/`hook_armeabi_reboot` riefen `ksu_handle_sys_reboot()`
  und danach Original-`sys_reboot()`; `kernel/reboot.c` rief den Handler im
  Originalpfad ein zweites Mal → jeder Supercall doppelt (Feature-Liste 20
  statt 10, open_redirect -EEXIST, sus_path -ENOENT → Shell-Break via
  `add_sus_path /data`). Fix: alle 4 Hooks short-circuiten auf
  `magic1 == KSU_INSTALL_MAGIC1` + `enabled_features[0]='\0'` nach
  `copy_from_user`. Live486: 10 Features, CMD je 1× LOGI, priority Cross
  A/B ESRCH(3), open_redirect add/dup korrekt. **Achtung**: `add_sus_path
  /data` im Test löste Soft-Hang aus (zygote-Loop, kein Panic) — nicht
  wiederholen; BRENE auto-hidet `/data/local/tmp` (Pfad-Tests unter
  `/data/adb/`). TRY_UMOUNT: toter Config, nicht in Feature-Liste.

- [x] **1. Sched+VM Tune** (`f5aaa8b` DarkKiller28) — **Build485 abgeschlossen**:
  Device-`post_boot.sh` hatte die Werte bereits (Commit steckte im Vendor-Image);
  Kernel-Seitig fehlten die Sysctls (`#ifdef CONFIG_SCHED_DEBUG`, gepinnt aus in
  Hardening `35023093f`). Fix: `CONFIG_SCHED_DEBUG=y` in sweet_defconfig
  (`5e03e93bd`) — `sched_proc_update_handler` normalisiert beim Schreiben,
  deshalb reichte Sysctl-Exposure nicht, Handler musste mit. Live485:
  latency/min_gran/wakeup/migration/nr_migrate = 6ms/1ms/0.5ms/0.25ms/64 sticken,
  sched 65/85 + watermark 35 + dirty_ratio 30 + min_free 32768 live.
  **Nicht sticken** (Android-services resettet): dirty_expire 1500/writeback 150 —
  akku-kritisch laut Original-Meinung eh draußen (swappiness 10 + extra_free 30375
  aktiv, nicht 160/131072). BORE `sched_burst_*` + `sched_*_boosted` bleiben
  Kernel-fremd (kein BORE/WALT-Boost im 4.14-Port) — post_boot-Echos laufen
  ins Leere, unkritisch. Reflex-Governor (Punkt 2) separat.
- [x] **2. Reflex Governor** (`80a346a`) — **gestrichen 2026-11-24** (User-Entscheidung):
  kein unabhängiges Benchmark, Gaming-Tuning deckt PELT-Lücke schon ab
  (schedutil 500µs/50 hispeed aggressiv genug), k6a-ctl-Redesign > Gain;
  Anekdoten teils negativ für Gaming. Port (~500 LOC) nicht gelohnt.
- [x] **3. Audio BT v7** (`182eb86`): `audio_policy_configuration.xml` −15 BT A2DP + `sm6150.mk` +1 `bluetooth_audio_policy_configuration_7.0.xml` — *Device, Low-Risk, geplant als 4-Step Cherry-Pick (21 Zeilen)*

## Device Cherry-Pick
- **Quelle**: `DarkKiller28/android_device_xiaomi_sm6150-common@182eb86`
- **Ziel**: `device/xiaomi/sm6150-common` — `configs/audio/audio_policy_configuration.xml` (remove 3× `BT A2DP` ports + 3× `route`) + `sm6150.mk` (`PRODUCT_COPY_FILES` `bluetooth_audio_policy_configuration.xml → /vendor/etc/bluetooth_audio_policy_configuration_7.0.xml`) + `xi:include` nach `r_submix`
- **Steps**: `git fetch DarkKiller28 182eb86 && git cherry-pick -x 182eb86` → `mka` → `adb ls /vendor/etc/bluetooth*.xml` + BT Pair

