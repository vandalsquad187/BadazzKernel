# AGENTS — BadazzKernel (Dev/Agent-State)

Kurz-Referenz für Agenten/Developers. **Historie, lange Status-Logs und Build-/Runden-Reports
leben in [`docs/devlog/`](docs/devlog/)** — 2026-11-19 beim Docs-Umbau aus dieser Datei
ausgelagert (Inhalte unverändert). Öffentliche Nutzer-Doku: [`README.md`](README.md);
Governor-Regelwerk: [`docs/k6a_gov_rules.md`](docs/k6a_gov_rules.md);
k6a-ctl: separater Repo-Stand mit eigenem `README.md` + `CHANGELOG.md`.

## Current State (Stand 2026-11-23)

- **Gerät: Build 483 = Stand** (Commit `bd8a80e4e`, CI Run #45253929619 grün,
  `flash483.sh` rc=0 „flash done", **AK3 ohne Auto-Reboot** — manueller Reboot
  nötig, Boot-Backup `boot-build482.img`): **Build482 = `sus_path_uid_filter`-Port**
  (Commit `daf3ee056`, Allow-List, CMD-Tausch `0x55554`=SET/`0x55555`=DEL, Port aus
  JackA1ltman `susfs_patch_to_4.14`, Stale-Gate + `kfree_rcu` wie priority,
  Observer-Renew `susfs_parse_sus_path_app_strings` an beiden packages.list-Pfaden);
  **Build483 = B8-Renew-Gate-Fix** (Worker-Snapshot wurde von `strsep` mutiert —
  ab 2 Paketen `strcmp`-Gate immer fehl, Swap + `resolved again` entfielen, beide
  Filter; Fix: Parse auf `parse_buf`-Copy, Gate gegen pristine). **Evidence live483**:
  `Build483: susfs core …` @t=1.19 s + `git_hash=bd8a80e4`, B6-Kette auto
  (`patched inode_rename` + `observer chained` @t=6.9 s), Multi-Set rc=0
  (sus_path `gms+termux+ckd`, priority `gms+termux`), **Multi-Paket-Renew nach
  ckd-Install → beide Worker `resolved again`**, Sichtbarkeits-A/B: ckd in Liste
  `run-as` liest `susvis/f`, raus → `ENOENT`, Shell `u:r:su:s0`/uid0 versteckt
  (Truth nur `u:r:ksu:s0` via `runcon`), `add_sus_path`-Flag lebt
  (Parent `d?????????`, Lookup `-ENOENT` in `fs/namei.c`), CLI
  `set_sus_path_uid_filter` → `0x55554` → `ret: 0` + LOGI. Vorgänger: 482/483
  Details devlog `susfs-priority-audit.md` (B8), B6/B7 in 479–481 geschlossen.
  Teststand: sus_path + priority Filter `com.google.android.gms`+`com.termux`,
  `enable_log 1`, ConkyDroid deinstalliert (CLI hat kein `del` → Backlog
  `del_priority_uid_filter`/`del_sus_path`). SUSFS-Log nach Boot
  runtime-disabled → `susfs enable_log 1` vor LOGI-Checks.
  Freeze-Welle #10–#12 weiterhin offen (Sniffer-×3 v7.1 als Alarm, Wakelock bewusst aus).
- **Gerät (478er-Stand, historisch)**: Build 478 = Audit-Race-Fix, Commit `b14b62525`
  Patch `f3fade9d3` aus dem Audit-Review, angewandt + Kommentare gestrichen +
  Marker-Bump + Current-State): susfs `priority_uid_filter` Stale-Renew-Gate
  (strcmp vor Swap — upstream-erbter Bug, `WildKernels …/fs/susfs.c:2103-2145`
  hat dasselbe Muster) + Flag-Lock für `is_uid_filter_set_before` + SPDX-Header;
  Scope USER-only als Known Decision. **Evidence**: Syntax-Check rc=0/0 Errors,
  CI **Run #478 success** (`45077297214`), Flash `flash478.sh` rc=0 „flash done"
  (Backups `boot-build477.img`), live Banner `build478` + `git_hash=b14b6252`,
  SET rc=0 + LOGI, **ESRCH-Paar 4/4 PASS** (Root-Bypass `-20` ↔ Cross `ESRCH(3)`
  symmetrisch). Details + neue Befunde **B6 (Renew-Trigger tot —
  `ksu_rename_observer` ohne Caller, Worker läuft nie)** /
  **B7 (`susfs_init` tot — Marker `Build478:` nie sichtbar)**: devlog
  `susfs-priority-audit.md`; Backlog: `misc-backlog.md`. SUSFS-Log nach Boot
  runtime-disabled → `susfs enable_log 1` vor LOGI-Checks. Freeze-Welle #10–#12
  weiterhin offen (Sniffer-×3 v7.1 als Alarm, Wakelock bewusst aus).
- **Gerät (477er-Stand, historisch)**: Build 477 = Reboot-Fix `7fc358f27`, CI Run 45049445376 grün,
  AK3-Flash `~/tmp/flash477.sh` rc=0, live verifiziert `git_hash=7fc358f2` +
  Banner `build477`, Boot-Backup `boot-build476.img`: susfs
  `priority_uid_filter`-NULL-Guard (`kfree_rcu(NULL)` beim ersten SET panickte
  ≤476 — Ramoops-Trace `__call_rcu ← kfree_call_rcu ←
  susfs_set_priority_uid_filter ← ksu_handle_sys_reboot`, History-Beleg
  `kernel_panic,null,1795359972`); getpriority/setpriority-Funktionstest
  **PASS** (PRIO_USER-Cross-Test ESRCH vs. Root-Bypass, Hook `kernel/sys.c:244/316`).
  Reboot-Hunt Events #10–#13 + Freeze-Welle (offen, Beobachtung mit
  `kwatch_nosuspend`-Wakelock + Sniffer-×3 v7.1): devlog
  `compat-modules-susfs.md` 2026-11-22.
- **Repo**: `vandalsquad187/BadazzKernel` `main` — Six-Point-Review Phasen 1–4 umgesetzt
  (Phase 1 `0fe3023b8` CI-Hygiene, Phase 3 Docs-Umbau `6c85d4506`, Build 463 = Phase 4
  Consensus-Gate, dieser Commit).
- **Gerät (468er-Stand, historisch)**: ~~Build 468~~ (Charge-Thermal A+B, `38aef538a`, CI Run 44915170922
  grün, Flash + **A/B Lademessung PASS** devlog XVI): bei stl=23 FCC-Vote
  **3 A** (vorher 1 A), FC2/CP aktiv bis Level 25 (`MAX_THERMAL_LEVEL 26`), ibat
  2.0–2.3 A, Live-DT 28 Levels mit L23=3000 mA. Flash-Quelle
  `/data/local/tmp/build468.zip`. Vorher 465 = Härtung INIT_ON_ALLOC/STACK_END/WBT,
  R2 ✅ PASS (devlog XII) — Thermal-R2 auf 468 noch offen. **FastCharging-Modul
  entfernt** (`/data/adb/modules/FastCharging/` existiert nicht mehr; No-Op-Analyse
  devlog XIV, A/B gegen Kernel-BQ2597X).
- **Build 463 committet (gov 1.7.7)**: Consensus-Gate — `temp_smooth` zieht **nur bei
  Konsens (alle lebenden Gold-Zonen steigen gemeinsam)** nach oben (schließt die +15-K-
  Bias-Grenze: ohne Konsens friert die Referenz, Klemme absorbiert dauerhaft), L4-Latenz
  via **Peak-Backstop** (Roh-Peak ≥ l4 über 8 Ticks → L4; Exit hält Roh-Peak-Margin,
  kein Flap); S8d-Golden gekippt „Bias eingefroren". Host-Tests **167/167**, DOC-DRIFT grün,
  Syntax-Check pristine-compared 0/0, R4 im selben Commit, `kver_ok()` deckt 1.7.7 ab.
  **CI grün** (host-test 17, Lockdep 24, Build Kernel **Run 464** — der Phase-1-Push
  verbrauchte 463, Marker/Commits bleiben einheitlich „Build 463"; Flash-Quelle =
  Artefakte `Badazz-kernel-sweet-v4.14.369-build464aosp`/`…miui`).
  **Flash ✅ + R2-Feldrevalidierung 2026-11-19 ✅ PASS**: L4 0 s, ≥86 °C 1/1500
  Samples (1-s-Exkursion, 0,07 %, unter L4-Schwelle 87; hb-Reihe 0), spike_drops 0,
  Backstop 0 Treffer, Exakt-Schwellwert-Eskalationen, subjektiv
  **+5–10 fps** vs. 461 und flüssig bis zum Ende (Bericht: devlog
  `build-scopes-and-rounds.md` IX; 1-Hz-Serie `io_r60r_DONE_1795121151.log`).
- **k6a-ctl**: **v1.6.8 / versionCode 178** committet (`81c0c66` pidof→/proc-Scan Tri-State,
  `65ff32a` Persistenz `cd_ladder`/`cd_silver_caps`), ZIP `k6a-ctl-v1.6.8.zip` gebaut
  (Gate OK) — **deployed 2026-11-19 nach User-OK**: Backup `/data/adb/k6a-ctl-bak-v167`,
  atomare `mv`-Ersetzung, settings.conf-Keys angehaengt, sauberer Controller-Restart
  (Banner 1.6.8 mit `cd_ladder=1 silver_caps=0 0 0` verifiziert, hb tick Reset, Tri-State-Verify live).
- **Review-Reihenfolge**: Phase 1 ✅ → Phase 2 ✅ → Phase 3 ✅ → Check ✅ → Phase 4 ✅
  (Build 463/464 vollständig: CI ✅ → Flash ✅ → R2 ✅ PASS).
- Rollendes Status-Log bis Build 457: [`docs/devlog/state-log-to-build457.md`](docs/devlog/state-log-to-build457.md).

## k6a-ctl (Magisk-Modul — integraler Bestandteil des Kernel-Pakets, User-OK 2026-10-29)

- **Pfad** `/data/adb/modules/k6a-ctl/`, id `k6a-ctl`. **Gehört ausdrücklich zum Kernel**
  (User-Anweisung) — beim Kernel-Bau/-Flash mitdenken.
- **GATE-FALLE**: ko-Version > `kver_ok()`-Gate → Modul lädt NICHT, `/sys/kernel/k6a_gov`
  fehlt, stiller Legacy-Betrieb ohne Fehler-Toaster. Nach jedem k6a_gov-Version-Bump:
  `service.sh` `kver_ok()` erweitern + `GOV_KO_VER` setzen; Notfall-Reload:
  `insmod /data/adb/modules/k6a-ctl/k6a_gov.ko` (HB merkt es im nächsten Tick).
- **Aufteilung**: State-Maschine/Game-Detect/Floors/Temp = **Kernel** (`k6a_gov.ko`,
  `/sys/kernel/k6a_gov`); Config/Thresholds/Schedutil/WebUI = **k6a-ctl** (Userspace).
  Details: [`docs/devlog/ctl-companion.md`](docs/devlog/ctl-companion.md).

## Repo & Docs Layout

| Path | What it is |
|------|------------|
| `README.md` | Public, English. Features, install, versions, roadmap |
| `AGENTS.md` | This file — compact dev state, Fallen, Working Agreements, Common Issues |
| `docs/devlog/` | Ausgelagerte Historie (Status-Logs, Build-Scopes, Runden, USB, CODM, …) |
| `docs/k6a_gov_rules.md` | Governor-Entscheidungs-Regelwerk (R1–R4), bei jeder Regelungs-Änderung mitpflegen |
| `CONTRIBUTING.md` | Upstream kernel contribution guide (not project-specific) |
| `Documentation/` | **Stock Linux 4.14 kernel docs** (6090 files). No project content — do not put Badazz docs here |
| `anykernel/` | AnyKernel3 flash template used by CI |
| `.github/workflows/build-kernel.yml` | The CI that produces the flashable ZIP + release. ⚠️ `.gitignore` listet `.github` → **`git add -f .github/workflows/build-kernel.yml`** |


## CI (`build-kernel.yml`) — Kurzfassung

- Push `main` → Dual-Build (`aosp`+`miui`) + host-test + lockdep, **nur Artefakte**
  (Flash-Quelle); **Release `v4.14.369-badazz-buildNNN` nur per `workflow_dispatch`**
  (seit `f2b059635`, Build399-Lesson), 13–17 min;
  `miui/**` → nur Artefakte; **Pushes via GITHUB_TOKEN feuern kein CI**.
- `.github`-Push-Sicherheit: der miui-Resync-Job muss `.github/` nach dem Merge auf Branch-Stand
  zurücksetzen (Token darf Workflows nicht pushen); `.gitignore` listet `.github` → `git add -f`.
- Poll: `gh run view <id> --repo vandalsquad187/BadazzKernel --json status,construction` in
  `sleep 55`-Schleifen, **timeout ≥ 1700000 ms**; `gh release list --limit 3`.
- Vor jedem Push: Syntax-Check-Rezept in [`docs/devlog/local-build-notes.md`](docs/devlog/local-build-notes.md)
  (pristine-compared gegen den Fehlerkatalog).
- Vollständige CI-Mechanik (Matrix, Config-Pfad, ccache, Artifact-Naming-Falle, release-Body):
  [`docs/devlog/ci.md`](docs/devlog/ci.md).

## Working Agreements

- **No code comments** — rationale lives in the commit body and in `pr_info` markers.
- **Marker style**: `pr_info("BuildNNN: …\n", …)`; grep target must be unique per build.
- **Commits**: title + evidence body, **no `Co-Authored-By`**. The Write tool is broken on this box
  (emits NULs) → always commit with a heredoc:
  `git commit -q -F - <<'EOF' … EOF`
- **CI**: push to `main` → Dual-Build-Artefakte (~13–17 min); Release nur per `workflow_dispatch`. Poll with
  `gh run view <id> --repo vandalsquad187/BadazzKernel --json status,conclusion` in `sleep 55`
  loops, **timeout ≥ 1700000 ms**. Releases: `gh release list --repo vandalsquad187/BadazzKernel --limit 3`.
- **Syntax-check before push** (see `docs/devlog/local-build-notes.md`), always pristine-compared.
- User-facing reports are written in **German**.

## Known Decisions (bewusst so, nicht „offener Befund“)

- **`CONFIG_K6A_GOV=m`** (nicht `=y`) — seit Build 341 (`c42a67d07`) absichtlich:
  Version-Lock via `kver_ok()`/insmod in k6a-ctl `service.sh`, `.ko` in der ZIP,
  CI-Assert `CONFIG_K6A_GOV=m`. Kconfig-Help: „Build as module for version-lock“.
  Details: `docs/devlog/k6a-gov-history.md` (Build-341-Abschnitt).
  Umstellung auf `=y` würde die gesamte Gate-Maschinerie entsorgen — nur nach
  expliziter Entscheidung, nicht „nebenbei“.
- **WebUI-Token liegt im ausgelieferten `webroot/app.js`** (`const KTOKEN=…`, von
  `service.sh` gesedet) — Known Decision, kein Umbau geplant. Scope: **Schutz vor
  cross-user/cross-device, nicht vor on-device Apps** — jede App kann
  `127.0.0.1:8767` erreichen und das ausgelieferte JS lesen. Bind nur
  `127.0.0.1` (extern ausgeschlossen), Token-Gate (`k=`) auf state-changing
  Routes, `config/webui.token` `chmod 600` + gitignored.
- **`# CONFIG_FTRACE is not set`** gepinnt in `sweet_defconfig` — overridet
  `default y if DEBUG_KERNEL` (`kernel/trace/Kconfig:170`); Events laufen über
  `CONFIG_TRACING`/tracefs ohne Function-Tracer. Device-Beweis:
  `/sys/kernel/tracing/events/k6a_gov/k6a_state` (id=1621). CI-Assert in
  `build-kernel.yml` verhindert stillen Flip. KPROBES bleibt `=y`
  (KernelSU-Next `depends on KPROBES`).
- **`verity_dev_keys.x509` aus Repo-Root entfernt** — byte-identisch mit
  `certs/verity.x509.pem`, nur von x86-cuttlefish referenziert (wird nicht
  gebaut); `sweet_defconfig` hat `SYSTEM_TRUSTED_KEYS=""`. Kein Secret.
- **KernelSU-Next-Pin: manuell bumpen, nie auto-force** — Submodul @
  `7abf1c77` auf Branch `dev-upstream-merge` (eigener Fork
  `vandalsquad187/KernelSU-Next`, URL in `.gitmodules`; Fork-Default `dev`
  liegt *hinter* dem Pin — kein Dependabot-`gitsubmodule`, das würde gegen
  `dev` „downgraden“). Wöchentlicher `ksu-pin-check.yml` vergleicht nur mit
  Upstream `KernelSU-Next/KernelSU-Next` `dev` und failt bei Behind
  (Notify, kein Push). Bump: FF, bei Divergenz Merge von `upstream/dev` in
  `dev-upstream-merge` (2026-11-21: 150 upstream-Commits, ours 127 ahead →
  Merge `e7312bd2` + 4.14-Fix `7abf1c77`); danach CI-Patch-Gate
  (`patches/ksu-prctl-gate.patch` dry-run + 3 Assertions). Ausdrücklich
  abgelehnt: tägliches `reset --hard` + `push --force` auf `main`.
- **Priority-UID-Filter: Scope USER-only** — Hooks nur in `PRIO_USER` /
  `IOPRIO_WHO_USER` vor `find_user()` (`kernel/sys.c:244/316`,
  `block/ioprio.c:140/241`), upstream-identisch (Upstream-Heimat
  `WildKernels/GKI_KernelSU_SUSFS`, Patch `50_add_susfs_in_gki-…patch`,
  Patch-Z. 2406-2439/9-45; `simonpunk/susfs4ksu` ist 404 — Quelle von
  `97cbbaf`/`2557ea2` nicht mehr auflösbar). PROCESS/PGRP bewusst offen:
  Ziel-PID-Kenntnis + proc-Sichtbarkeit sind separat abgedeckt; Abweichen
  erzeugt bei jedem Upstream-Sync Merge-Reibung. Der `kfree_rcu`-NULL-Guard
  (Build476) ist 4.14-spezifisch — Upstream zielt auf 6.18 GKI, wo
  `kfree_rcu(NULL)` safe ist.

## Common Issues
1. **Fresh clone build fails**: need `CONFIG_KSU_SUSFS=y` + sub-options, submodule init
2. **SUSFS implicit declaration**: guard calls with `#ifdef CONFIG_KSU_SUSFS_*`
3. **Deadlock on cat status**: fixed in d4835b6 — `get_cd_max_freq` must not take mutex
4. **Hardcoded CPU6**: fixed — use `find_gold_cpu()` portable
5. **Boot hang at crDroid logo** (Build 267-278): see `docs/devlog/compat-modules-susfs.md`
6. **Local Termux build fails** (`modpost` / `elf.h`, no `bison`/`perl`): use the syntax-check recipe in `docs/devlog/local-build-notes.md`, let CI do real builds
7. **USB `ep cmd timeout` / device not enumerating**: see `docs/devlog/usb-debugging.md`
8. **Whole SoC resets on USB-C plug, log empty**: that was Fault 4 — fixed in Build 336. If it ever
   recurs, do not trust logcat or `/proc/last_kmsg`; capture with `~/tmp/cap334.sh` and set
   `panic_on_oops=0` first so the oops does not reboot the phone (see `docs/devlog/usb-debugging.md`)
9. **A tool call hangs forever**: an orphaned worker is still holding the pipe — run
   `$SU -c "sh $HOME/tmp/cap_scan.sh kill"`
10. **OTG stick / `sdg` vanishes with no kernel log**: Termux's libaums
    (`com.github.mjdev.libaums.storageprovider.UsbDocumentProvider`) claims the interface via
    `UsbManager.openDevice()`, unbinding `usb-storage`. Restore with `usbfs/unbind` +
    `usb-storage/bind` — see *Fault 2* in *USB Debugging*. Do not chase it as a kernel bug; you
    will not find the holder in `/proc/*/fd` either, because root has `CapBnd=0` (no ptrace).
11. **Reboot-Rätsel (≤476) — zwei Klassen + Lese-Fallen**: erstes
    `susfs set_priority_uid_filter` pro Boot = `kfree_rcu(NULL)`-Panic (Guard seit
    `7fc358f27`/Build477). pstore/ramoops nur bei **warmen** Resets (Class B
    meist kalt → Gap unlesbar); History-Grund trennt: `kernel_panic,…` =
    Panic-Reset, plain `reboot` = SW-Reset. kmsg-Timestamps = µs, frieren in
    Suspend (awake-vs-Wall-Differenz = Suspend-Summe, keine Anomalie).
    Schnell-Beweis-Quellen: `ramoops-boot-*` (strings -n 4), History via
    `strings -a /data/property/persistent_properties`, Death-Archive
    `kwatch-full-prev-*`/`deathsniff/`.
12. **Susfs-„fehlende" Logs/Markers (historisch Stand 478, mittlerweile gefixt)**:
    (a) SUSFS-LOGI stumm, bis `susfs enable_log 1` — Boot-Zustand
    runtime-disabled trotz `DEFINE_STATIC_KEY_TRUE` (**bleibt**); (b)
    `pr_info("BuildNNN: susfs core …")` — war tot (B7), seit 479 via
    `late_initcall(susfs_init_late)` live; (c) „resolved again" — Renew-Trigger
    war tot (B6), seit **481** via `selinux_inode_rename`-Replace +
    `CONFIG_KALLSYMS_ALL=y` live (Events `renameat:` + Worker-Parser sichtbar);
    ab 2 Paketen zusätzlich B8-Gate-Fix (**483**: `strsep`-Mutation → Parse-Copy).
    Details: `docs/devlog/susfs-priority-audit.md`.

## Fallen (Pflichtkenntnisse)

1. **`pidof` sieht die Prozesse hier nicht** — `k6a-controller` (comm=`sh`),
   `vendor.thermal-engine`, `mi_thermald` landen nicht in `pidof`-Treffern. Kein `pidof`
   in Checks: `/proc/[0-9]*/cmdline`-Scan mit `case`-Builtin (0 Forks), **Muster nur in
   Shell-Variablen** (nie im argv eines Kindprozesses → sonst Self-Match „lebt"), `$$`
   -Exclusion belt+braces, Tri-State rc (0 lebt / 1 tot / 2 /proc unlesbar). Test-Scripts
   als Datei per Pfad starten (`sh <datei>`), nie `bash -c '… Muster …'`. Dekoys immer mit
   `>/dev/null 2>&1 &` (sonst halten sie die Tool-Pipe → Tool-Timeout).
2. **`CONFIG_FORTIFY_SOURCE=y` bootlooped** (Build 456/458a überführt; 4.14-FORTIFY vs.
   GCC 13.3 `__builtin_object_size`). Finalstand: **`INIT_STACK_ALL_ZERO=y` AN,
   FORTIFY AUS**. CI-Negativ-Assert in `build-kernel.yml` — Revert nur zusammen mit
   Entfernen des Asserts. Positiv-Assert für INIT_STACK mitgepflegt.
3. **Termux-su**: `su 'befehl'` ohne `-c` frisst die Argumente; die su-Shell verliert den
   KSU-PATH (`nc: not found`-Restarts) → `export PATH=/data/adb/ksu/bin:/system/bin:/system/xbin:$PATH`,
   absolute Pfade. Lokales `sh` = dash/bash, Device = mksh → Code mksh-kompatibel halten
   (local/case/`$(())`/`${var##*/}` ok). Write-Tool der Box ist defekt (NULs) → Commits
   immer per Heredoc `git commit -q -F - <<'EOF'`.
4. **Phantom-Killer**: Background/nohup-Worker sterben nach ~8,5 min (Android) — Dauertasks
   nur via `run/`-Files bzw. Foreground. Hängender Tool-Call = Orphan hält die Pipe →
   `$SU -c "sh $HOME/tmp/cap_scan.sh kill"`. Repo-Falle: `.gitignore` enthält `.github`.
5. **`uname`/`/proc/version` sind gespoofed** (BRENE/SUSFS `SPOOF_UNAME`): Melden
   `6.12.0-android16-6.12-perf-…` — **REAL ist 4.14.369-badazz**. `zcat /proc/config.gz`
   zeigt ebenfalls den Spoof-Kontext und ist **kein** Beweis gegen `sweet_defconfig`
   (u. a. FORTIFY-Y im IKCONFIG ≠ Repo-Default). Build-/Kernel-Beweis: dmesg-Banner,
   `/sys/kernel/k6a_features/{git_hash,kernel_version}`, `git_hash` ↔ Commit.
   Details: `docs/devlog/compat-modules-susfs.md`, `build-scopes-and-rounds.md`.

## Docs-Index ([`docs/devlog/`](docs/devlog/))

| File | Inhalt |
|------|--------|
| `ci.md` | CI-Mechanik: Dual-Build, miui-Resync, Config-Pfad, Artifact-Naming, release-Job |
| `state-log-to-build457.md` | Rollender Status-Log bis Build 457 (vor dem Umbau) |
| `ctl-companion.md` | k6a-ctl Magisk-Rolle, Companion-Details, v1.4.9-Runde |
| `usb-debugging.md` | USB-Faults 1–4 + On-device-Capture-Rezepte (Build 300–336) |
| `local-build-notes.md` | Local Build Notes (Termux): Syntax-Check-Rezept vor jedem Push |
| `k6a-gov-history.md` | gov v1.6.0-Hardening + Build-341-scope (`CONFIG_K6A_GOV=m`, insmod-Proof) |
| `compat-modules-susfs.md` | SUSFS, Android-17-Audit, Boot-Hang-Root-Cause, Modul-Kompatibilität, NFC, BRENE |
| `codm-methodology.md` | CODM-FPS-Messmethodik r24–r45 (Varianz, Gates, Mess-Werkzeuge) |
| `build-scopes-and-rounds.md` | Build-Scope-Reports 350–462 + Runden-Berichte r8–r16 (chronologisch) |
| `misc-backlog.md` | TODO-Tunings-Backlog + Device-Cherry-Picks |
| `gov-version-changelog.md` | README-Archiv: k6a_gov-Versionstabellen v1.6.0 → v1.7.6 |
| `readme-usb-fixedbugs.md` | README-Archiv: USB-Status (Faults 1–4) + Fixed-Bugs-Tabelle (v1.3.2) |
| `susfs-priority-audit.md` | Audit `susfs_priority.c` (Build 478): Race-Fix, Flag-Lock, Scope-Doku, offene Secondary-User-A/B |
