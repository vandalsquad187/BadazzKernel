# compat modules susfs (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> SUSFS, Android-17-Audit, Boot-Hang-Root-Cause, Modul-Kompatibilitaet, NFC, BRENE

## KernelSU-Next SUSFS
- SUSFS in `fs/susfs.c`, `include/linux/susfs.h`
- Hooks in `fs/stat.c` (`CONFIG_KSU_SUSFS_SUS_KSTAT`) and `kernel/sys.c` (`CONFIG_KSU_SUSFS_SPOOF_UNAME`)
- Submodule `KernelSU-Next` @ `b100bd28` (`v3.3.0-95-gb100bd28`, runtime `KSU_VERSION=38309`, UAPIv4)
- `git submodule update --init --recursive` required for fresh clone

## Android 17 Readiness (Audit 2026-10-23)

A17 unterstützte Kernel-KMIs (AOSP Q4-Sync): `android17-6.18` (neu), `android16-6.12`
(GKI), `android15-6.6`, `android14-6.1`, `android14-5.15` (Legacy-GKI). Unser 4.14
ist nie GKI gewesen → bleibt **Legacy-Device-Path** mit launch-level VINTF-Konfig;
A17 zwingt dem Kernel selbst keine Änderung auf. Ourkernels-Verdict: **A17-tauglich
wie er ist**, keine fürsche Kernel-Arbeit nötig. Konsequenzen/Items:

1. SUSFS-UNAME-Spoof `6.12.0-android16-6.12-perf-g1d01cd293` bleibt **gültig** —
   KMI `android16-6.12` ist in A17 offiziell unterstützt; der Spoof deckt den
   A17-Versionscheck für unsere 4.14-Basis ab.

2. Config-Gate-Audit (nach Susfs-Bump, vor erstem sweet-A17-ROM-Flash):
   defconfig gegen `kernel/configs/android-4.14` diffen, VINTF-Nerln (4.14-
   Legacy-Frage) unverändert lassen; der build-Gate prüft Richtung dort schon.

3. KSU/k6a-ctl: susfs-Supercall-ABI = v2.3.0 (Build 394 gesynced ✓),
   KernelSU-Manager ABI compat essential; sepolicy.rule des Moduls unverändert
   (A17 hat dieVendor-Namespace-Änderungen die wir brauchen NICHT geändert).

4. Test: erster sweet-A17-ROM-Build = Boot + KSU + k6a_gov insmod seulement.
   Bei Failure: `/proc/version`-Spoof-String + defconfig diff. Kein Plan-B
   (6.1-Legacy bleibt bis Samsung Supportende).

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
| **FastCharging** | ⏸️ | **Deaktiviert 2026-11-21** — No-Op auf sweet: kein service.sh, Ziel-Nodes (`/sys/kernel/fast_charge`, `allow_hvdcp3`, …) fehlen, thermald=Stock. Kernel macht FastCharge selbst (`BQ2597X_CHARGE_PUMP` + `QPNP_SMB5`, dmesg ICL 3 A / FCC 4,3 A). `disable`-Flag gesetzt; bei Bedarf per `rm …/disable` + Reboot reaktivieren, dann Ladeleistung gegen Kernel-Stock messen |
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

## BRENE-SuSFS spoofs `uname` auf 6.12.0 (2026-11-08, am Gerät verifiziert)

- **Befund**: Das Gerät läuft **BadazzKernel v4.14.369-badazz-build432**
  (Code-Commit `a220c80d`, k6a_gov v1.7.0). `uname -a` zeigt aber
  `6.12.0-android16-6.12-perf-g1d01cd293, Jan 26 2026` — **die
  Uname-Zeile ist gespoofed**.
- **Quelle**: das KernelSU-Modul `brene` = „BRENE - SuSFS" v0.0.72
  (`/data/adb/modules/brene`, Autoren rrr333nnn333/simonpunk/KOWX712,
  SuSFS-Backend). Es fälscht für Userspace `/proc/sys/kernel/osrelease`
  + `/proc/sys/kernel/version` und versteckt Root-Artefakte.
- **Verworrene Folge-Auswertung** (2026-11-08): der „kernel build 433
  checken"-Befund sprach fälschlich von einer „anderen 6.12-Kernel-Linie".
  Richtig ist: `uname` ist mit BRENE-Modul **nie** eine Kernel-Verifikation.
  Die CI-Artifacts (`build433aosp`/`build433miui`, run 42436271342, kein
  Auto-Release) und die Release-Chain `build416…432` sind unabhängige
  Ketten; build433 enthält keinen Kernel-Code-Diff (HEAD nur Actions-
  dependsabot-Bumps `c2c06e6f`; Code bleibt `a220c80d`).
- **Zuverlässige Kernel-Verifikation am Gerät**:
  1. `/sys/kernel/k6a_gov/status` → `version=1.7.0` + `build_hash=a220c80d`
     (k6a_gov's eigener Build-Stempel, im Kernel verdrahtet),
  2. `uname -a`/`/proc/version` = Userspace-ge-fälscht — nicht verwenden.
- **Kernel-Side Follow-up** (Notbrems-Akku-Arm, k6a-ctl v1.5.4):
  k6a_gov **v1.7.1** füllt `bt` aus `power_supply/battery/temp` —

  **Build 434 (2026-11-08, gelandet, Commit `e9eef01cf`)**:
  `battery_guard_tick()` liest die Akkutemp JEDEN Tick (auch
  `battery_guard=0`), `bt_live`-Flag + Einmal-Log
  `Build434: bt live %dC guard=%u`, Latch-Trip-Zeile auf
  `Build434:` re-markiert, Version 1.7.0 -> 1.7.1 (`kver_ok`
  dynamisch unverändert). Semantik-Erhalt: guard-off Lesefehler
  -> `bt=-1` wie vorher; guard-on Lesefehler -> Latch + letzter
  Wert behalten. Checks vor dem Push: Host-Test **23/23**
  (S4-Battery-Latch-Case über die echte battery_guard_tick-
  Referenz), clang-fsyntax-only Pristine-Vergleich
  (Fehler-/Warnset byte-identisch, +7 Zeilenoffset).
  CI-Run 42439679297 grün (Resync/aosp/miui, Release-Job
  skipped per Definition), Artifacts `build434aosp` +
  `build434miui` (kein Auto-Release, Latest bleibt build432).
  Host-Test-Gate-Run 42439679366 grün direkt nach Push.
  **Offen**: aosp-ZIP flashen + am Gerät verifizieren
  (`bt=XX` != -1 im status, eine `Build434: bt live`-Zeile in
  dmesg, k6a-ctl v1.5.4 Notbrems-Akku-Arm scharf); danach
  Issue #3 schließen.

