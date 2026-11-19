# state log to build457 (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> Rollender Status-Log bis Build 457 (vor dem Docs-Umbau 2026-11-19)

## Current State (Stand 2026-11-14, Build 457 live)
- **Repo**: `vandalsquad187/BadazzKernel` branch `main` @ **`17a66600a`** (Build 457: KSU-prctl/reboot-Gates
  + defconfig-Revert von 456 — **456 bootlooped am Gerät**, Ursache = FORTIFY/INIT_STACK-CFLAGS, beide
  revertiert; siehe Build-456/457-Abschnitt unten). Kette: build441 `831efce8a` (NoMount)
  -> `456be5f7d` (GPU-OPP-Cache + kgsl_k6a_get_idx + psy-Cache)
  -> `784a55541` (CI lockdep: NoMount-Step) -> `5bdd119ac` (Build446: Plan/Commit/Latch — KGSL/Devfreq-I/O
  aus `gov->lock`) -> `cd02a1f4e` (gpu_idx_read-Return-Value-Bugfix, r8b-Fund) -> `d0fdae972` (Build450:
  hist aus status ausgelagert) -> `52917a18e` (Build452: adaptive slow-poll in GAMING) -> `489b360ce`
  (r9/r9b-Kalibrierung) -> `5622eb83e` (Build 454, flashed als build455) -> `f5257d095` (Build 456,
  Bootloop) -> `17a66600a` (Build 457, **live via Push-Run-Artifact**).
  Releases: **v4.14.369-badazz-build455** (Run 43502371855, Latest) — 455 verifiziert
  (ko **53 248 B**, git_hash/zone-mirror unten im r10-Abschnitt); 457 lebt nur als Artifact
  (Run 43640674494); davor Releases 453 (ko 69 384 B, 2026-11-13) und 451/450/449/447.
- **KGSL-I/O aus `gov->lock` (Build446, `5bdd119ac`) — implementiert + in vivo bewiesen (r8c)**:
  Plan/Commit/Latch-Muster: alle KGSL/Devfreq-Schreib-Readbacks laufen im Commit ausserhalb von
  `gov->lock` (Work-Structs `k6a_gpu_work/k6a_gpu_res/k6a_bw_work/k6a_bw_res/k6a_limits_work/.any`),
  Plan trifft Entscheidungen mit dem 1-Tick-Snapshot (`gpu_live_max/min` im Struct, Init -1),
  latch überträgt Readbacks zurück, `gpu_reset`/`gpu_floor_reset` = sync-Wrapper (enable=0/exit/
  Komatallback), `plan_release` = RELEASE-Self-Heal bei enabled=0-Ticks. Lockdep-Run grün
  (43125320613). Details unten im Scope-Abschnitt Build 446-449.
- **Kernel-Optimierungen (Build 445, `456be5f7d`, live bewiesen am Geraet)**:
  1. **GPU-OPP-Cache**: `gpu_freqs[]`/`gpu_num` im Struct, gefuellt im freq_init_worker
     (einmaliger Log `gpu levels=8 cur_idx=4` beim Fill, danach 0 weitere); lazy
     `gpu_table_locked()` + `gpu_idx_read()`-Helper; `enforce_gpu_cap()`/`enforce_gpu_floor()`/
     `gpu_floor_reset()` lesen die Cache-Tabelle + Index-Readback via neuem Export
     `kgsl_k6a_get_idx()` (kgsl_pwrctrl.c: nur Live-Indizes, KEIN Tabellen-Walk, selbe Locking)
     — Steady-State 0 OPP-Walks/Tick, deutlich weniger Zeit unter `gov->lock`.
  2. **freq_init_worker no-giveup**: vorher 30x500ms -> gold_num=0 -> Enforcement still tot
     bei spaetem cpufreq. Jetzt 30x500ms, dann dauerhaft 2s-Retry (freq_init_warned-Flag,
     Gold-Load-Log einmalig im Gold-Block).
  3. **psy-Handle-Cache** in `read_bat_temp()`: `power_supply_get_by_name("battery")`
     nicht mehr pro Tick (List-Walk unter Spinlock), Handle gecacht, bei Fehler
     (-ENODEV) Drop-and-Requery, `power_supply_put()` im exit.
  4. Host-Test 23/23 (struct power_supply-Forward-Stub in harness.c).
- **build445 geflasht + verifiziert (2026-11-11 ~22:23)**: Banner `...build445-aosp`,
  `k6a_features git_hash=784a5554`, ko **66 744 B** (kleiner als 440er-67 040 — OPP-Cache-Refactor),
  Upgrade-Pfad 441->445 sichtbar (fsck-f2fs-Linie), NoMount `22 (691)` intakt, **P7 aktiv**
  (`hist=` leer nach Boot, `hash_verified=0 hash_state=0 bt=-1` bis zum ersten GAMING),
  nach r7: `hash_verified=1 hash_state=1 bt=42` = Feed-Seed exakt wie dokumentiert.
- **r7 (2026-11-11, Loggers nicht gearmt — Verifizierung via hb-Stamps + kernel-hist,
  granularer als Logger-Logs)**: 2 GAMING-Fenster (386s->1211s ~13:45 pid 29326 hit=354;
  1222s->1907s ~11:25 pid 6074 hit=318), **miss=0 in beiden**, beide sauber `gaming>off`,
  **kein Residual** (pidof leer). Kernel-Temp-Peak 75, ctl-Peak 88 (Fenster 2 heisser).
  Caveat: `badazz_safe` setzt `gpu_cap=0` (kein Cap) -> OPP-Enforce-Pfad kaum belastet;
  hang-watch gueltig, Enforce-Stresstest = **erledigt via r8b/r8c (siehe Build-446-449-Abschnitt unten)**.
- **Hang-Serie**: r4 HANG -> r4b/r5a/r5b/r6/r7 = **5 Folgerunden hangfrei**.
- **k6a-ctl v1.6.1 released + deployed (2026-11-11)**: k6a-ctl `684b535` — P0-Fixe:
  silver-hispeed aus SILVER-Tabelle (`silver_at_pct 90`, vorher gold_at_pct 90 ≈ 2,07 GHz
  > SILVER_MAX 1,8 GHz -> schedutil-Snap auf Max), stale-recovery ko-Suche in
  service.sh-Reihenfolge (/data/adb -> /system/lib/modules -> /vendor), bw-Bound 30000
  (Kernel-Paritaet), `_load_gov` grep-a-Fallback ohne strings, Controller-Case 1.10+
  (`1.[5-9].*|1.[1-9][0-9].*`). GOV_KO_VER bewusst BEHALTEN (check_module.sh-Gate braucht es).
  Gate OK 0 warns, Release-Zip 53 268 B, 5 Dateien deployed (md5-Doppelcheck), Restart ohne
  Reboot. **Hotfix `c0b8a81`**: Start-Banner hatte hartes `v1.6.0`-Literal (684b535 vergass
  den Bump) -> liest jetzt `_MODVER` aus module.prop. **Lektion nc/PATH**: manueller
  service.sh-Start aus Termux-SU verliert den KSU-PATH -> webui `nc: not found`, 2s-Restart-
  Loop; Fix: `PATH=/data/adb/ksu/bin:/system/bin:...` beim manuellen Start setzen.
- **CI-Funde der 456be5f7d-Kette**: lockdep.yml hatte den "Integrate NoMount"-Step NICHT
  (build-kernel.yml hatte ihn) -> olddefconfig brach an `fs/Kconfig:323: can't open file
  fs/nomount/Kconfig` — VORgelagerter Bug aus der NoMount-Integration, NICHT durch die
  Kernel-Aenderungen. Fix `784a55541` (gleicher Step + 3 Gates wie build-kernel.yml).
  **GitHub-Artifact-API-Outage** (2026-11-11 ~20:2x): upload-artifact scheiterte beide Legs
  5x (`Unexpected token '<', "<!DOCTYPE "...`), Builds/ZIPs selbst gruen, Run blieb zombie-
  in_progress (cancel-API: "completed", status-API: in_progress, rerun --failed blocked
  "already running") — Ausweg: frischer Dispatch (43099154854) lief durch.
- **NoMount-Integration (Build 441, geflasht + verifiziert 2026-11-11)**:
  - **Was**: NoMount VFS path-redirection subsystem, dev `8f05de7` (NOMOUNT_BASE_VERSION 22,
    commit count 691), built-in `CONFIG_NOMOUNT=y` (sweet_defconfig neben KSU/SUSFS-Block).
    Integriert via `kernel/setup.sh` (pinned): Symlink `fs/nomount` -> `NoMount/kernel/src`,
    `fs/Makefile` `obj-$(CONFIG_NOMOUNT)`, `fs/Kconfig` source vor dem aeusseren endmenu (Zeile 323).
    **NoMount-Clone + Symlink bleiben UNTRACKED** (nested .git = kaputter Gitlink) — der CI-Step
    rendert beide frisch.
  - **CI (build-kernel.yml)**: Step "Integrate NoMount" (Schritt 1b, vor Configure) mit
    `NOMOUNT_REF=8f05de7…df69` + 3 Gates (nomount.c, Makefile-Zeile, Kconfig-Source);
    `CONFIG_NOMOUNT=y` in REQUIRED_CONFIGS; Post-Build-Check `fs/nomount/nomount.o` (61 448 B).
  - **Am Geraet bewiesen**: dmesg `[0.53s] Key type nomount registered` + `NoMount: Loaded
    successfully`; Metamodule v2.2.0 (605, `/data/adb/modules/nomount`) macht Boot-Hijacks
    @8.9 s (Superblock 0xfd00029/0/1, parent dirs ino 665/315, xattr handlers) = **ABI
    v2.2.0-Userspace <-> dev-Kernel kompatibel**; `nm version` -> `22 (691)` via `su -c`
    (funktioniert — die alte CapBnd=0-Sorge gilt hier NICHT); **`/proc/mounts`: 0**
    Modul-/Overlay-Mounts (rezygisk/brene/tricky_store laufen rein per VFS-Injection);
    brene uname-Spoof unveraengert; k6a/KSU/susfs intakt (`git_hash=831efce8` via
    k6a_features, k6a_gov v1.7.2, KSU 38532); 0 Oops/Panic (7 Grep-Hits = ramoops-Wortfunde).
  - **Fallbacks/Trivia**: ohne konfigurierte Regeln inert (`fs_initcall` registriert nur den
    "nomount"-Key-Type, `add_key`-Kontrollkanal braucht CONFIG_KEYS=y+KEYS_COMPAT=y — beide da).
    `add_key` aus App-UID (Termux ohne su) wird von Android-seccomp mit SIGSYS gekillt
    (Python-ctypes-Probe starb mit "Unknown signal 31") — Beweisfuehrung immer via dmesg/`nm`.
    dmesg-Anker: `Key type nomount registered`, `NoMount: Loaded successfully`,
    `NoMount: [DEBUG] … hijacked`.
  - **Upgrade-Pfad**: naechstes Release-Tag (v2.3.0, sobald dev-Änderungen drin) → `NOMOUNT_REF`
    in CI-Step + lokal `bash -s <tag>` neu ausfuehren; setup.sh ist idempotent (grep-guards).
  - **4.14-Kompatibilitaet (vorab gegen torvalds v4.14 verifiziert)**: `full_name_hash(salt,…)`
    3-arg (fs/namei.c:1921), `swap()` (kernel.h:917), `d_u.d_alias` hlist,
    `take_dentry_name_snapshot` (<5.3-Guard), `iterate_shared` (4.7+), `dentry_open(cred)`,
    `DCACHE_OP_WEAK_REVALIDATE`, xattr-`__get`-Split per Makefile-Autodetect aus,
    `get_user_pages_fast` FOLL_WRITE als write=1.
- **Repo**: `vandalsquad187/BadazzKernel` branch `main` @ **`deeac297f`** (build440-Merge, P7 insmod-STARTS-OFF).
  Kette: build438-Merge `e120f655` (v1.7.2: P1 gold-cpu-Cache, P2 Lock-Fenster, P3 cap-Log-Diet,
  P6 cool_set enabled-Gate, P6b battery_guard_tick unsequenced-UB) -> `777c5f15b`
  (P7 enabled=false bei Probe) -> `deeac297f` (Merge, CI gruen, Release build440).
- **Build 440 geflasht + verifiziert (2026-11-11)**: banner `...build440-aosp`, ko 67 040 B
  in `/data/adb` UND module-dir, ctl v1.6.0 -> DAILY clean. **P7 in vivo bewiesen**:
  `hist=` LEER nach Boot (vorher: auf JEDEM Boot ein ~27-s-GAMING-Window
  `off>gaming@~12.8 s` -> `gaming>off@~40 s` vor dem ersten ctl-Kontakt) — Kernel
  bleibt OFF bis k6a-ctl enable=1 schreibt.
- **P7-Nebenwirkung (bewusst, benign)**: Feeds/Seed initialisieren spaeter:
  `hash_state=0`, `bt=-1`, `temp=0` direkt nach Boot — der bt-live-Feed
  (`Build434: bt live`-Banner) und das State-Hash seeden erst beim ERSTEN
  GAMING-Eintritt (den es jetzt ohne Boot-Window nicht mehr sofort gibt).
  Akzeptanz-Check naechste Runde: hash_state=1 + bt-live-Banner beim ersten GAMING.
- **Build 438 geflasht + verifiziert (2026-11-09)**: banner `...build438-aosp`,
  `k6a_features git_hash=e120f655`, ko 67 040 B in `/data/adb` UND module-dir,
  `k6a_gov v1.7.2 loaded (legacy=1)`, status `hash_verified=1 hash_state=1
  build_hash=e120f655`, schedutil cpu0/4/6/7 hl=90/hf=0, ctl v1.6.0 -> DAILY clean.
  **P6 in vivo bewiesen**: cdev `k6a_gov` (cooling_device11) — `cur_state=3`-Write
  bei `enabled=0` -> `state=off` BLEIBT off (vorher: GAMING-Label-Ghost des r3/-
  "letzter GAMING-Write-Race"), Restore cur=0 sauber.
- **Neue Funde der build438-Kette**:
  1. **CI-S4-Fund (P6b)**: host-test schlug in CI (gcc) 3x S4-Battery-Latch, lokal
     (clang) 23/23 5x gruen — Root Cause: C-UB in `battery_guard_tick()`:
     `battery_guard_apply(read_bat_temp(&bt) == 0, bt)` schreibt `bt` (1. Arg)
     und liest es (2. Arg) im selben Ausdruck — argument evaluation order ist
     unsequenced (UB), gcc waelhlt anders als clang -> bt war Garbage -> Latch
     blieb aus. Fix: `ok`/`bt` vorab sequenzieren. **Lektion**: Calls mit
     Out-Params immer in Variablen sequenzieren, nicht inline in der Arg-List
     koppeln; nur weil lokal clang gruen ist, ist die Semantik nicht bewiesen
     (gcc-Beweis via CI).
  2. **miui "Install toolchain"-Hang** (dispatch-run 42695863799): miui leg hing
     >15 min still im Toolchain-Download-Step, aosp leg success — cancel +
     re-dispatch (42698349846) -> beide Legs + Release gruen. Resueme: toolchain-
     Flakes sind mit cancel+redispatch behebbar, kein Workflow-Defekt.
  3. **Release-Automatik bestätigt**: Push nach main (PAT) -> Build+Artifacts
     OHNE Release (release-Job `if: workflow_dispatch`); dispatch-RUN erzeugt
     Tag `v4.14.369-badazz-build438` mit aosp/miui/SHA256SUMS;
     `build-info.txt`: `K6A_GOV_VERSION=1.7.2`, `BUILD_NUMBER=438`,
     `COMMIT_SHA=e120f655…`; ko vermagic `...build438-aosp`.
- **Offen**: r4-Hang (mid-ROUND SYSTEMHANG 2026-11-09, kein Panic gerammt) bleibt in
  Beobachtung. Stand nach r7: r4b/r5a/r5b/r6/**r7 OK = 5 Folgerunden hangfrei**; das
  Hang-Muster reproduziert NICHT (r6+r7 sogar ohne Residual-Prozess). Monitor miss=0 in
  r5a/r5b/r6/r7. Kernel-Throttle feuerte nie (throttle_events=0). Verblieben: OPP-Enforce-
  Stresstest (Cap aktiv, `badazz_safe` cappt `gpu_cap=0`), Roadmap-Posten (KGSL-I/O aus
  `gov->lock`, status_compact, adaptive GAMING-Polling, webui-Backoff). Details:
  `~/tmp/k6a-ctl-buglog.md` r5-Phase + r6/r7 + v1.6.1-Rollout.


- **Repo**: `vandalsquad187/BadazzKernel` branch `main` @ **`7dbd5ce70`** (susfs round 2).
  Kette: `1639f9514`/`8116a7c4a` (KSU 414/416-Fixes) -> `6355e9f47` (virt_addr_valid-Regression
  revertiert, 416) -> `808e27158` (susfs namei refcount, 419) -> `3db83cdd4`/`6681496af`
  (fork atomic_or, gitlink) -> `c82afac10` (v1.7.0, 421) -> `83a38074b` (AGENTS: Gate-Falle)
  -> `bedfc232b`/`904a9bd29` (Host-Test + exec-bit) -> `e5d21b884` (SHA-Pinning+dependabot)
  -> `f385b26e6` (susfs thin-core + out/ enttracked, 426) -> `7dbd5ce70` (round 2, 428).
  Fork `dev-upstream-merge` @ **`79003ef8`** (DEL_SUS_PATH-Sync). Releases **NUR via
  `gh workflow run build-kernel.yml`** (dispatch-only); Push-Runs bauen ohne Release,
  `paths-ignore: ['**.md','docs/**']`; nur aosp-ZIP flashen (Geraet=crDroid); Releases/
  Tags nur mit explizitem OK.
- **Neu in CI (2026-10-31)**: Alle `uses:`-Stellen auf Commit-SHAs gepinnt (checkout v4.4.0,
  cache v4.3.0, upload-artifact v4.6.2, download-artifact v4.3.0, gh-release v2.6.2) +
  `.github/dependabot.yml` (wöchentlich, gruppiert); erster SHA-Run grün beweist
  Verhaltensidentität. **Host-Test** (`tools/k6a_gov_host_test/`, 23 Checks 23/23 grün
  lokal+CI, `host-test.yml` nur bei k6a_gov/test-Änderungen, `contents: read`) —
  Validiert State-Machine/Hysterese/L4-Flapping/Battery-Guard-Latch/Cap-Rampen gegen den
  ECHTEN k6a_gov.c-Code (Laufzeit-Extraktion), Zukunft: jede k6a-Änderung vor dem Flash
  geprüft. `out/` enttracked + gitignored, `susfs*.v210` Altlasten weg.
- **Geraet**: **build428** (Latest) geflasht + verifiziert 2026-10-31: root OK (`u:r:ksu:s0`),
  k6a_gov **v1.7.0**, susfs-Init `git_hash 7dbd5ce7`. **SUSFS-KSU-Domain-Ausnahme am
  Geraet bewiesen**: KSU-Shell SIEHT `/system/addon.d` (vorher blind), App-Shell ENOENT;
  300x sus_path-Stress = 0 Warnings. v1.7.0-Nacht-Stress bewiesen: 9 h GAMING-Residency
  (32.7M ms) ohne Oops, L4 nie beruehrt.
- **k6a-ctl v1.4.6 -> P1+P4-Haertung (2026-11-01, deployed)**: P1 =
  `kver_ok()`-Hartgate (`1.5.*|1.6.*|1.7.*`-Liste) -> dynamisch
  `1.[5-9].*` — Versions-Bumps kosten fortan KEINE service.sh-Aenderung mehr;
  GOV_KO_VER nur noch Log-Nennung, Boot-Beweis am 01.11. 15:08:19 ("k6a_gov
  1.7.0 geladen" ohne Warn). P4 = Stale-Watchdog in k6a-controller:
  (state_age_ms|temp|state)-Tupel 5 Ticks eingefroren bei delegiert+GAMING ->
  enable=0 -> einmal rmmod+insmod+Re-Apply(profile/thresholds/batt)->enable=1;
  Misserfolg bleibt aus (60-Tick-Heartbeat-Log). Trigger-Grenze 5 Ticks
  (User-Votum). Controller-restart ohne Reboot via kill -TERM + service.sh-
  Watchdog-Respawn ✓. **2 Deploy-Fallen geloggt**: (a) `<<<`-Here-Strings sind
  Bashismus — Modul laeuft mksh (`printf ... |` statt <<<); (b)
  `cur=$(a)|$(b)` geparst als assignment+PIPE (leeres stdin) statt
  String-Konlkat — ausloester: Timestamp-Leer-Trigger + unnötiges
  rmmod+insmod (als FALSE-POSITIVE ausgelöst, ~15 s episoner Schaden,
  Recovery-Pfad dafuer END-TO-END bewiesen: rmmod+insmod+Re-Apply+enable=1
  ok). Backups: service.sh.bak-v146-dyngate,
  bin/k6a-controller.bak-v146-p4watchdog. Lokale Referenzen ~/tmp/k6actl/.
- **USB-Cleanup-Idle-Urteil (offen bis unplugged)**: dmesg-Wachstum
  ~160 Zeilen/min WAHREND des Ladens = Charger-FG-Spam ([bq27z561]/
  QCOM-STEPCHG/QG-K, bewusst belassen) — kein USB-Breadcrumb-Rest. Der
  Reviewer-Check (30-min-Doppelsample, Idle < einige hundert Zeilen/h) muss
  UNPLUGGED laufen. KernelSU-`sys_execve: su found`-Zeilen = Eigenrauschen
  der Termux-Diagnose.
- **USB-/Charger-Review-Runde (2026-11-01, abgeschlossen)**: Reviewer-Patch
  `usb_debug_cleanup.patch` (7 Dateien) — Hot-Path-Breadcrumbs dev_err/pr_info ->
  dev_dbg/pr_debug (echte Anomalien bleiben Fehler-Level), aw8622x-Haptik-Build330-Logs
  raus, f_fs %px -> %pK (44 Sites), Heartbeat `b332_hb_fn` standardmaessig AUS +
  deferrable (Opt-in `dwc3.b332_hb_ms=500`); ~170k Wakeups/Tag + dmesg-Uberschrift in
  2 h laufen jetzt an DYNAMIC_DEBUG=off leer. Ausgespart bewusst: 11 smb5-lib
  Charger-IRQ pr_info + eine Build313-Dump-Zeile. **SNK-only-Tradeoff in README**
  (USB-OTG/Reverse-Charging weg). **Worktree-Falle geloggt**: der Patch lag bereits
  uncommitted im Tree (verlorene Session) — nach Apply-Konflikt erst
  `git diff HEAD -- files | grep Muster` gegen Patch-Inhalt pruefen, nicht
  blind -R annehmen. Post-Flash-Checks: `dmesg | wc -l` 30 min (Idle-Ziel < einige
  hundert Zeilen/h), Laden/ADB/Enumeration sauber, ggf. Breadcrumbs via
  DYNAMIC_DEBUG-Testbuild.
- **Blackout 2026-10-31 (diagnostiziert)**: Nach Nacht mit offener CODM-Lobby — Display
  blieb schwarz, Inputs tot, Force-Reboot noetig. Log-Forensik (`cod_run.log`-Gaps):
  Kernel 10 h STABIL (5-s-Loop schrieb lueckenlos), kein Panic/Ramoops-Content, letzte
  Suspend-Pause 00:04-00:24, danach Wachhalten durch Lobby-Wakelocks (~10 h, Dozings
  05-06 Uhr, ab 06:17 gar keins mehr). **Verdikt**: fehlgeschlagener Display-Resume
  (MIUI-Flake) nach ueberhitztem wachem Geraet — KEIN Kernel-Regression (k6a_gov/susfs
  beruehren Display/PM nicht). **Lektion**: CODM vor dem Schlafen beenden; Lobbys
  halten das Geraet wach (thermal peak 74 °C ueber Nacht). Ramoops-Leseversuch via
  sus_path-Hiding (AI laeuft: console-ramoops ist sus_path) — Bind-Mount-Umgehung
  scheiterte auch an find/filldir-Hooks.
- **KSU-Regression build414 (Root Cause + Lektion)**: `virt_addr_valid`-Guards (mein
  "toten !page-Check ersetzt"-Fix) killten alle Syscall-Table-Hooks — auf arm64-4.14 deckt
  `virt_addr_valid` nur die **Linear Map** ab, `sys_call_table`/`ksu_syscall_table` liegen
  im **KIMAGE** (unter PAGE_OFFSET) → Guard immer FALSE → KSU "nicht unterstuetzt",
  Manager-UID-Erkennung tot, alles EPERM. `__pa()` loest KIMAGE korrekt auf
  (`__kimg_to_phys` via `kimage_voffset`). **Lektion**: Hook-Install-Refactors niemals
  "theoretisch tot"-erklären und bündeln — einzeln flashen/verifizieren, mit Root-Diag
  beweisen.
- **Kernel**: `4.14.369`, KSU-Next **built-in, 4.14 direct-patch mode**, **k6a_gov v1.6.3**
  (`CONFIG_K6A_GOV=m`, sweet_defconfig:208 — `.ko` in der ZIP, insmod durch k6a-ctl).
  GPU-Tabelle live 800/650/565/430/355/267/180/140 (idx 0-7; k6a sieht 0-6); KGSL-Floor-
  Interplay `min_t(thermal_pwrlevel_floor=4, user_min)` (live verifiziert, siehe
  gpu_floor-Abschnitt unten).
- **k6a_gov Fixes 1-10 (2026-10-27, Commits `f48a2d428` + `a462761d0`)**:
  1. Gold-Clamp respektiert `user_policy.max`-Release (`enforce_max_freq()`).
  2. `enable_store(0)` ruft zusaetzlich `gpu_floor_reset()`.
  3. `gpu_floor_reset()` cleared `saved_min=-1`; `enforce_gpu_floor()` schuetzt den gemerkten
     User-Min beim Re-Cap.
  4. Battery-Guard: `read_bat_temp()` (int*-Signatur) — Fehlerfall sauber 0 statt
     Fake-Temp; `battery_guard_tick()`-Helper, vom `temp_valid` entkoppelt (gov_thread
     haelt LOCK).
  5. Init-UAF: `schedule_delayed_work` erst nach `kthread_run` (err_cooling-Pfad).
  6. `game_pid_store`: `kstrtoint` + `PID_MAX_LIMIT`-Bound; `game_pid=` jetzt in `status`.
  7. `static struct k6a_gov *gov`; module params 0644 -> 0444; `pr_fmt(fmt) "k6a_gov: " fmt`
     (Erste-Zeile-Define, 7x Praefixe aus den Strings entfernt).
  8. L4-Flapping: `#define K6A_L4_MARGIN 3` — Rueckstufung nur wenn `tp + 3 < cd_l4_temp`.
  9. `profile=0/off`: force-OFF sofort — `state_machine()`-Head plus direktes
     `set_state_locked(K6A_OFF)` im `profile_store()`, unabhaengig von `thresholds_user`.
  10. GPU-Cap: neu `s32 gpu_cap_saved_max` (User-`max_pwrlevel` vor dem ersten Cap merken);
      `gpu_reset()` restored das User-Max (vorher starr idx 0), Fail -> Retry next tick;
      `enforce_gpu_cap()` released, wenn `user_max >= best`; `k6a_gov_exit()` ruft
      `gpu_reset()`.
  - **r60-Serie (Blackscreen, 2026-10-27)**: r60f + r60g Crash mit Sampler, 3 clean ohne,
  r60i nur Match-Tail, **r60j = Voll-Datensatz** (1396 rows ueber die vollen 1500 s,
  Sampler 7854 s vor dem Match armiert, Fan-ON): GPU-Peak 800 MHz, maxbusy 99 %, MAXTEMP
  83 °C, Akku -24 %/25 min, klog sauber (nur Boot-`a615_zap`-Load) -> **Sampler exonerated**;
  der Blackscreen bleibt **intermittierend** (Power/PMIC-Glitch-Verdacht, Szene-Hypothese
  tot). r60j = die Fan-ON-Baseline. **Fan-OFF-Redo pending** — r60k x3 / r60l / r60m / r60n
  gingen durch Beobachter-Toede verloren.
- **Beobachter-Hardening (2026-10-27)**: Hintergrundprozesse aus dem Termux/su-App-Baum
  sterben zufaellig nach ~30-550 s, auch setsid/nohup mit PPID=1 (Android 16/SDK 36,
  Phantom-Killer); Settings-Knobs (`settings_enable_monitor_phantom_procs=false`,
  `max_phantom_processes=2147483647`) gesetzt aber wirkungslos. Belege: klog-Daemon-Ende
  bei uptime 5402 (`klog_r60.log`), logcat-Stand 25 585 812 B (`lc_r60.log`), Sampler-Toede
  r60d ~320 s, r60i 154 s, r60k 176 s/212 s, r60m 189 s, r60n 144 s. **Loesung: KSU-Modul
  `/data/adb/modules/badazz_iowatch/`** — ksud/Boot-Spawn ist killer-immun (k6a-ctl
  service.sh lebte Stunden). `service.sh` v4: mkdir-Lock-Dedupe
  (`/data/local/tmp/.io_watch_lock`), sleep-2-Tick, Spawn mit `&`; liest `.io_samp_env`
  (MAXWAIT/DUR/OUT) + `.io_arm_flag` -> `io_samp4.sh`. Verifiziert nach Reboot: 1 Watchdog
  (pid 1812, ppid=1), Sampler pid 29319 (ppid=1812). Mitten-in-Session-Spawns bleiben
  killbar — nur der Boot-Spawn ist immun; bei Bedarf Reboot vor dem naechsten Run.
- **Sampler-Politik**: `io_samp4.sh` (zero-fork-Heartbeat-Trigger, env-Datei
  `.io_samp_env`, frischer OUT pro Run, laeuft zwischen Runs weiter), **DUR=1500**
  (User-Direktive), **Discharging-Pflicht** (Kabel raus vor dem Arm), Luefter AUS fuer den
  Fan-OFF-Redo-Zweig. Alte r50-r59-Welt (`fps_samp2.sh` capped-Variante, `sf_probe.sh` v2,
  `arm.sh` 1560-s-Fenster; **nie `fps_samp2.sh stop`** — trunciert `fps.log`) pausiert.
- **A17**: Audit 2026-10-23 (Abschnitt unten) — 4.14 bleibt Legacy-Device-Path; SUSFS-
  Uname-Spoof und susfs-ABI v2.3.0 gueltig. **Tester-Kit pending** (`/sdcard/Download/cr17/`).
- **KSU-Next carry-over**: Submodule `79003ef8e8` (v3.3.0-311, fork-Kette s. Current State), UAPI=6,
  Runtime 35000+revcount (build387 = `38523`). su-Flow: `/system/bin/su` -> sucompat-
  Redirect `/data/adb/ksud` -> preexec-Escape (`ksu_preexec_ksud_escape`, beide Pfade) ->
  `escape_with_root_profile()`; das `kern_path("/data/adb/ksud")` laeuft unter
  `override_creds(ksu_cred)` (DAC durch `/data/adb` 0700). GRANT_ROOT: `syscall(142
  reboot-magic)` -> ksu-fd -> `ioctl(fd, _IO('K',1) /*KSU_IOCTL_GRANT_ROOT*/)`, Termux ist
  allowlisted. ksud `/data/adb/ksud` md5 `f3eacfd0` (v3.4.2-50-gb427a3f9, Context
  `u:object_r:ksu_file:s0`); Daemon laeuft NICHT (built-in-Modus), `post-fs-data` nicht
  manuell nachfahren. Features ENABLED: su_compat/kernel_umount/sulog/adb_root/
  selinux_hide; DISABLED: shell_su_adb_only. Crown-Bits 26-30 (`include/ksu.h`), dmesg-
  Archiv `/sdcard/Download/dmesg_b385.txt`.
- **On-device-Operativa**: `cp` unter escaped-root-cred schreibt NULLen -> immer
  `cat src > dst` + `md5sum` + `chmod 755` + `chcon u:object_r:ksu_file:s0`. `ksud install`
  nie aus `/data/adb/ksud` selbst. `su` nur in Termux' mount-namespace; absolute Pfade
  im `su -c`. SUSFS spoofs `uname` (`6.12.0-android16-...`) — Build-Beweis = dmesg-Banner /
  `BuildNNN:`-Marker.
- **USB-Faults**: alle vier geschlossen (Build 336/338/348-Kette).
- **User speaks German**; device reports in German; User flasht/rebootet selbst (aosp-ZIP),
  meldet Build-/CI-Status.
