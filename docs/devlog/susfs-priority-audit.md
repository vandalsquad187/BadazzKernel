# susfs_priority.c — Audit (Build 478)

> Vollständiges File-Audit von `fs/susfs/susfs_priority.c` (Stand `90edf7a4c`),
> inkl. der vier Hook-Stellen (`kernel/sys.c`, `block/ioprio.c`) und der
> Defines (`susfs_def.h:63-67`, `susfs.h:164-168`). Befunde und ihre
> Behandlung im Build 478.

## Befunde

### #1 — Stale-Renew-Race (MEDIUM, gefixt in 478)

Der Renew-Worker (`susfs_priority_app_strings_worker`) kopiert bei Start
einen String-Snapshot (Z. 261-268), löst dann die UIDs auf (mehrere
`kern_path`+`vfs_getattr`, Z. 277 → `susfs_parse_priority_app_names`) und
swapped die Bitmap bedingungslos (Z. 281-285). Läuft zwischen Snapshot und
Swap ein paralleler `susfs_set_priority_uid_filter`, der einen **neueren**
Filter installiert, überschreibt der Worker diesen still mit dem alten
Stand — der SET-Aufruf ist verworfen bis zum nächsten Renew.

**Upstream-erbter Bug:** `WildKernels/GKI_KernelSU_SUSFS`,
`.github/actions/susfs/susfs618/kernel_patches/50_add_susfs_in_gki-…patch`
(`fs/susfs.c:2103-2145`) hat exakt dasselbe Muster
(Snapshot → Parse außerhalb Mutex → Swap). Wir portieren ihn mit.

**Fix (478):** Statt Generation-Counter ein Inhaltvergleich unter dem
zweiten Mutex vor dem Swap: `strcmp(susfs_priority_app_strings, app_strings)`
— bei Differenz (oder `NULL`) kein Swap, `goto out_restore_creds`. Der
bestehende Exit-Pfad gibt `new_bitmap` per plain `kfree` frei (nie
publiziert, also kein `kfree_rcu` nötig — kein Double-Free), `app_strings`
wird `kvfree`'d, `running` via `atomic_set(…, 0)` zurückgesetzt. Vergleich
gegen den Snapshot selbst (Heap-Kopie), nicht gegen einen gerade
freizugebenden Pointer. Beide Lock-Regionen (Vergleich und Swap) sind
disjunkt — keine doppelte Mutex-Aufnahme im selben Pfad.

### #2 — `is_uid_filter_set_before` outside the lock (LOW, gefixt in 478)

`static bool` (Z. 161) wurde in Z. 207-208 außerhalb der Mutex gelesen,
geschrieben im Swap-Pfad (Z. 224-225) unter der Mutex — racey gegen den
SET-Schreibpfad, wenn auch nur log-kosmetisch. Fix: Lesen + LOGI nun unter
einer eigenen kurzen Lock-Region vor dem Parse (keine Sleeping-/Alloc-Calls
darunter).

### #3 — Hook-Scope USER-only (Known Decision, upstream-identisch)

Upstream-Abgleich: `simonpunk/susfs4ksu` ist 404 (Commits `97cbbaf`/
`2557ea2` nicht mehr auflösbar); aktuelle Upstream-Heimat ist
`WildKernels/GKI_KernelSU_SUSFS` →
`.github/actions/susfs/susfs618/kernel_patches/50_add_susfs_in_gki-…patch`
(Hook-Stellen Patch-Z. 2406-2439 bzw. 9-45). Upstream hookt exakt unsere
vier Stellen — nur `PRIO_USER` / `IOPRIO_WHO_USER`, jeweils vor
`find_user()`. Kein PROCESS/PGRP upstream.

→ **Known Decision** (AGENTS.md): Scope bleibt USER-only. PROCESS/PGRP
sind bewusst offen — Ziel-PID-Kenntnis setzt proc-Sichtbarkeit voraus, die
SUSFS separat versteckt; Abweichen würde bei jedem Upstream-Sync
Merge-Reibung erzeugen.

### #4 — Secondary-User-Normalisierung (INFO, **A/B PASS 2026-11-24**)

`uid % 100000` (Z. 317) mappt Work-Profil-/Secondary-User-UIDs (1010000+)
auf den App-ID-Bereich — gewollt (Commit 82837be27: „Sekundaer-User").
Korrekt für alle realen Android-UID-Räume; statische Prüfung OK.

**Live-A/B (Build485, Testuser 11 + `prio_ab2/prio_ab3`, 5/5 konsistent
nach Renew-Settle `sleep 1`):**
- Cross (uid 10339) → Primary gefiltert (10251 gms): **ESRCH 5/5** ✓
- Cross → Secondary gefiltert (1110251 = 11·100000+10251): **ESRCH 5/5** ✓
  — `% 100000`-Mapping greift, Bitmap-Match auf App-ID korrekt.
- Cross → Self (10339→10339): OK (by Design — `uid_eq`-Guard vor Filter).
- Root-Bypass (0→10251): OK 5/5 ✓; `setpriority` symmetrisch zu `getpriority`.
- Nicht-gefilterte Secondary (1110314): kein ESRCH-von-Filter.
- Flake-Hinweis: Tests <1 s nach `set_priority_uid_filter` liefen teils
  durch (Renew-Worker mid-swap) — mit `sleep 1` stabil.

### #5 — Fehlender SPDX-Header (INFO, gefixt in 478)

`// SPDX-License-Identifier: GPL-2.0` als Zeile 1 ergänzt (Repo-Standard,
vgl. `k6a_gov.c`).

## Verifiziert sauber (keine Änderung)

- NULL-Guards an beiden `kfree_rcu`-Stellen (Z. 228/287 vor 478) —
  Panic-Fix aus `7fc358f27` geschlossen, 4.14-spezifisch nötig
  (`kfree_rcu(NULL, rcu)` rechnet `&NULL->rcu = 0x10` aus; Upstream zielt
  auf 6.18 GKI, wo das safe ist).
- RCU-Disziplin: `rcu_dereference_protected` + `lockdep_is_held`,
  `rcu_assign_pointer`, `kfree_rcu` erst nach Mutex-Release; Aufrufer
  halten `rcu_read_lock` (`sys.c:261`).
- User-Pointer: `copy_from_user` fester Größe, NUL-Check per `memchr`
  (Z. 174), `scnprintf`-Bound-Check (Z. 87-91), `from_kuid`-Unmapped wird
  vom Range-Check abgefangen.
- Bit-Range: MIN=10000, MAX=19999, `BITS_TO_LONGS(10000)`=157 Longs,
  Index ∈ [0,9999] — kein Overflow.
- Cleanup-Pfade vollständig, kein Leak.

## Verify (478) — abgeschlossen 2026-11-22/23

- `clang -fsyntax-only` pristine-compared (Rezept `local-build-notes.md`):
  `susfs_priority.c` + `susfs_core.c` **rc=0 / 0 Errors** (nur bekannte
  prä-existente GNU-extension-Warnung cgroup-defs.h:434, identisch beide Läufe).
- CI **Run #478 success** (`45077297214`, HEAD `b14b62525`), Artefakt
  `Badazz-kernel-sweet-v4.14.369-build478-aosp.zip` (md5 `5c8cd162…`).
- Flash `~/tmp/flash478.sh` (abgeleitet mit `s/477/478/` + `s/build476/build477/`
  für die Backup-Namen): update-binary **rc=0 „flash done"**, Backups
  `boot-build477.img`/`dtbo-build477.img` (md5 + sdcard).
- Live (Boot 23:35, History `reboot,shell,1795386994`): Banner **`build478`**,
  `git_hash=b14b6252` ✓, Sniffer×3 + Follower respawn 23:36:38 ✓.
- **SET** `com.termux,com.google.android.gms` → rc=0, LOGI
  „the priority uid filter is set" + „CMD_… ret: 0" + „renewing" (neuer
  Flag-Lock-Zweig) + „is now added … 10339" ✓ — Achtung: SUSFS-Log war
  zur Bootzeit runtime-disabled; `susfs enable_log 1` vorher nötig
  (Boot-Zustand: Default `static_key TRUE`, aber KSU/ksud setzt 0 — wie 476/477).
- **ESRCH-Paar 4/4 PASS**: Root→10251 = `-20` (uid<10000-Bypass),
  Root→10339 = `-20`, Caller 10339→Ziel 10251 = **ESRCH(3)**,
  Caller 10251→Ziel 10339 = **ESRCH(3)** (symmetrisch, cross-uid only).

## Neue Befunde beim Live-Verify (nicht im Audit — eigene Baustellen)

### B6 — Renew-Trigger tot auf dem Split-Build (MEDIUM, geschlossen — Build 481)

`ksu_rename_observer` (`pkg_observer.c:61`, static) hatte **keinen Aufrufer**:
`ksu_observer_init` war Stub (`return 0`) → die `susfs_parse_priority_app_strings()`-
Call-Stellen (`pkg_observer.c:56/106`) wurden nie erreicht → **der Renew-Worker
lief nie (470–480)**. Live-Beweis478: `install-existing`/`uninstall`/
`force-stop`/`bg-dexopt-job` = 0 `renameat:`-Zeilen.

**Ursachenkette über drei Builds:**

- **Build 479 (v1, falscher Wire-Punkt)**: `KSU_LSM_HOOK_INSERT(inode_rename)`
  aus `susfs/lsm_hook.h` → Laufzeit `-EINVAL` „target_name is required".
  Ursache: Kbuild verlinkt **`hook/lsm_hook.o`** (Replace-Only-Impl, Header
  ohne `insert`-Feld/INSERT-Makro); `kernel/susfs/*` (Insert-API, `sus_path.c`
  etc.) sind **nicht eingebunden** (Kbuild kennt kein `susfs/`). Beleg dmesg479:
  `lsm_hook: hook inode_rename: target_name is required` +
  `observer inode_rename hook failed -22`.
- **Build 480 (v2, Replace, aber Data-Symbol fehlt)**:
  `KSU_LSM_HOOK_INIT(inode_rename, "selinux_inode_rename", …)` gegen die aktive
  Impl + Pre-Resolve des Ziels (`find_kernel_symbol_exact` → `original` als
  target-Hint, schließt das NULL-Fenster zwischen slot-patch und write-back),
  `-EALREADY` des `init.c:206`/`boot_event.c:35`-Doppelaufrufs als OK gewertet →
  schlug mit `-ENOENT` „failed to resolve security_hook_heads"
  (`hook/lsm_hook.c:280`): **`CONFIG_KALLSYMS_ALL` nie gesetzt** (Kconfig-Default n,
  sweet_defconfig hatte nur `KALLSYMS=y`) → kallsyms enthält nur Functions,
  `security_hook_heads` (Data) ist unauflösbar. Beleg480: `git_hash b6971145`,
  `/proc/kallsyms` zeigt `t selinux_inode_rename`, `security_hook_heads` fehlt
  (nur 1 Data-Symbol `_etext`), Event-Test (python `setuid(1000)` +
  rename `pkgs.test → packages.list` = exakter Trigger) = 0 Zeilen. Dieser
  Resolver-Pfad wurde vorher von niemandem benutzt (einziger Käufer der API =
  der Observer) — alle anderen `find_kernel_symbol_exact`-Ziele sind Functions.
- **Build 481 (v3, geschlossen)**: `CONFIG_KALLSYMS_ALL=y` in
  `arch/arm64/configs/sweet_defconfig` (GKI-konform; schließt zudem alle
  künftigen Data-Symbol-Lookups frei; Kconfig-Roundtrip bestätigt `=y`).
  **Live481-Evidence**:
  - `lsm_hook: patched inode_rename hook slot … from ffffff9e7a1fcdd0 to
    ffffff9e7b02e1b0` (t=6.91 s) + `renameat: observer chained to
    selinux_inode_rename (orig ffffff9e7a1fcdd0)` — SELinux korrekt gekettet.
  - Events: Boot-Package-Scan (t=51/60/104 s), ConkyDroid-Install (t=2161 s),
    Uninstall (t=2224 s) → `renameat: packages.list.tmp -> packages.list …`
    (Fast-Path inkl. `cached /data/system d_inode`).
  - Worker: `susfs_parse_priority_app_names` parst den kompletten
    packages.list-Stack nach **install UND uninstall** (t=2161/2234) —
    Renew-Trigger läuft seit Build481, vorher nie.
  - Regression: SET rc=0 + `renewing the priority uid filter` /
    `the priority uid filter is set`; **ESRCH-Paar 4/4 PASS** (Root
    `set=0 get=-20` für 10251+10339, Cross 10339↔10251 `set=-1 errno=3`
    symmetrisch), Nice anschließend revertiert.
  - Rest-Teststand: Filter aktiv für `com.google.android.gms`+`com.termux`
    (CLI hat kein `del` → Backlog `del_priority_uid_filter`); ConkyDroid
    deinstalliert, APK temporär entfernt.

### B7 — `susfs_init()` nie aufgerufen → Marker nie sichtbar (LOW, geschlossen — Build 479)

`fs/susfs/susfs_core.c` hatte keinen Aufrufer für `susfs_init()` (kein
initcall) — der `BuildNNN:`-Marker war nie im dmesg (auch in 476/477 nicht).
Fix in Build 479: `late_initcall(susfs_init_late)` mit int-Wrapper
(`initcall_t = int(void)`, `susfs_init` bleibt void — upstream-synced Header
`include/linux/susfs.h`/`fs/susfs/susfs.h` unangetastet). **Live**: `Build479:
susfs core vv2.4.0 synced` @t=1.207 s, Regression 480/481 identisch
(`Build481: susfs core …` @t=1.161 s). Marker-Policy beibehalten: nur
anbieten, wo der Aufrufer existiert (jetzt initcall); Build-Beweis zusätzlich
Banner + `git_hash` (bewährt).


### B8 — Renew-Gate `strcmp` scheitert ab 2 Paketen (MEDIUM, gefixt — Build 483)

**Ursache (482-Verifikation entdeckt):** Der Worker-Snapshot `app_strings`
wurde von `strsep()` im Parse **in-place mutiert** (Komma → `\0`). Das
Stale-Gate `strcmp(stored, app_strings)` verglich danach den beschaedigten
Buffer gegen den intakten Stored-String — ab ≥2 Paketen (Komma vorhanden)
**immer ungleich** → `goto out_restore_creds` vor Swap: kein Bitmap-Update,
kein `resolved again`-LOGI. Einzelpacket (ohne Komma) war unsichtbar
betroffen. **Betrifft beide Worker** (`priority` seit Build478-Stale-Gate,
`sus_path` mit dem Port in482). Ursprungsmuster upstream-erblich
(WildKernels-Gate ohne Parse-Mutation dazwischen).

**Fix (Build483, `bd8a80e4e`):** Parse auf separatem `parse_buf`-Copy,
`kvfree` direkt danach; Gate gegen **pristine** Snapshot. SET-Pfad war
nicht betroffen (stored-Copy vor Parse).

**Live483-Evidence:**
- Multi-Set rc=0 (`gms+termux+ckd` sus_path, `gms+termux` priority).
- ckd-Install-Trigger → **beide** Worker: `the uids of the … uid filter
  are resolved again` (priority pid+1, sus_path pid+2) — vorher nur bei
  Einzelpaket.
- Sichtbarkeits-A/B (Allow-List): ckd in Liste → `run-as` liest
  `susvis/f` (`secret`); raus → `ENOENT`; Shell `u:r:su:s0`/uid0 →
  versteckt (uid-Bypass greift erst ab `u:r:ksu:s0` — `runcon` dort
  sieht `ls -la` vollstaendig); `add_sus_path`-Flag lebt (Parent-Listing
  `d?????????` = Lookup-Intercept in `fs/namei.c` `-ENOENT`).
- CMD-Tausch: CLI `set_sus_path_uid_filter` → `0x55554` → `ret: 0` +
  LOGI; `DEL_SUS_PATH` liegt auf `0x55555` (Hand-Supercall, CLI hat kein
  del — Backlog unveraendert).
- Regression: B6-Kette + `Build483`-Marker + `git_hash=bd8a80e4` nach
  Reboot; SET priority rc=0; Observer `renameat`-Events booten.
- Teststand: Filter aktiv `com.google.android.gms`+`com.termux`
  (sus_path + priority), `enable_log 1`, ConkyDroid deinstalliert.
