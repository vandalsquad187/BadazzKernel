# build scopes and rounds (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> Build-Scope-Reports 350-462 + Runden-Berichte r8-r16 (chronologisch)

## Build 350 Scope (Hygiene, 2026-10-12)

Bewusst **kein** funktionaler Kernel-Eingriff: es gibt für die Stalls keinen
belegbaren Config-Defekt (siehe Stall-Ursache oben). Scope:

| Änderung | Datei | Begründung |
|---|---|---|
| Tote `thermal_floor`-Sysfs entfernt (105 Z.) | `drivers/gpu/msm/kgsl_pwrctrl.c` | Schnittstelle `/sys/kernel/k6a_gpu_thermal/thermal_floor/{temp_low,temp_high,levels}` hatte **0 Konsumenten** im Tree — Schreiben dorthin tat nichts. Tuner-Falle. Der echte GPU-Cap-Pfad ist ausschließlich `kgsl_k6a_set_max_level_idx`. Bonus: entfernt einen vorbestehenden clang-only Fehler (`device_initcall`). |
| `CONFIG_TASK_XACCT=n`, `CONFIG_TASK_IO_ACCOUNTING=n` | `sweet_defconfig` | Rest-Accounting nach build349 (`TASK_DELAY_ACCT` aus). `/proc/uid_stat` ist auf modernem Android tot; `/proc/pid/io` (BatteryStats-IO) geht verloren. `TASKSTATS` bleibt an. Nichts `select`et die Optionen. |
| `CONFIG_KSM=n` | `sweet_defconfig` | `ksmd` scannt dauerhaft (`KSM_RUN_MERGE` ist 4.14-Default) — Hintergrund-CPU ohne Messgewinn. Nichts `select`et/`depends on` KSM; Verbraucher sind `#ifdef`-geschützt. |

Nicht angefasst (bewusst): `HZ_1000`, `PREEMPT`, `NO_HZ_IDLE`, `PSI`,
`KPROBES`, IO-Scheduler/`SCSI_MQ_DEFAULT`/`BFQ`/`BLK_WBT`, `MEMCG`, `THP`,
Governor-Wechsel, ZRAM-Writeback, `IKCONFIG_PROC` (SUSFS-Ziel), `PERF_EVENTS`,
und `cd_l2_gpu_max` (kein belegter Nutzen, siehe oben).

Syntax-Check (pristine-verglichen, clang 21.1.8): Pristine 1 Warnung + 1 Fehler
(`device_initcall`), geändert 1 Warnung (identisch, Zeile 1970) + **0 Fehler**.
Nach `sweet_defconfig` + `olddefconfig` bleiben alle drei Optionen draußen;
  `PSI=y K6A_GOV=m KSU=y TASKSTATS=y` unverändert.

## Build 351 Scope (bw_apply-Bugfix, 2026-10-17)

Externer Code-Review (§1–§4) wurde vollständig verifiziert; umgesetzt nur der
einzige belegte funktionale Defekt — ein Einzeiler:

| Änderung | Datei | Begründung |
|---|---|---|
| `bw_apply`: `if (!k6a_devfreq_get_bw(...) && mn != want)` → `if (k6a_devfreq_get_bw(...) \|\| mn != want)` | `drivers/thermal/k6a_gov/k6a_gov.c` | Vorher wurde die Korrektur **übersprungen, wenn der Read-Back fehlschlug** — exakt in der Situation, in der der tatsächliche Wert vom Soll abweichen kann. Mit `\|\|` wird bei Read-Fehler **oder** abweichendem Wert jederzeit re-applied. Marker `Build351:` auf den gezeigten Zeilen; `K6A_GOV_VERSION` bleibt 1.5.0. |

Nicht umgesetzt (bewusst, mit Begründung im Review-Diskurs):
- CI-Hygiene (UAPIv2-Doppel-Trigger, `LOCALVERSION`-sed, Trigger-Branches) —
  verifiziert, aber ohne Kernel-Funktion; als nachgelagerter
  P1-Hygiene-Punkt notiert.
- `thresholds_user`-Persistenz (§2, Reset bei `insmod`) — Designfrage,
  separat zu entwerfen; `k6a-ctl` bleibt bis dahin der Setter.
- `CONFIG_ENERGY_MODEL` (§5) — ohne Beleg, dass eine nicht existierende
  Energie-zu-Takt-Zuordnung die Stalls ändert; bleibt offen.

Auswertungswerkzeug (nicht Repo-Inhalt): `~/tmp/validate.py` implementiert die
Robustmetrik aus „Verbindliche Methode" oben — `med180` + `p60`, v>0-Filter,
Fenster 180–570 s; verifiziert an r29–r45.

Syntax-Check (pristine-verglichen, clang 21.1.8, beide Dateien mit
`-DK6A_GIT_HASH/-DK6A_BUILD_HASH` versorgt — ohne diesen Define bricht der
Header mit `#error`): **beide rc=0, 0 Fehler, 0 Warnungen.**

## Build 352 Scope (GAMING-Schwellen 96/99/102, CI-Hygiene, 2026-10-18)

Vorgeschichte (r46–r54, alle `build351`, NOFAN, Discharging, eine Runde
pro Messung über `arm.sh`): Re-Baseline gekappt r46/r47/r48 → med180
62,5/58,3/63,0 (Ø 61,3, s≈2,47 ≙ 348er-Cluster 2,25) = **build351
verhaltensidentisch zu build348**. Block I Kappe AUS (stock thresholds
93/96/99/90) r49/r50/r51 (UC-A/B/C) → med180 60,5/66,5/62,0 (Ø 63,0),
SoC max **95 °C** in allen drei, k6a eskaliert selbst in `cd_l2`
(20–39 % der Samples, p6 fällt auf 1708800) — „Kappe aus" ist in vivo
nur halb aus, der Governor reimplementiert die Kappe über die Schwellen.
Block II: ein `sysfs`-Write `echo "96 99 102 93 1708800 1555200
1324800" > /sys/kernel/k6a_gov/cd_thresholds` (Kappe bleibt AUS),
r52/r53/r54 (TH-A/B/C) → med180 **70,2/74,5/77,0** (Ø 73,9), Täler **0**,
k6a **100 % gaming**, p6 **100 % auf 2304000**, tp max identische 95 °C.
Der FPS-Gewinn (+10 bis +13 med180, konsistent in 3 Läufen) kommt
ausschließlich daher, dass die Runde `gaming` nie verlässt — die alte
93-°C-Schwelle (l2) war die eigentliche Bremse, nicht die p6cap-Kappe.
r44 (75,7) ist damit entschlüsselt: erster „TH-artiger" Lauf, in dem das
CD-Fenster nie antrat.

| Änderung | Datei | Begründung |
|---|---|---|
| GAMING-Profil `cd_l2_temp,cd_l3_temp,cd_l4_temp,cd_recover` `83,86,90,78` → **`96,99,102,93`** | `drivers/thermal/k6a_gov/k6a_gov.c` (profiles[]) | Bloßes heben der l2-Schwelle von 93 auf 96 hält k6a die ganze Runde in `gaming` (r52–r54: Krone aus 3× 100 % `gaming`, 0 Täler). Gold-Caps der l-Schwellen (1708800/…) unverändert — sie werden nun schlicht nie scharf. `K6A_GOV_VERSION` 1.5.0 → **1.5.1**. |
| Trigger-Branches `[main, 23.2, ksu-next-fix-100]` → `[main]` | `.github/workflows/build-kernel.yml` | `23.2`/`ksu-next-fix-100` sind tote Test-Branches; Doppel-Builds/Release-Rennen. |
| `LOCALVERSION`-sed robuster: `sed -E 's/^CONFIG_LOCALVERSION=.*/…buildNNN"/'` + `grep -qF buildNNN`-Verifizierung (fehl → `::error::`) | `.github/workflows/build-kernel.yml` | Vorher schlugen beide sed-Steps stumm fehl (`\|\| true`), wenn `olddefconfig` die Zeile schon verändert hatte; Verify erst durch Hint-`grep`. |
| `main.yml` (Legacy-Telegram-Build) mit `if: github.ref == 'refs/heads/main'` versehen | `.github/workflows/main.yml` | Jobs gegen Arbeits-Branches feuern sonst bei jedem Push (UAPIv2-Doppeltrigger-Befund Z246/264). |

k6a-ctl bleibt Persistenz-Layer: `/data/adb/modules/k6a-ctl/config/
settings.conf` schreibt `cd_thresholds=…` beim Boot — bisher `93 96 99
90…`. Die new GAMING-Defaults greifen nur, wenn der ctl-Write entfällt
oder auf die selben Werte gesetzt wird; **ctl-Config wird separat
angereicht** (auf Gerät: `96 99 102 93 1708800 1555200 1324800`).

Nicht umgesetzt: `thresholds_user`-Persistenz im Kernel (Designfrage,
k6a-ctl bleibt der Setter), `CONFIG_ENERGY_MODEL` (unbelegt), CI-
Doppeltrigger der UAPIv2-Sektion (siehe oben, nun mit `main`-Gate).

Syntax-Check (pristine-verglichen, clang 21.1.8, beide Dateien mit
`-DK6A_GIT_HASH/-DK6A_BUILD_HASH` versorgt): **beide rc=0, 0 Fehler,
0 Warnungen.**

## Build 353 Scope (Exit-BW-Floor-Fix, cool_set-Clamp, 2026-10-18)

Anlass: externe Strukturanalyse von `main @ 8adaa6a0f` (Build 352) meldete
4 Befunde; 3 verifiziert, 1 veraltet.

| Befund der Analyse | Verifizierung | Status |
|---|---|---|
| 1. `k6a_gov_exit` gibt BW-Floors nicht frei (`k6a_gov.c:~1370` — `kgsl_k6a_set_max_level_idx(0)` + `apply_policy()` ja, aber kein `reset_bw_floors()`; auf rmmod (Version-Lock-Pfad von k6a-ctl) bleiben `gpubw`/`cpu-llcc-ddr-bw` gelatcht) | **bestätigt** — gleicher Bug-Typ wie v1.3.1 „Cap wird nie freigegeben", nur Exit-Achse | **Fix umgesetzt:** `reset_bw_floors();` vor `cancel_delayed_work_sync()` |
| 2. k6a-ctl schreibt alte Schwellen `93 96 99 90` beim Boot und überschreibt die neuen Kernel-Defaults | **veraltet** — seit k6a-ctl v1.4.1 (`0d6dece`) ist `settings.conf` auf `96 99 102 93 …` und der Boot-Write idempotent; auf Gerät live verifiziert (`cd_thresholds=96 99 102 93`, `thresholds_source=user`, einziger Boot-Write in service.log) | keiner |
| 3. `cool_set` akzeptiert Zustand 0 (OFF) — `k6a_gov.c:804` clamp nur Obergrenze (`s > K6A_CD_L4`); ein Thermal-Core-Write von 0 hat keinen legitimen Absender (k6a-ctl nutzt `enable=0`, der einzige OFF-Pfad mit voller Freigabe), würde aber Schutz stilllegen | **bestätigt** — `enum k6a_state { K6A_OFF=0, … }`, kein legitimer `cool=0`-Schreiber im ctl | **Fix umgesetzt:** `if (s < K6A_GAMING) s = K6A_GAMING;` — OFF nur noch via `enable=0` |
| 4. Kleinigkeiten (sprintf Tätigkeit in status_show ohne PAGE_SIZE-Check, l2t-rec-Deadband-Warnung feuert nur auf sysfs-Pfad, `poll_ms`-Read in msleep ohne Lock) | notiert, alles benign/kosmetisch; das 3-K-Band (96−93) ist die AGENTS.md-dokumentierte bewusste Abweichung | keine Aktion |

Änderungen: `k6a_gov_exit` + `cool_set` (jeweils 1 Zeile, `Build353:`-Marker
im Kommentar), `K6A_GOV_VERSION` **1.5.1 → 1.5.2** (k6a-ctl `kver_ok`
akzeptiert `1.5.*`, Gate bleibt grün; distinguierbar für Volume-Lock).

Syntax-Check (clang 21.1.8, Rezept aus Local Build Notes,
`-DK6A_GIT_HASH/-DK6A_BUILD_HASH`): **rc=0, 0 Fehler, 0 Warnungen.**

## Post-352-Messsituation (Thermik, Deckel, FAN — r50–r59, 2026-10-18)

Alle Runden: gleiche Karte, Discharging, `arm.sh`-Gate (1560-s-Fenster, med180
via `validate.py`, Minute-Buckets + late-med 600–Ende separat; build351/352-Kernel,
Schwellen wie angegeben, 1 Runde = 1 Messung).

| Runde | Bedingung | med180 | late-med | bt max | k6a / p6-max |
|---|---|---|---|---|---|
| r50/r51 | uncapped, stock-Schwellen | 66,5 / 62,0 | — | — | k6a ctx: 20–39 % `cd_l2` |
| r52–r54 | uncapped, **96/99/102/93** | 70,2 / 74,5 / 77,0 (Ø 73,9) | — | — | **100 % gaming**, p6 100 % @2304 |
| r55 | „K-A" (Kappe vermutet) | 65,2 | — | — | **eigl. uncapped** — Sampler-uncap-Write (hp_stop `echo 2304000 > p6/scaling_max_freq`) hebt die ctl-Kappe lautlos auf |
| r56 | **FAN**, uncapped | **82,5 (Best)** | **69,6** | 32,3 °C | 100 % gaming |
| r57 | **NoFan**, uncapped | 72,0 | **45,6** | 41,7 °C | 100 % gaming; **Kollaps min 11→**: f6 2,30→2,25 GHz, bt>38 °C |
| r58 | NoFan, Trip95→**98 °C** | 63,8 ↓↓ | 38,6 ↓ | **45,2 °C ↑** | 17 % `cd_l2` (p6@1708800) — **TripWrite blieb 98 stehen, Pinning trotzdem ≤95,7** |
| r59b/r59c | **FAN, Gold-Cap 1708800** (capped-Sampler) | **fehlt** (SF `frame=` friert ein — `lay`-Counter statisch; sf_probe v1 bricht nach ~5 s ab) | — | 28,7 / 30,1 °C | **p6=1708800 in 412/412** — Cap end-to-end für erste Zeit schwarz auf weiß |

Befunde aus dieser Serie:
1. **Die 96-°C-l2-Schwelle ist der FPS-Hebel** (r52–r54 vs r50/r51), nicht die Gold-Kappe.
2. **Lüfter = die NoFan-Alternative**: SoC-CPU-Droop ab bt≈38 °C ist der Kollaps-Mechanismus
   (nicht k6a-CD, nicht Trip-Pinning); TRIP98 macht es schlimmer (Akkuhebel). Trips bleiben
   auf 95 — Daemon-Pinning ist **nicht per sysfs-Trip schraubbar** (Write bleibt, Pin bleibt).
3. **Gold-Cap end-to-end klappt seit v1.4.3/capped-Sampler** (r59b: 100 % @1708800, dt.
   Cap-Enforcement bestätigt); med180-Zahl der FAN+Cap-Zelle fehlt → **r59d** (gleiche
   Bedingung, `sf_probe.sh` **v2** pro-Snapshot-layer-fresh) ist noch offen, um den
   Gold-Cap-Vergleich mit Zahlen (u. a. r56 82,5 UNCAPPED) abzuschließen.
4. Instrumente: `fps.log`-`lay`-Frame-Counter friert seit >r59a ein (SF `--latency` /
   BLAST) — **fps-Quelle ist `sf_probe.log`** (Timestamps). `sflat_all`-Fragmente ≈60 fps
  -Avg über 306 frames (r59b), med180≈60 – **indikativ, nicht normierbar** (n-per-Window-
   Festival). Die Datensätze r59a (abgebrochen/truncated, „Runde nie geführt") und r59b/c
   bleiben ohne med180-Zahl — Messung gilt als **nicht referenzierbar**.

Messmethode-Änderungen:
- `fps_samp2.sh` (Gerät) = capped-Variante; `fps_samp2_uncap.bak` = Original. Sobald ein
  Gold-Cap-uncapped-R Durchlauf gewünscht ist, Rückkehr über `cp fps_samp2_uncap.bak
  fps_samp2.sh` **vor** dem Arm — **nie während des Samplings**.
- `sf_probe.sh` v2 (Gerät): `fg=COD` → pro Snapshot Layername frisch bestimmen + `--latency
  <LM>` mit `lmlen/rc/rows`-Markern → robust gegen zerstörte SurfaceView-Layernamen.
- **NIEMALS** `sh fps_samp2.sh …` manuell aufrufen („stop" = `DUR`-Argument!): trunciert
  `fps.log` (r59a-Befund). Stop via `kill -INT <pid>` (Trap → `hp_cont` sauber), im Zweifel
  `arm.sh`-Neustart, oder `pgrep -f fps_samp2 && kill -INT`.

## Build 394 Scope (SUSFS v2.3.0 kernel-core sync, 2026-10-23)

Upstruktur-Aufnahme: susfs v2.3.0 lebt upstream (sidex15/pershoot/WildKernels, `kernel_patches/v2.3.0`
und KernelSU-Next `legacy-susfs-v2`); KernelSU-Next 72f8cc62 trägt die SUS-Funktionen
**im Submodul** (sus_mount/sus_path/susfs_kstat/susfs_opath/btf_layout/lsm_hook/susfs_main = LKM-
Einstieg mit „refuse loading if in-built SuSFS is present"). Unser Kernel-Tree ist die
Thin-Core/Variante (`fs/susfs/`-Split: core/features/kstat/mount/open_redirect/path/spoof,
494 Z.) + Hooks nur in fs/stat.c (`susfs_sus_kstat_spoof_generic_fillattr`), fs/namei.c
(SUS_PATH ×2), kernel/sys.c (uname), selinux/avc.c (avc-log). Der Bump ist deshalb
**ABI-/Version-Sync, kein Feature-Portat**:

| Änderung | Datei | Grund |
|---|---|---|
| `SUSFS_VERSION "v2.2.0" → "v2.3.0"` | `include/linux/susfs.h:13` | Version-String-Konsistenz mit Submodul-ABI (supercall meldet v2.3.0 aus `susfs_abi.h:62`) |
| `DEFAULT_KSU_MNT_MINOR_DEV (1<<12)`, `STATX_SUS_KSTAT*` Bits, `susfs_is_current_app_uid()` | `include/linux/susfs_def.h` | upstream-True-Bit: Constants, die v2.3.0 ABI definieren; in Thin-Tree inert (keine super/world/state-Hooks), aber def.h #kein ABI-Drift |
| `strcat` → gebundenes `strlcat`-Makro (SUSFS_APPEND_FEATURE) | `fs/susfs/susfs_features.c` | defensiv (8192-Puffer macht Overflow theoretisch unmöglich, aber verbiete Concat-Umbruch von v2.3.0-Muster) |
| `Build394: susfs core v%s synced` pr_info | `fs/susfs/susfs_core.c:susfs_init` | post-flash dmesg-Beweis für die Version |
| `.v210`-Leichen gelöscht (fs/susfs.c.v210 + 2 Header) | verworfene v2.1.0-Single-File-Backups | dead weight |

Bewusst NICHT portiert: vfs_statfs/fdinfo/proc-fd-Spoof-Kanäle des 2.3-Refactors
(61e9e48) und `fs/super.c`-MinorDev-Gap-Fix — die knallen an unsere Architektur
(SUS-MOUNT = Submodul-Aufgabe, nicht Kernel-Blob); `result_mask`-Parameter des
Upstream-Fills ist im 4.14-`generic_fillattr`-Site bedeutungslos (kein statx-Mask-Filter).
SUSFS-Konfig (defconfig §5229–5237) unverändert.

Syntax-Check (clang 21.1.8, Kern-Recipe aus Local Build Notes ohne `__ASSEMBLY__`):
fs/susfs/susfs_core.c + susfs_features.c → beide rc=0, nur die (kern-wide bekannte)
GNU-Variable-Sized-Warning aus `cgroup-defs.h`.

## Build 446-449 Scope (Plan/Commit/Latch + gpu_idx_read-Fix, 2026-11-12)
- **Build446 (`5bdd119ac`, Release build447, Run 43127438219)** — KGSL/Devfreq-I/O aus
  `gov->lock` via Plan/Commit/Latch:
  1. Struct: `+s32 gpu_live_max, gpu_live_min` (Init -1) = 1-Tick-Snapshot der kgsl-Indizes.
  2. Neue Work-Structs `k6a_gpu_work/k6a_gpu_res`, `k6a_bw_work/k6a_bw_res`,
     `k6a_limits_work` (mit `.any`) / `k6a_limits_res`; Defines `K6A_W_NONE/RELEASE/APPLY`;
     Header-Kommentar "── Apply Limits (Build446: Plan/Commit/Latch)".
  3. Trios `gpu_cap_plan/exec/latch`, `gpu_floor_plan/exec/latch`,
     `bw_floors_plan/exec/latch`; `plan_limits` (Sync-Plan unter Lock für !legacy-Komatallback),
     **`plan_release`** (RELEASE-Self-Heal bei enabled=0-Ticks), `commit_limits` (ohne Lock),
     `latch_limits` (Re-Lock). Exec-Guards: RELEASE läuft immer, APPLY nur bei
     `READ_ONCE(gov->enabled)`. `gpu_reset`/`gpu_floor_reset` = sync-Wrapper (exec+latch) für
     enable=0/exit/!legacy-Komatallback unter Lock.
  4. `gov_thread`: plan unter Lock -> unlock -> commit -> Re-Lock -> latch.
  5. Checks: clang -fsyntax-only rc=0 (Warnset byte-identisch zum Pristine), Host-Test 23/23,
     CI 3 Runs grün (Host 43125320571, Build 43125320510, **Lockdep 43125320613** = Restruktur
     lockdep-sauber). ko 68 608 B (+1,9 KB).
- **r8/r8b (Enforce-Stresstest, Schwellen 65/70/75/60 + gpu_cap=585)**:
  1. **k6a-ctl-Clobber-Lektion**: `_enter_gaming()` (bin/k6a-controller :614+) läuft
     `_apply_caps_gaming` -> **`_apply_cd_thresholds`** (schreibt Config-CDT via gov_write,
     :456; Idempotenz-Check `[ "$cur" = "$CDT" ]`) -> `gov_write enable 1`. Kernel-seitig
     gesetzte Test-Schwellen werden beim GAMING-Entry vom Config-Wert überschrieben. Fix-Wege:
     settings.conf-Edit (`/data/adb/modules/k6a-ctl/config/settings.conf`, Key `cd_thresholds=`)
     + Controller-Restart, ODER Schwellen nach dem GAMING-Entry direkt schreiben. Config wird
     nur bei cfg_load gelesen -> Restart nötig. **TERM-Handler des Controllers schreibt beim
     Cleanup die alten Schwellen zurück** (17:23:57-Fund); Watchdog-Respawn dauert ~2-4 min
     (`ps` zeigt Scripte als `sh` — pidfile-Check nutzen). `cd_thresholds`-Node ist LESBAR
     (Direct-Verify), Kernel-`status` hat KEINE cd_l*_temp-Keys. Batch-Fallback: `printf` bricht
     bei `%`-Zeichen -> Batch-Datei + `cat file | su`.
  2. **r8 (leere Runde, kein Kernel-Bug)**: 22 min enabled, 0 CD-Transitions — `_enter_gaming`
     clobberte die Schwellen auf 96/99/102/93 (Config), Kernel-Peak 74 < 96 -> nie CD. Temp-Pfad
     gesund (temp=74 src=1 valid=1, bt=39, hash_verified=1).
  3. **r8b (Storm, Root-Cause-Daten)**: 22,2 min (530 Samples), 44 throttle_events,
     L2->L3->L4-Zyklen, Gold-Caps sauber (enforce_max_freq-Keep unter Lock funktioniert).
     **Storm**: `Build446: gpu cap` = 6014 Applies (~1/256ms-Tick), `was lowered externally
     (live_max=-1)` = 2003 (pr_info_ratelimited ~1,5/s), 1 released, bw floors 86x (0 fails),
     0 Oops. Funktional korrekt (idempotente Re-Applies hielten den Cap), aber Readback
     konvergierte nie.
- **gpu_idx_read-Return-Value-Bug (`cd02a1f4e`, Release build449, Run 43255030399)**:
  `kgsl_k6a_get_idx()` gibt bei Erfolg die **Anzahl der Einträge >= 0** (hier 7) zurück,
  `< 0` nur bei Fehler — `gpu_idx_read` reichte roh durch, alle Caller behandelten
  **jeden Wert != 0 als Fehler** (`!gpu_idx_read()` -> `!7 == 0` -> `live_have=false` auf
  Erfolg). **Der Bug war schon im Pristine build445 (`456be5f7d`) drin** —
  enforce_gpu_cap/enforce_gpu_floor brachen direkt nach dem Index-Read ab -> build445s
  GPU-Enforcement war still toter Code (surfaced nie: r7 gpu_cap=0, r8 kein CD).
  Fix: `return kgsl_k6a_get_idx(...) < 0;` (0=ok/1=fail), Caller unverändert. Host-Test
  23/23, CI grün. Post-Flash-Verify build449: banner, `git_hash=cd02a1f4`, ko 68 608 B,
  NoMount `22 (691)`, 0 Oops, P7-Muster exakt.
- **r8c (Acceptance, Fix bewiesen)**: Logger dieden beim Flash-Reboot (Muster: Logger
  sterben an Reboot) — Auswertung via main-dmesg-Ring (enthält nur r8c) + kernel-hist:
  **54 Applies ≈ 1 pro State-Change** (27 throttle_events + Level-Wechsel), **0x
  "was lowered externally"**, 1 Release sauber (`gpu_last_idx=-1` am Ende), bw floors 53x
  (0 fails/0 not-reflected), temp_peak 66, hist `off>gaming→cd_l2→cd_l3→cd_l4` mit Zyklen,
  0 Oops. Readback konvergiert (`gpu_live_max` gelatcht -> Plan schweigt) — Re-Apply-Storm
  weg, Roadmap-Posten **KGSL-I/O aus dem Lock: abgeschlossen**.
- **Verblieben (Roadmap)**: status_compact, adaptive GAMING-Polling, webui-Backoff,
  Micro-Optimierungen, CI-Hygiene-Posten. Details: `~/tmp/k6a-ctl-buglog.md` r5/r6/r7 +
  r8/r8b/r8c-Phase (Build-446-449-Abschnitt).

## Build 450-451 Scope (status_compact, 2026-11-12)
- **status_compact (`d0fdae972`, k6a_gov v1.7.2 -> v1.7.3, Release build451, Run
  43272556530) — geflasht + verifiziert 2026-11-12**:
  1. hist aus `status` ausgelagert in eigenen Node `/sys/kernel/k6a_gov/hist`
     (gleiche Render-Semantik: ms-since-insmod:temp:from>to, aeltester zuerst,
     leer ohne Transitions; from/to > K6A_CD_L4 skip wie vorher). status ist damit
     klein/fix-gross (1s-Controller-Heartbeat + 2s-Webui lesen es staendig).
  2. status hat die r8-Diagnose-Gap-Keys neu: `cd_l2_temp/cd_l3_temp/cd_l4_temp/
     cd_recover` (Capture unter gov->lock, neben thresholds_source). `cd_thresholds`-
     Node bleibt die 7-Werte-Schreib-Schnittstelle.
  3. k6a-ctl **v1.6.2** (`fd5f25b`, Gate OK 0 warns, ZIP 53 681 B, Release gepusht):
     hist-Seed beim Controller-Start aus dem Node (roh-cat + Guard gegen r()-"0"-
     Fallback), **eigener erster hist-Eintrag ohne Kernel-Seed** (Tick-Logic:
     leeres _HIST + Transition -> Eigen-Entry statt nur Kernel-Seed-Abhaengigkeit);
     alter Kernel (hist= in status) bleibt via _SHIST-Seed abgedeckt (beide Richtungen
     kompatibel). Start-Banner-Fallback v1.6.1 -> v1.6.2 (c0b8a81-Trap-Muster).
     WebUI-Hist-Tile zeigt jetzt wall-clock-Epochen (vorher: Kernel-Seed-Rows
     ms-falsch-skaliert = 1970er-Zeiten, bestehendes kosmetisches Muster).
  4. Verify am Geraet: banner `build451-aosp`, `git_hash=d0fdae97`, ko 69 384 B,
     NoMount 22 (691), 0 Oops, status version=1.7.3 + cd_l*_temp-Keys, hist= aus
     status raus (0 Treffer), hist-Node live (`40113:57:off>gaming`), Controller
     v1.6.2-Zyklus, `gov_hist=` mit Epoch-Timestamps = eigener-Erster-Eintrag-Pfad
     bewiesen. Kein Authoring-Log-Marker (Feature ohne neue Logzeilen), Comment-
     Marker `Build450:`.
- **Verblieben (Roadmap)**: adaptive GAMING-Polling, webui-Backoff, Micro-
  Optimierungen, CI-Hygiene-Posten (paths-Filter Status unklar — Doku-Push feuerte
  keinen Build, Code-Push schon; pruefen ob AGENTS.md-Ignore existiert).

## Build 452-453 Scope (adaptive GAMING-Polling + webui-Backoff, 2026-11-12)
- **Adaptive GAMING-Polling (`52917a18e`, Run 452-Grün, Release build453)**:
  Build402-far_cool-Maschinerie (slow-poll-Latch, slow_base, +3K-Steig-Reset,
  1000ms-Floor — gab es nur fuer OFF/BATTERY-far) um GAMING-far erweitert:
  `(state==GAMING && temp+5 < cd_l2_temp)` — gleiches Margin-Shape wie der
  bewiesene BATTERY-Fall. GAMING weit unterm L2-Schwellwert pollt jetzt 1 Hz
  statt 4 Hz; der Steig-Reset holt die schnelle Rate zurueck, sobald die Temp
  anfaengt zu steigen. 5-K-Luft = mathematische Garantie: eine Schwellwert-
  Kreuzung ohne vorherigen Reset ist unmoeglich (Reset feuert bei
  recent_min+3). CD-States bleiben IMMER bei poll_ms (dwell_out/Escape-Latenz).
  Worst-Case-Latenz: Flat-Soak innerhalb der 5-K-Bande (<=91 °C bei L2=96)
  — dort ist Polling schon schnell. Unterhalb: 3K-Reset innerhalb 1 slow-Tick
  (1 s) laesst >=2 s Reserve bei 0,5-K/s-Rampen. `poll_adaptive` bleibt
  default 1 (Kick-off-Toggle). Verifizierung nach Flash: `ticks=`-Delta
  (10 s) im kuehlen GAMING ≈ 1/s statt ≈ 4/s, nahe Schwellen/CD → 4/s.
- **Webui-Backoff (k6a-ctl v1.6.3, `b28d01d`)**: fetchData alle 2 s sichtbar,
  10 s im hidden-Tab (`document.visibilitychange` + dynamisches Interval).
  Spart nc-Traffic + Controller-CPU im Idle. Gate OK 0 warns, ZIP 53 907 B.
- **CI-Hygiene: bereits sauber, nichts zu tun** — build-kernel.yml hat
  paths-ignore `**.md`/`docs/**` (Doku-Pushs feuern keinen Build — bestaetigt
  durch die stille 1e6cb235f/a106b1635-Pushs), host-test.yml paths
  k6a_gov/**+tools/**, lockdep.yml paths drivers/thermal|gpu-msm|locking.
- **Micro-Optimierungen: kein konkreter Posten identifiziert** — Kernel-Shape
  nach Build446/450/452 gut; Item bewusst deferred (keine erfundene Arbeit).
- **Roadmap danach leer**: alle gebuchten Posten abgearbeitet (KGSL-I/O,
  status_compact, adaptive Polling, webui-Backoff, CI-Hygiene geklaert,
  Micro-Ops deferred). Naechste Kernel-Runde = neue Scope-Entscheidung.

## B+C-Runde (2026-11-13, kein Kernel-Release)
- **B — Hist-Zeitkorrektur (k6a-ctl v1.6.4, `ffc9988`, ZIP 54 588 B, deployed
  ohne Kernel-Flash)**: Kernel-hist-Rows (ms-since-boot, CLOCK_MONOTONIC
  suspend-exklusiv) werden beim Seed exakt nach Epoch konvertiert via
  `/proc/timer_list` "now at"-Domain (am Gerät bewiesen: uptime 52930 s
  suspend-inklusive vs. mono 19 776 s — /proc/uptime wäre schief). `_K6A_MONO_MS`
  gecacht → alter-Kernel-Fallback ebenfalls konvertiert, 0 Extra-Forks/Tick;
  graceful Fallback ohne timer_list. app.js unverändert. Live-Beweis: Node
  `42878:55:off>gaming` (ms) → DATA `1794566934:55:off>gaming` (Epoch), Banner
  `k6a-ctl 1.6.4 start`. Funktions-Unit-Test lokal (ages 600/2400/0 s) ✓.
- **C — KASAN-Run (lockdep.yml kasan=true, Run 19/43417909472) → FAIL
  klassifiziert als Vendor-Blocker**: qcacld-3.0 `__qdf_bug` undefined beim
  vmlinux-Link (Vendor-WLAN-QDF unter KASAN-Konfig nicht linkbar — die im
  Workflow-Input dokumentierte 4.14/GCC13-Kategorie). k6a-Code unbelastet
  (Modul-Phase nach vmlinux nie erreicht; drivers/thermal kompilierte sauber).
  **Optional r10-Kandidat**: KASAN-Config-Variante ohne qcacld für einen echten
  k6a_gov-KASAN-Check (Workflow-Config-Tweak).
- **r9 (heute Abend) steht aus**: adaptive slow-poll × CD-Interplay verifizieren
  (ticks-Delta-Muster, L2-Entry-Latenz, L3↔L4-Flip-Rate als r10-Damping-Basis).

## r9/r9b (2026-11-13, config-only Kalibrierung — Vendor-Cliff-Fund)
- **r9-Fund**: Default-Schwellen 96/99/102/93 sind für die KB8-Temp-Quelle
  (2nd-highest der 4 Gold-CPU-Zonen) im echten Gaming NICHT erreichbar genug
  (r9-Peak 87) → Stack passiv (0 throttle_events), Vendor-thermal-engine
  drosselt selbst (70→16 fps-Bounce, teils unspielbar). Der Design-Intent
  (präventive GPU-Caps vor der Vendor-Engine) braucht kernel-sensor-gerechte
  Schwellen.
- **Fix = settings.conf-Kalibrierung** (65/70/75/60, r8c-bewiesen, User-approved)
  + Controller-Restart — kein Kernel-Flash. r9b: Peak 77, GPU-Zonen 70.6/67.4,
  Vendor nur tp=2/3 mild, 109 throttle_events, externally=0, 0 Oops, User
  "angenehmer" ✓. thresholds_source=user.
- gpu_caps-Node beim Custom-Profil: 800/565/430 (L3=565 lvl2, L4=430 lvl3).
- Offen/Tuning: L2↔L3-Flip-Damping (falls bemerkt), kalibrierte Defaults in
  ctl-Template + kernel profiles[] (nächste Kernel-Runde), KASAN ohne qcacld.

## r10/r11 (2026-11-13/14, Review-Scope Build 454/455 + k6a-ctl v1.6.5 — live verifiziert)
- **Externer Review („Analyse v2") triagiert + implementiert**: K1 valid → K2b valid →
  K4 valid (Doc-Drift) → K7 valid → K8 reduziert → C1 valid → C5 valid; K5 deferred
  (insmod-Hash), K6 REJECTED (game_pid ist Diagnose-Kanal, bleibt), C2 ENTFÄLLT
  (Backoff existierte schon: service.sh 3→30 s, >10 Crashes/60 s → give-up),
  C3 REJECTED, C4 outdated (WebUI-Token seit v1.5.1). User-Approval „Voller Scope".
- **K1 Sensor-Fail-safe (`5622eb83e`)**: `invalid_ticks` im Struct + `sensor_failsafe_tick()`
  — 10 aufeinanderfolgende Ticks ohne Temp-Quelle (2,5 s @ poll 250 ms) **nur im
  state GAMING** → CD_L2 erzwungen + Einmal-`pr_warn "Build454: sensor fail-safe"`,
  eine hist-Row, Recovery natürlich über die State Machine (Quelle wieder da → CD-Exit).
  Fail-open nur im GAMING (Hot-Side), gehaltene CD_L2/L3/L4/OFF bleiben (schon
  konservativ). Reset im t_valid-Zweig. r11-Boot: **still** (0 pr_warn) — dormant wie
  designed, erster Live-Test steht noch aus (Sensor-Tod simulieren nötig).
- **K2b profile_store(0)**: Cap-Vars wurden zwar gecleart, aber ohne `apply_policy()`
  lief die Freigabe als 1-OPP/Tick-Rampe statt OFF-sofort. Fix: enforced_max=0 +
  cap_now=0 + `apply_policy()` (Spiegel des enable_store(0)-Blocks aus Build 446).
  GPU-Cap released wie bisher über den nächsten Plan-Tick.
- **K7 Hotplug-Re-Resolve**: Cached-Gold-CPU hotplug-off → `cpufreq_cpu_get()` NULL,
  seen=0, der „not applied"-Check skippte still. Jetzt frisches `find_gold_cpu()` +
  Cache-Update + Readback-Retry im selben Tick. **Deadlock-Falle entdeckt**: erste
  Version hatte inneres `mutex_lock` — `enforce_max_freq` läuft bereits unter
  `gov->lock` (plan_limits), Fix ohne Lock.
- **K8 Zone-Spiegel-Log (einmalig, freq_init_worker)**: 8 Quellen mit id+type
  (`Build454: zone ...`). r11-Boot beweist: cpu-1-0..3-usr id 26-29, cpu-0-0-usr
  id 18; xo-therm/soc-therm/thermal_zone0 „not found (yet)" (vor Registration —
  erwartet). Bonus: ctl-Zonen-Fund (Gold=zone26/Silver=zone18) matcht den
  Kernel-Mirror — erstmals gegenseitig verifiziert.
- **Hygiene**: 14 verbliebene `sprintf()` in show-Handlern → `scnprintf(b, PAGE_SIZE, …)`
  (24 scnprintf total, 0 sprintf). Syntax-Check pristine-vs-neu: Warnset identisch.
- **K4 Doc-Drift geschlossen**: README behauptete hysteresis `fast` default 500 seit
  v1.6.0-Ära — Code (Build 347+) schiffte **3000** (explizit `gov->hysteresis_fast = 3000;`).
  Erläutert evtl. die r9b ~5 s-Flips. README-Row korrigiert (1..5000, default 3000/2000),
  Version-Refs → 1.7.4, hist-Node-Row, v1.7.4-Feature-Sektion. **CI-Guard** in
  tools/k6a_gov_host_test/run.sh: Code-Hysteresis-Defaults + K6A_GOV_VERSION gegen
  README-Node-Table → „DOC-DRIFT: OK …" bzw. Fail. (Guard selbst hatte 2 Bugs während
  der Runde: `[0-9]*` matcht auch leer — `hysteresis_fast = fast;` im Store — Fix
  `[0-9][0-9]*`; README-Extraktion frisst Newline-loses printf-Futter nicht — Fix awk.)
- **Host-Test 31/31**: extract.py +`sensor_failsafe_tick`; S7-Szenario (Fail-safe feuert
  nur aus GAMING bei exakt 10 Ticks, hist-Row, Reset bei valid, gehaltener CD_L3 bleibt);
  harness `tick()` resettet invalid_ticks (gov_thread-Spiegel).
- **CI**: Push-Runs grün (host-test sofort, Build 11 m, Lockdep 7 m); Release als
  **build455** (Run 43502371855) weil die Push-Runs die Nummer 454 verbrauchten
  (bestätigt: NNN = Run-Nummer, Dispatch ≈ Push+1).
- **k6a-ctl v1.6.5 (`1d48701`)**: `_read_int()` in k6a-lib.sh (0 forks: read+eval,
  Zielvar-Semantik wie read_temp/TEMP_NOW) → alle 5 `_write_data`-Hot-Path-Leser
  (P0/P6 cur_freq, GPU devfreq cur_freq, battery capacity/temp) + service.sh-Uptime-Scrub
  an 3 `cut`-Stellen. **Gate [7] Fuzz (12 Cases) fand beim ersten Lauf einen echten Bug in
  `_read_int` selbst**: POSIX-read liefert rc=1 bei fehlendem Newline, Variable ist
  trotzdem gesetzt — Fix: Verzweigung auf Nichtleere statt rc. hb_pid/hb_hit/hb_miss
  im data.txt-Blob (nach gov_hist) + app.js-Anzeige unter der Hist-Tabelle. Banner-
  Fallback + module.prop 1.6.5/175. README: **Failure-Semantics-Tabelle** (Kernel hold +
  Fail-safe / ctl-Scrub / Watchdog-Backoff / Turbo-Notbremse-Re-Arm). Gate 41/41,
  `sh -n` grün, Controller-Smoke Sandbox 3 Ticks (34 Keys). Release v1.6.5 (Zip 56 609 B).
  **Lektion gh release notes**: backtick-Fragmente in `--notes` mangled durch Shell-
  Command-Substitution (`cut`-Befehl lief!) — Notes via `--notes-file` nachlegen.
- **r9c-Runde (build453 + v1.6.4, 05:10–05:17)**: 74 throttle_events (2→76), CD_L3-Dips
  2× „@70 without dwell" (25 s auseinander), Cap-Rampe 2,208 GHz→1,555 GHz→rauf, Flips
  unverändert 1–3 s-Rhythmus am 65/70-Boundary, Controller überlebte (0 Crashes).
  Logs: `/data/local/tmp/a15r9c_{kst,dmesg}.log`. Uptime-Domain-Fund: /proc/uptime
  (boottime, suspend-inclusive) vs. Kernel-printk/hist (monotonic, suspend-exklusiv)
  differieren um die Suspend-Zeit (Nacht ≈ 2 h 50 m) — hist-Rows sind NICHT uptime-
  vergleichbar, nur dmesg-Stamp-vs-hist-Stamp (gleiche Domain).
- **Flash + Deploy (2026-11-14 05:53)**: build455-aosp.zip (19 346 005 B, SHA256
  `4c70215b…79bd1` verifiziert gegen SHA256SUMS) via KSU-Manager geflasht; ctl v1.6.5
  in-place VOR dem Reboot (Backups `.bak-v165`), Reboot übernimmt alles. Boot-Sequenz:
  app.js-Token → ko 1.7.4 geladen → service start → stale-lock übernommen (6570) →
  Zonen neu erkannt → thresholds ← 65/70/75/60 → **k6a-ctl 1.6.5 start** → GAMING/custom.
- **r11-Boot-Verify (alle grün)**: k6a_gov 1.7.4 (ko 45 056→**53 248 B**), Zone-Mirror ×8,
  Fail-safe still, 0 Oops, schedutil erzwungen, gpu_caps 800/565/430, thresholds
  65/70/75/60, hb-Keys im data.txt, HB alive. ksud/KSU-Module-Loading funktionierte mit
  dem Magisk-Format unverändert.
- **UMGEBUNGSWECHSEL dokumentiert**: (1) **KSU Next ist jetzt Root-Provider** (User-
  Flash via KSU-Manager; `id` → context `u:r:ksu:s0`; Magisk wird nicht mehr als Loader
  erwartet — Magisk-Format-Module laden über ksud). (2) **`uname -r` spoofed**:
  `6.12.0-android16-6.12-perf-g1d01cd293` (SUSFS-Uname-Spoof) — REAL ist 4.14.369-badazz;
  Beweis über k6a_gov-Laden + Modulgröße, NICHT über uname. Nicht in Panik verfallen.
  (3) **Termux-su-Wrapper-Syntax**: `/data/data/com.termux/files/usr/bin/su` (ELF, execv
  `/system/bin/su` + hardcodiertes `sh -c`) — Aufruf ist `su 'befehl'` OHNE `-c`
  (`su -c '…'` → „unknown option"-Wirrwarr); KSU-Grant für Termux bleibt erhalten.
  (4) `/sys/kernel/k6a_gov/version` existiert nicht — Version ist Key in `status`.
   (5) `pidof` matcht den Controller nicht (heißt `sh` im comm) — `pgrep -f` oder HB-Log nutzen.

## r12/r13 (2026-11-14, Mittelweg-Tests auf build455) — PUMP-MECHANIK KORRIGIERT
- **r12 (passive-Rediscovery)**: passive 96/99/102/93 + poll_adaptive=0 statt r9b-Kalibrierung →
  Intro-Spur **120 fps wie build451** (r9b 65/70/75/60 + poll_adaptive=0 war der Intro-Killer:
  L2-Entrys mitten in der Intro-Phase). Round 2 (heat-soak, ~100 Fps Ziel): Vendor-Eskalation
  26-75 fps Jump-Zyklen bei Kernel 100 % gaming → Vendor-Onset-Zone Kernel-Quelle **~78-82 °C**
  (Skin 74-76). Logs: `/data/local/tmp/a15r12_dmesg.log`, `r12_status.txt`.
- **r13 (Mittelweg 76/80/86/73 + gpu_caps 800/680/500, poll_adaptive=0)**: throttle_events +69,
  hist 75↔80/82-Flips alle 4-8 s → **PUMP** an der [L2,L3)-Grenze. Logs: `a15r13_dmesg.log`,
  `a15r13_hist.txt`, `a15r13_status.txt`.
- **Pump-Mechanik (WICHTIG — alte Aussage war falsch)**: L3→L2-Exit = `t < gov->cd_l2_temp`
  (k6a_gov.c:613) — **NICHT** hardcodiert; das Flip-Band = `[cd_l2_temp, cd_l3_temp)` ist
  **config-formbar** (r11s-Band [65,70), r13s-Band [76,80)). Pump tritt auf, wenn das Last-
  Gleichgewicht IM Band liegt; Exit UNTERHALB des gecapten Gleichgewichts = strukturell pump-frei.
- **Test D1 (armed, 2026-11-14 21:4x, auf build457)**: `cd_thresholds=72 80 87 66 1708800 1555200
  1324800` (settings.conf Zeile 55) + gpu_caps 800/680/500 + poll_adaptive=0. Band [72,80);
  L3-Cap-Äquilibrium (~74-77) robust über Exit 72. Erwartung: kühle Intros 120, Soak steady ~100
  statt Sägezahn. Vendor-Onset (~78-82) wird vom L3-Cap unterschritten/knapp gestreift; Eskalation
  D2 = `gpu_caps 680/680/500` falls nötig. **Reboot resettet gpu_caps+poll_adaptive** (Sysfs-only!)
  — nach jedem Flash Re-Arm nötig; cd_thresholds überlebt via settings.conf (ctl re-applied).
- **QG-K-Charging-Detection-Falle**: Fuel-Gauge-Lines (`charge_status=2`, gepinntes soc, positive
  batt_ma) sind stale/Latch-Garbage — Beweis: Discharging/30 %/3,66 V während QG-K "charging"
  meldete. Einzige Quelle: `/sys/class/power_supply/{usb,dc}/online+present`.
  (r9-Kabeltheorie verfallen.) grep-Falle: `fail.safe|Oops|BUG` = 257 pm6150_charger rc=-22
  Noise-Hits (0 echte Fail-safes).
- **Controller-Restart-Gap**: TERM-Handler schreibt die im RAM gemerkten Thresholds zurück;
  bis zum nächsten Commit-Cycle (~2 min) läuft der Kernel mit ALTEN Werten — kurzfristige
  sysfs-Overrides NICHT via Restart ins persistence-Flashen treiben.

## Build 456/457 (2026-11-14, KSU-Supercall-Gates — Defconfig-Hardening-Bootloop)
- **Anlass (Duck-Detector-Report, Plan-Mode-Matrix)**: prctl(0xDEADBEEF)-Dispatcher
  `KernelSU-Next/kernel/supercall/supercall.c:131` `ksu_handle_prctl` — arg2==2 get_info
  (:293-321) + arg2==3 crown-diag (:200-220) **ungated** (jede App kann root-Prozesse
  listen); `ksu_handle_sys_reboot` :531 — fd-request `KSU_INSTALL_MAGIC2` (:634-649) **ungated**
  (Kommentar :636: „jede App kann fd anfordern"). Korrekt gated: set_manager_appid (:653/:659
  uid==0), SuSFS-Commands (:543 uid==0), prctl-FD-Install (:248 is_manager).
  `is_manager()` = static inline `manager/manager_identity.h:16/:47`; Hook `hook/syscall_
  table_hook_arm64.c:130-133` fällt bei `-ENOSYS` zum echten prctl durch → Kamouflage-by-Design.
- **Fix (f5257d095, patches/ksu-prctl-gate.patch)**: 3 Gates — get_info + crown-diag:
  `if (!is_manager()) return -ENOSYS;` (Funktion ist `long ksu_get_crown_diag(...)`, NICHT int);
  fd-request: `if (current_uid().val != 0 && !is_manager()) return 0;`. Kein App-Bruch:
  Compat-Empfehlungen stehen im Plan-Mode-Report (npm-Expose, HiddenApi-Bypass).
- **Repo-Mechanik**: KernelSU-Next = **Submodul** @ `79003ef8e8df26b38d55a46b994e4362448bd725`
  (v3.3.0-318, vandalsquad187/KernelSU-Next.git), Checkout mit `submodules: true` (:98);
  `drivers/kernelsu` = **Symlink** → `../KernelSU-Next/kernel`. Patch-Datei `patches/
  ksu-prctl-gate.patch` mit umgeschriebenen Pfaden (a/KernelSU-Next/kernel/...), `patch -p1`
  aus Repo-Root; Round-Trip bewiesen (pristine-Backups in Termux-tmp). **.gitignore-Fallen**:
  Zeile 34 `*.patch`, Zeile 92 `patches` UND `.github` → `git add -f` nötig.
- **Workflow (.github/workflows/build-kernel.yml)**: Step „Apply KSU hardening patch" nach
  Checkout + 3 grep-Assertionen (lokal getestet: ASSERT1/2/3-OK); REQUIRED_CONFIGS hatte
  temporär FORTIFY+INIT_STACK_ALL_ZERO (457: wieder entfernt). Resync miui/test mit main
  (X-ours) nimmt beide Commits mit.
- **f5257d095 (Build 456) = BOOTLOOP**: + `CONFIG_FORTIFY_SOURCE=y` + `CONFIG_INIT_STACK_
  ALL_ZERO=y` (Makefile :837/:845, `-ftrivial-auto-var-init=zero`, GCC 13.3; security/
  Kconfig.hardening-Choice ohne plugin-deps; CI assertete beides, KSU-Patch applied — Beweis
  im CI-Log Run 43636158212; 456-vs-455-Image-Diff = 31,2 M Bytes = 70 %). Gerät: Xiaomi-
  Splash, ~3-s-Reboot-Zyklus, pstore/ramoops nicht verfügbar; aosp-ZIP korrekt geflasht
  (Banner/size/inner-zip-Struktur verifiziert).
- **17a66600a (Build 457) = Boot-Fix, Gates drin**: beide defconfig-Items revertiert (455-
  Stand), KSU-Gates unangetastet → bootet sauber (Banner `build457-aosp`, 0 Oops, su OK,
  k6a unverändert). 457-vs-455-Image-Diff = 5,3 M Bytes (12 %) = Abschnitts-Realign durch
  die kleine KSU-Änderung. **Verdachtsverteilung**: FORTIFY und/oder INIT_STACK-`zero`
  killen early boot auf diesem Vendor-Tree (4.14+GCC13) — isoliert NICHT festgenagelt;
  Isolation (ein Item pro Build) nur optional bei Interesse. Global-CFLAGS-Härtung =
  höchster Blast-Radius → nie mit anderem Scope kombinieren; Lektion: Härtungs-Flashs
  nur mit Zeitfenster und Backup-Kernel.
- **KSU-Gate-Akzeptanz (nach 457-Flash)**: Device bootet + su arbeitet; Manager-Dialog
  (fd-ioctl-Pfad) vom User zu bestätigen — get_info/crown-diag sind App-seitig nur für
  den Manager nützlich, Gate darf Nicht-Manager die -ENOSYS-Kamouflage geben.
- **Manager-Swap auf spoofed (2026-11-14, verifiziert, Go erteilt)**: Upstream-dev
  (KernelSU-Next/KernelSU-Next, Run 2269 @ `7ccf6147`) baut `manager-spoofed` =
  `KernelSU_Next_v3.4.2-198-g7ccf6147-spoofed_33553-release.apk`. **Cert-Verifikation
  (Vorsicht, 2 Fallen)**: (1) APK Signing Block Geometrie — head-u64-Check-Position
  = `cd_offset - sz - 8`; die Pair-Walk-Position ist danach automatisch `pos` (der
  Kernel resettet NICHT) = `cd_offset - sz`; wenn man selbst resettet, liest man
  `id=0x6ca` (= erstes u64 low-word) statt `id=0x7109871a` — false NO-V2-PAIR.
  (2) Erwartungswerte im gepinnten `KernelSU-Next/kernel/Kbuild` (:188-196 Fallback,
  Größe `0x3e6`=998 B, Hash `79e590113c4c…21bf7`, Signer CN=Rifat Azad). v2-Payload-
  Layout: signers→signer→signed_data→digests(algo u32 0x103, len u32, 32B)→
  certificates→cert_len u32. Spoofed-Cert = **byte-exakt MATCH** → Throne-Tracker
  (`on_post_fs_data`/`on_boot_completed` `track_throne_now`, kernel-seitig, unabhängig
  vom fd-Gate; Live-Observer `track_throne(true)` ab boot_completed) registriert das
  neue Package automatisch. Bootstrap-Kette intakt: ksud `CHANGE_MANAGER_UID`
  (uid==0-Gate unberührt, supercall.c:667-677). Mapping: Kernel 38532 = merge-base
  `b427a3f9` + fork; dev-Manager +148 Commits → kosmetischer Update-Hinweis möglich,
  Funktion degradiert per -ENOSYS-Durchfall. Install-Sequence **REAL (User-Ablauf
  2026-11-14, läuft)**: erst den ALTEN Manager DEINSTALLIEREN, dann spoofed
  installieren/öffnen — side-by-side funktioniert NICHT (der Throne-Tracker hält
  offenbar genau EINEN Seat: der bereits registrierte appid wird vom Scan nicht
  an ein zweites valid-APK abgegeben). Manager-Akzeptanz auf spoofed v3.4.2-198:
  **läuft** ✓ (Registrierung + get_info + fd-Pfad bestätigt in vivo).
  `mappings-spoofed` = ProGuard-Mappings, nicht installierbar.

## r14 (2026-11-14 23:22-23:38, build457, D1-Runde — User „super flüssiges Spiel")
- **Setup**: cd_thresholds 72/80/87/66 + gpu_caps 800/680/500 + poll_adaptive=0;
  Manager-Swap-Reboot hatte gpu_caps/poll_adaptive resettet → Re-Arm vor der Runde
  (Lektion hart bestätigt: **JEDER Reboot resettet beide**, auch nicht-Flash-Reboots).
- **Verdict D1 = ERFOLG**: (1) **Vendor-Engine still die GANZE Runde**
  (`vendor_sconfig=0`, `vendor_cpu_limits=none`) — trotz Source-Spikes bis 80: das war
  das D1-Hauptziel (r12: 26-75-fps-Vendor-Chaos, r13: Escalation). (2) Intro sauber:
  ~10-12 min reines gaming (0 Events), CD-Phase nur in den letzten ~5 min
  (~1180-1463 s). (3) **throttle_events=12** (r13: +69) bei Subjectiv „super flüssig".
  (4) Residual: Endphasen-PUMP L2↔L3 — 11 Flips in 44 s (1323-1367 s, ~5,5-s-Kadenz,
  Readings 71↔80): Equilibrium ~76 °C liegt MIT ±4-5-K-Zonen-Noise an den Bandkanten
  [72,80). Auswirkung benign: CPU 1708↔1555 MHz +   GPU 800↔680 MHz, beide mild.
- **D1 = neue Baseline (User-OK 2026-11-14) + rebootfest via k6a-ctl v1.6.6**:
  Fund — `gpu_caps`/`poll_adaptive`-Nodes sind Sysfs-flüchtig (Reboot → Kernel-
  Defaults 800/565/430 bzw. adaptive=1); das ctl las gpu_caps nur für Telemetrie.
  Fix in k6a-ctl v1.6.6 (`b6dd5c6`, Release v1.6.6, Gate grün): settings.conf-Keys
  `gpu_caps`/`poll_adaptive` + `_apply_gov_caps()` beim Controller-Start
  (idempotent wie `_apply_cd_thresholds`; deckt Boot/Watchdog-Respawn/rmmod-
  Recovery ab, alte Kernel failen still). Deploy-Tanz: Kontext = `app_data_file:
  s0:c83,c257,c512,c768` für controller/module.prop, `system_file:s0` für
  settings.conf; HB-Pid-Erkennung via `/proc/[0-9]*/cmdline`-Scan (service.log-
  hb-Zeile trägt pid=0 → data.txt ist der Diag-Ort). Verifiziert: Start-Log
  `gpu_caps=800000000 680000000 500000000 poll_adaptive=0`, Sysfs-Readback
  identisch. Nächster Boot beweist den Write-Pfad (bis dahin nur der Skip-Pfad).
- **D1a aktiviert + 458a gestartet (2026-11-15)**: settings.conf cd_thresholds
  72→70 (Template-Sync `22c5dfe` im ctl-Repo, ctl-Commits: v1.6.6 = `b6dd5c6`),
  Entry 80 unverändert (Vendor-Risiko konstant). Hot-Reload-Hinweis: ctl parst
  pro Tick, aber `_apply_cd_thresholds` feuert nur bei State-Wechseln — Mid-Gaming-
  Edits wirken erst bei der nächsten Transition/Boot. Build 458a = FORTIFY allein
  (`66bb2a510`: `CONFIG_FORTIFY_SOURCE=y`, INIT_STACK_ALL_ZERO bleibt aus,
  REQUIRED_CONFIGS-Assert `CONFIG_FORTIFY_SOURCE=y` wieder drin); CI-Run
  `43682402722`. Isolationsplan: 458a bootet → FORTIFY unschuldig → 458b (INIT
  allein) nagt den Schuldigen fest; 458a bootlooped → FORTIFY ist er.
  Nach jedem 458a/b-Boot: Banner/Oops/Thresholds/gpu_caps/poll_adaptive prüfen
  (v1.6.6 sollte D1a-Werte selbst setzen — Write-Pfad-Beweis).
- **Reboot-Proof 2026-11-15 ERFOLGREICH (v1.6.6 vollständig bewiesen)**: Boot-Log
  `[06:51:05] cd_thresholds ← 70 80 87 66` + `[06:51:06] gpu_caps ←
  800000000 680000000 500000000` — beide `←`-Zeilen = echte Writes auf
  Reboot-Defaults (Skip-Pfad war vorher der einzige bewiesene); Sysfs-Readback
  exakt D1a. D1a = aktive Baseline.
- **458a BOOTLOOP → FORTIFY überführt (2026-11-15)**: Splash-Loop 2-3 s Zyklen,
  User hat 457 noch am selben Tag zurückgeflasht (Rescue-Dance erprobt, Minuten).
  Bisect-Matrix damit komplett: **455 ✓ (beide aus) / 456 ✗ (beide an) / 457 ✓
  (beide aus) / 458a ✗ (FORTIFY allein)** → `CONFIG_FORTIFY_SOURCE` allein =
  Bootloop-Ursache; INIT_STACK unverdächtig (456 durch FORTIFY erklärt).
- **pstore-Forensik = Dead End (erklärt)**: `/sys/fs/pstore` leer nach Bootloop.
  Ramoops korrekt registriert (dmesg: `ramoops: attached 0x400000@0xb0000000`,
  console/pstore-1 @0.252 s, Zones 20 KB) — aber die 458a-Death-Cycles sterben
  früher (< 0,25 s, vor pstore-Registrierung) → nichts zu capturen. Mechanismus-
  Hypothese: 4.14-FORTIFY-Implementierung vs. GCC 13.3 (`__builtin_object_size`-
  Semantik-Änderungen seit GCC 10; Ära-Kernels pingen alte GCCs). FORTIFY-Fix =
  Port der Compiler-Kompatibilitäts-Patches (tief, unsicher, geringer Nutzen) —
  nicht verfolgt. sweet_ramoops bleibt für spätere Spät-Crashes nutzbar.
- **v1.6.6 dritter Live-Beweis (post-Flash)**: nach 457-Rückflash re-armete der
  ctl selbst: `70 80 87 66` + `gpu_caps 800/680/500` + `poll_adaptive 0` —
  Persistenz gilt auch über Kernel-Flash-Zyklen.
- **Build 459 = INIT_STACK allein** (`c21e4ac62`: FORTIFY zurück auf 457-Stand,
  `CONFIG_INIT_STACK_ALL_ZERO=y` + `# CONFIG_INIT_STACK_NONE is not set`;
  Workflow-Assert `CONFIG_INIT_STACK_ALL_ZERO=y` statt FORTIFY-Assert). CI-Run
  `43687047805`. Plan: bootet → 459 = Final (INIT an, FORTIFY aus); bootlooped →
  beide schuldig, Revert auf 457-Stand (baut 460 = 457-äquivalent, main-konsistent).
- **459 BOOTET → FINAL-BUILD (2026-11-15)**: CI grün (run `43687047805`), Banner
  `build459-aosp` verifiziert, 0 echte Oops/Panics (einziger grep-Treffer =
  benigne `evtlog_status: panic:1`-DRM-Debug-Zeile, identisch in 457s Boot),
  k6a_gov 1.7.4 alive (state=gaming, temp 50, throttle_events 0), su OK,
  v1.6.6 Re-Arm post-Flash ✓ (dritter Kernel-Flash-Zyklus in Serie).
  **Härtungs-Finalstand: `CONFIG_INIT_STACK_ALL_ZERO=y` AN, `FORTIFY_SOURCE`
  AUS (schuldig, unverträglich mit GCC 13.3 auf 4.14 — frühster Boot-Tod,
  pre-ramoops).** Isolationsfall vollständig: 1 Build = 1 Variable, Rescue-Dance
  (Download-Zip + Recovery-Flash)   bewährt in Minuten.
- **r15 (10:46-~11:15, build459, D1a-Testrunde) → D1a FALSIFIZIERT, Revert auf
  D1-72 (2026-11-15)**: User „ziemlich laggy". Hist/Residency: cd_l3 843,9 s
  (14 min! vs r14: 72,5 s) — zwei kontinuierliche L3-Strecken (575 s + 253 s)
  bei CPU 1555/GPU 680. Mechanik: Exit < 70 → L3-Exit erst nach tiefer
  Abkühlung → statt r14's schnellen Flips (71↔80, subjektiv smooth) lange tiefe
  L3-Dwells = gedrosselte Minutendauer = lag. throttle_events 9 (vs r14: 12) —
  Event-Count war irreführend: **Dwell-Zeit, nicht Event-Zahl, ist die
  Lag-Metrik**. Vendor weiterhin stumm (vendor_sconfig=0). Rückkehr-Logik:
  r14-Exit 72 → Exits @71 = minimale Überschreitung → schnelle L3-Exits →
  Flips benign; Exit-Tiefe 70 kippt das Equilibrium in den Dwell-Modus.
  **Thermal-Finalstand = D1 exakt: `72 80 87 66 1708800 1555200 1324800` +
  gpu_caps 800/680/500 + poll_adaptive 0** (r14 „super flüssig" bleibt der
  Goldstandard). Kein weiterer Test; 71-Experiment ausdrücklich verworfen.
- **ctl-Bedienlektion r15**: Mid-Gaming settings.conf-Edits parse der ctl pro
  Tick, aber `_apply_cd_thresholds` feuert NUR bei State-Transitions — im
  Dauer-GAMING wird ein Edit erst beim nächsten Transition aktiv; für
  Sofortwirkung: Controller-Neustart (kill -TERM + Watchdog-Respawn = 1 Start-
  Log-Zeile + Apply). Pid-Falle 2: `/proc`-Scan unter su kann in Timeout
  laufen (807 Pids); robuster Weg = `tr "\0" " "`-Scan auf cmdline + Ausgabe
  mit Pid-Feld, Kill in EINEM kurzen Befehl (compound mit sleep läuft in
  den 120-s-Tool-Timeout — zweistufig killen). Revert-Apply 11:21:08
  verifiziert (`cd_thresholds ← 72 …` + Start-Log).
- **Messung-Fixes (Wichtig)**: `temp_peak` ist **Pro-Tick-Snapshot** (`gov->temp_peak =
  t_peak` :1345, kein latched Max — 65 @+17 s nach Rundenende war kein Widerspruch
  zu den @80-Flip-Readings). Tick-Arithmetik: Rundensegment 3758 Ticks/964 s = exakt
  4 Hz (poll_adaptive=0 honoriert); Pre-Round-Defizit = Nach-Reboot-Fenster vor dem
  Re-Arm (adaptive 1 Hz Default). Hist-Ring (16 Slots) evicted früh —
  throttle_events als Transfer-Zähler gegen evicted Rows konsistent nutzbar (12 =
  10 Escalations + 2 Entries).
- **Optionale Mikro-Tweaks (nicht nötig, notiert)**: cd_l2_temp 72→70 (Exit tiefer
  unter Equilibrium → weniger Endphasen-Flips; Entry 80 bleibt → Vendor-Risiko
  unverändert); ODER gpu L3-Cap 680→650. Empfehlung: D1 als neue Baseline akzeptieren.
- **Tools**: r14-Dmesg-Capture-Worker wieder dem Phantom-Killer erlegen (47 Zeilen
  Drain, ~30 s — Mid-Session-Spawns bleiben killbar); Auswertung über Kernel-Ring
  (`dmesg`, Boot < 1 h) + hist/status-Nodes. Files: `/data/local/tmp/a15r14_status.txt`
  (pre), `a15r14_post.txt`, `a15r14_dmesg.log` (Fragment).

## 2026-11-17 — r16 A/B, Vendor-Ladder enttarnt, Daemon-Stop = Gaming-Default
- **r16 A/B (11-15/16, D1-72, v1.6.6)**: A = Lüfter ×2 Runden („super flüssig“): +34 Events
  (~17/Runde), L2 +880 s, L3 +334 s, hist = Mini-Flips @80↔71 (2-6 s), temp_peak 58. B = ohne
  Lüfter („besser als erwartet, hektisch“): +24 Events, gaming +781 s, L2 +653.8 s, L3 +266.7 s,
  hist = dichte Flips @71↔@80 (0.8-4.9 s) + 17-50-s-L3-Holds, Ende 64/peak 64. Vendor-Mirror
  stumm in beiden. **Buchhaltung-Lektion**: Residency-Zähler akkumulieren wachen Idle im
  mode=gaming-Lock (~11,7 h-Sprung in der Pause); su-date-Stamps springen (NTP) — nur monotone
  Zähler (events/residency/hist-ts) für Analysen.
- **VENDOR-LADDER ENTTARNT (thermal.dump)**: mi_thermalds `TGAME-SS-CPU0/CPU6` war in JEDER
  Runde aktiv (schreibt cpufreq direkt), während vendor_sconfig=0/vendor_cpu_limits=none —
  **Mirror blind gegen den TGAME-Direkt-Pfad; der korrekte Vendor-Probe ist der dump-TGAME-
  Cluster, nicht vendor_sconfig (Monitoring-Lektion)**. Ladder (Onsets Virtual-Sensor):
  39.0→1612800, 40.0→1497600, 41.0→1324800, 42.0→1248000, 43.0→1017600, **Klippe ~46.50→576000**
  (geklammert 46492→1017 / 46512→576), darüber 1804800. **Bang-bang ohne Hysterese** (2-4-s-
  Flips an Stufengrenzen; der „milde“ Eindruck entsteht nur aus der Mittelung — Vendor-Verhalten
  dokumentiert). Ladder-Tiefe: r15 (laggy) bis 576 (~5 min @46.3-46.5 → **r15-Lag = Kernel-Dwell +
  tiefe Ladder, Synergie**); A flach (min 1324); B = monotoner Soak-Abstieg bis 1017 (36 % Boden-
  zeit, ≤1324 61 %). **Effective caps = min(Kernel-cap_now, Ladder-Stufe)**; Notifier klammert nur
  nach oben (k6a_gov.c:449-455, kein cpufreq_update_policy wegen Rekursion) → Vendor-tiefe Writes
  laufen durch; GPU-Caps bleiben kernel-only (Ladder touchiert nur CPU-Freq).
- **Ladder-Input = quiet_therm/thermal_zone74** (Korrelationstabelle: Δ −1,2…−2,1 °C; xo/pa/cpu
  ~10 °C drüber). **Blast-Radius-Spoof**: quiet_therm füttert alle 8 dump-Tag-Typen (SS/TGAME-SS +
  BCL/CCC/BOOST_LIMIT-Monitoring) → Spoof friert die komplette Userspace-Regie ein; Enforcement
  bleibt kernel-PMIC5 (CONFIG_QTI_BCL_PMIC5=y + QTI_BCL_SOC_DRIVER=y) → Spoof strikt ungesteckt.
  **Spoof = nur Fallback** (ungetestete Transferfunktion). Vendor-Configs verschlüsselt
  (thermal-tgame.conf & Co. — Jan-1-2009-Stempel täuschten; mi_thermald entschlüsselt zur Laufzeit,
  /data/vendor/thermal/config/ leer) → Ladder-Onset-Edit nur via Runtime-Capture. Beide Daemons
  non-critical (init.mi_thermald.rc, init.target.rc:203) → ctl.stop hält; init.target.rc schreibt
  boot-seitig sconfig=10.
- **Phase-A-Soak (11-16 21:37→21:59, 1326 s ≈ B, kein Lüfter, ungesteckt)**: Trigger beide erfüllt —
  Virtual-Sensor 46.6 (≥45.5) mit **582 s @ 576 MHz** (Klippe getroffen!) und **74,4 % ≤1248** (≥50).
  Stufen: 1804 19,2 % / 1612 3,2 / 1497 3,3 / 1324 3,0 / 1248 5,4 / 1017 22,0 / **576 43,9**.
  Kernel: +10 Events, L2 +721,7 s, L3 +389,4 s, L4 0, temp_peak 69. → B triggert.
- **Daemon-Stop-Test (11-17, 2 Runden, kein Lüfter, ungesteckt)**: settings.conf
  `turbo_stop_daemons=1` (:79) + Controller-Restart; Start-Log `turbo_stop=1 turbo_spoof=0` +
  `turbo: thermal-daemons pausiert`; **Punkt-7-Verifikation**: pidof-Scan sofort + nach 3 Ticks —
  kein Respawn. **Definitiver Beweis: 0 neue TGAME-Entries im Dump** (Marker Zeile 332).
  Kernel = alleiniger Limiter: +37 Events (~18,5/Runde), L2 +488,3 s, **L3 +792,1 s** (~3× vs B),
  **L4 +13,3 s** (4 kurze Episoden 2-6,6 s @86-87, Summe exakt 13307 ms), Recovery-Kaskade sauber
  (cd_l2>gaming @65 < 66), keine spürbaren Lags (User, „viel action“ — besser als B „hektisch“).
  Trade-off: ohne Ladder Floor = Kernel-Caps 1324 statt Vendor 576 → mehr Hitze → mehr Kernel-L2/L3,
  aber flache Caps = smooth Feel (die TIEFEN Stufen waren der Lag-Treiber, nicht die Event-Frequenz).
  L4-Touch = Argument für die Feinleiter (461): Interpolation l2g→l4g hält L4 als echten Notfall.
  **Entscheidung: turbo_stop_daemons=1 = Gaming-Default (persistiert, auch Daily/SS-Ladder aus);
  Revert-Option bleibt.** Termux-Hintergrund-Lauf störte nicht (hit/miss 0/0, pid=0, Caps
  policy-wide). Nächste Schritte: Build 460 (Spike-Filter >15 K/Tick + Silver-Caps cd_*_silver_max
  + quiet_therm-Telemetrie temp_quiet=), Build 461 (Feinleiter isoliert), ctl v1.6.7 (_auto_eval-
  While-Loop, _enter_daily-Lock-Fix, README :793 „≈1 s“→2 s; Spoof-Infra nur falls relevant).
- **Tool-Lektionen**: `pidof` matcht k6a-controller/Daemons hier nicht → tr-Scan (self-match
  einplanen, Pattern aus der Scan-Zeile selbst matcht mit); service.log lebt in
  `/data/adb/modules/k6a-ctl/config/service.log`; settings.conf-Edit immer mit `chcon
  u:object_r:system_file:s0` nachziehen. Files: `/data/local/tmp/a15r16a_{status,post}.txt`,
  `a15r16b_{status,post}.txt`, `a17soak_pre.txt`, `a17soak_t0.txt`, `a17soak_post.txt`,
  `a17soakT0b.txt`, `a17stop_post.txt`.

## 2026-11-17 II — Build 460 (Spike-Filter/Silver-Caps/quiet-Telemetrie) + ctl v1.6.7
- **Build 460** (`b32dad620`, gov **1.7.5**): (1) **Spike-Filter** vor der State-Entscheidung
  (`K6A_SPIKE_K=15`, Rate-Klemme vs `temp_smooth` + Spread-Klemme tp vs 2nd-highest — ein
  glitchiger Zonenwert kann L4 nicht mehr sofort ausloesen; `spike_drops=`-Telemetrie).
  (2) **Silver-Caps** im CD-Pfad: sysfs `cd_silver_caps` (L2/L3/L4 Hz, `0 0 0` = aus =
  Null-Verhalten), sofortiger Apply (kein Ramp), Notifier-Klammer auf die CPU-0-Policy.
  (3) **quiet_therm-Telemetrie** `temp_quiet=` in status (zone74, keine Steuerung).
  Host-Test: **31/31 Pruefungen gruen** (S1-S7b); Run rot nur wegen DOC-DRIFT (README fehlte
  1.7.5) → README nachgezogen (`7a57f2d9b`, md-only, verbraucht keine Build-Nummer); das rote
  X auf b32dad620 cleart beim naechsten Code-Push (461). build-kernel.yml hat paths-ignore
  '**.md' — README-Push triggerte keinen neuen Run ✓.
- **ctl v1.6.7** (`5c88157`, Gate 0 Warns, Zip 58 KB): _auto_eval-Cgroup-Scan als While-Loop
  (13-Zeilen-Kappe weg), `_enter_daily` laesst `_TURBO_LOCK` stehen (Notbremse-Sperre loest
  nur `_turbo_guard` via TURBO_REARM_C — sonst darf Turbo bei heissem Geraet beim naechsten
  Gaming-Entry sofort wieder scharf werden), **Daemon-Stop-Verifikation** (3 Ticks nach der
  Pause: pidof-Check → `turbo: Stop verifiziert` oder `WARN ... wirkungslos`; Race-Guard:
  `_TURBO_ACTIVE != 1` cancelt), README-Fix GAMING-Takt 2 s. Deploy zusammen mit 460 ok —
  alle Aenderungen verhaltenneutral fuer den Runden-Test (Loop = Robustheit, Lock-Fix =
  betrifft nur Notbremsen-Fall, Verify = Logging).

## 2026-11-17 III — Build 460 geflasht + Runden-Test (kernel-only, L4 194 s)
- **460-Flash verifiziert**: Banner `4.14.369-openela-rc1-...-build460-aosp` (GCC 13.3.0,
  PREEMPT), `k6a_gov v1.7.5 loaded` @11.7 s. Re-Arm über Flash ✓ (cd_thresholds 72/80/87/66
  + gpu_caps 800/680/500 + poll_adaptive 0 via v1.6.6-Persistenz). Neue Keys live:
  `spike_drops=0`, `silver_caps=0 0 0`, `temp_quiet=` (trackt). turbo_stop=1: Daemons
  pausiert direkt beim ersten Gaming-Entry nach Boot.
- **460-Runden-Test** (1 Runde, kein Lüfter, ungesteckt, kernel-only): `spike_drops=0`
  (Filter still — reale Anstiege unberührt ✓), Dump **bleibt 3 Zeilen** (0 TGAME-Entries
  ✓), silver_caps inaktiv ✓. **ABER: L4 +194,5 s** in EINER Runde (Stop-Runden: 13,3 s in
  ZWEI): ~10 bang-bang-Episoden @86↔78-79 (2,5-25 s, Ring evicted frühe), +30 Events,
  L2 +106 s, L3 +442 s, Ende temp 61/quiet **45** (Onset-39-Nähe: mit Vendor-Ladder wäre
  diese Runde tief in 1017/576 gewesen — Soak-Muster). temp_peak=62 (rolling, post).
  **Befund**: kernel-only Floor (L4 = 1324) wird ohne Vendor-Ladder und ohne Lüfter hart
  erreicht — die 1708/1555-Zwischenstufen halten 87 °C nicht auf. Das ist exakt das
  Argument für **Build 461 (Feinleiter)**: Interpolation l2g→l4g über 72-87 °C statt
  3 flacher Stufen → caps kommen GRADUell früher runter, 87-Klippe bleibt Notfall.
  Files: `/data/local/tmp/a17b460_t0.txt`, `a17b460_post.txt`.
- **Ausstehend**: ctl v1.6.7-Deploy (Zip in /data/local/tmp/), Build 461 (Feinleiter),
  Runden-Feel-Feedback des Users zur L4-Episoden-Last.

## 2026-11-17 IV — 460-Feel-Feedback + Build 461 (Feinleiter)
- **460-Runden-Feel (User)**: „wenn überhaupt minimale ruckler zum ende — minimal
  spürbar". 194 s L4 in ~10 kurzen Episoden also quasi unsichtbar. Der kernel-only
  Pfad (kein Vendor, kein Lüfter) ist damit subjektiv clean.
- **Build 461** (`b32dad620`-Nachfolger, gov **1.7.6**): **Feinleiter** — statt 3
  flacher Stufen mit Dwells an den Grenzen interpoliert der Gold-Cap piecewise-
  linear durch alle drei Anker: `l2g@cd_l2_temp → l3g@cd_l3_temp → l4g@cd_l4_temp`
  (1708800→1555200→1324800 über 72→80→87 °C; 72-80: 19,2 kHz/K, 80-87: 32,9 kHz/K —
  früh fangen, spät härter bremsen). L4 bleibt sofort + Backstop (Eskalation vor
  der Kurve, im L4-State keine Interpolation). **`cd_ladder`-Toggle** (sysfs, 0/1,
  default an) = A/B ohne Reflash (0 = exaktes Build459-Verhalten). Ramp (1 OPP/Tick
  runter, 1 OPP/4 Ticks hoch) glättet die Kurve weiter. State Machine unangetastet
  (Events/Hysterese/Residency/GPU/BW bleiben), nur der Gold-Cap-Zielwert wird
  temp-abhängig. Host-Test beim Push sofort grün (README 1.7.6 synchron).

## 2026-11-17 V — Review-Runde: Regelwerk-Doc (R1) + 461-Runden-Protokoll (R2)
- **R1 erledigt**: `docs/k6a_gov_rules.md` — die Entscheidungs-Schichten in
  Ausführungsreihenfolge pro Tick (Sensor-Akquisition KB8 → Spike-Filter → State
  Machine → Feinleiter → Ramp → Notifier-Klammer) + Feinleiter↔Spike-Filter-
  Semantik + „Was passiert bei …" (Sensor-Verlust, Ein-Tick-Glitch, reale
  Anstiege, **Bias-Grenze ehrlich**: +15 K absorbiert, Filter trackt dann mit,
  L4 auf biased t möglich — Grenze, nicht Bug) + **Battery-Guard als Constraint
  der State Machine markiert** (Blockade des CD-Exits, kein eigener Regelkreis).
  README: ein Satz + Link, keine Tabellen-Änderung. Reviewer-Korrektur:
  README-Drift-Claim war gegen den historischen Tabellenkopf (:78 v1.7.0 =
  Builds 421-428) gerichtet, aktueller Stand 1.7.6 überall verifiziert, Guard
  grün — erledigt. **Update-Pflicht**: das Doc wandert mit jeder künftigen
  Regelungs-Änderung im selben Commit mit (R4-Vermerk).
- **R2-Protokoll (461-Runde)**: Flash 461 → Verify (`cd_ladder=1`, Re-Arm,
  Stop-Verifikation im v1.6.7-Log) → Runde (~22 min, ungesteckt, gleiches
  Szenario). **Exit-Kriterien (2 Beobachter)**: (a) L4-Residenz ≪ 194 s
  (460-Baseline), (b) **Anteil Zeit ≥86 °C + Cap-Treppenwechsel/Min aus der
  data.txt-Serie** (2-s-Takt, temp + policy_max) — trennt „Kurve arbeitet" von
  „hängt ewig in der letzten Stufe bei 86". Dazu: Events/L2/L3-Deltas,
  `spike_drops=0` = PASS (der Filter-Arbeitsbeweis kommt deterministisch aus
  S8/462, nicht aus realen Glitches), kein TGAME-Entry. A/B-Fallback ohne
  Reflash: `echo 0 > /sys/kernel/k6a_gov/cd_ladder` = exaktes 459/460-Verhalten.
- **R3-Exit-Kriterium (462 = reiner Test-Infra-Build, schriftlich fixiert)**:
  S1-S7b bit-identisch ✓ · S8 (a: reale Rampen 2 K/Tick unberührt, b: +40-K-
  Sprung geklemmt auf smooth+15, c: Spread-Glitch tp→t+15, d: Bias-Absorption
  als dokumentierte Grenze) neu grün · S9 (cd_silver_caps/cd_ladder-Store-
  Validierung) neu grün · **smooth-immer-Update-Assertion** (verhindert die
  „nur-on-clamp optimieren"-Refactor-Katastrophe) · spike_drops im Live-Check
  = 0 · gov-Version unverändert 1.7.6 · data.txt-Charakteristik unverändert.
  Goldens aus der **eingefrorenen 461-Quelllogik** (Tick-für-Tick-Expected-
  Values, deterministische Integer-Arithmetik = Paritätsbeweis; Fault-Injection
  am Gerät nicht machbar). Kernel: `k6a_spike_filter()`-Helper + **Bias-Grenzen-
  Kommentar an ZWEI Stellen** (über dem Helper + über der Call-Site in
  gov_thread — kann nicht driften).

## 2026-11-18 VI — 461-Session: AnTuTu-Volltelemetrie + Leiter-Beweis + ctl-Zoo
- **Foreground-Sampler = kill-proof**: 501/501 Zeilen überlebt (18 min, 1 Tool-Call,
  sync) — vs. Background-nohup = tot nach 8,5 min (Phantom-Killer, jetzt mit
  Kandidat: BRENE's inotify.sh). Methode: sync-su-Loop in EINEM Bash-Call mit
  langem Timeout, Serie auf Gerät loggen.
- **AnTuTu hot-start (22:41-00:0x, ~18 min getestet)**: Start @ 71 °C (vorwärmt),
  komplett CD: 357 Samples cd_l2 + 143 cd_l3, 0 gaming. **CD_L4 = 0, temp≥86 =
  0 %, Peak 81/82 °C.** gold_cap_now-Verteilung = 10 Level VOLL gestreut
  (1555200×142 … 1708800×102, Zwischenstufen ×21-54) → **Leiter parkt nicht**
  (Reviewer-Diskriminator mit Real-Daten erfüllt). 242 policy_max-Wechsel exakt
  auf dem 19,2-kHz/K-Raster + 1 Sample @ 1522286 (= 32,9-kHz/K-Raster, L3-Band)
  → Interpolation sichtbar in beiden Bändern. Events +29, spike_drops=0 →
  **R2-Exit-Kriterium „spike_drops im Live-Check = 0" erfüllt.**
- **AnTuTu run1 (cool start, 22:06-22:21)**: Peak 69 °C, null CD; Dips 2:45/9:45/
  10:40 = Subtest-Täler (Kernel-Signatur: keine); CPU7 flat = unser 1709er-Cap
  (Diagramm-„1800" = Rundung) → **Baseline = gaming-capped, NICHT schedutil-rein**
  (User-Fazit bestätigt). Baseline-Doc-Werte 34-36 °C Akku = Pack-Indikator,
  Gold-Zonen liefen 55-82 (unsere Serie).
- **22:22-22:31-Pump-Session** (79-80 °C, L2/L3): = User-Modul-/TEE-Arbeit, KEINE
  Runden-Daten. Abend-Residenz gesamt: cd_l2 1174 s, cd_l3 412 s, **cd_l4 = 0**
  — die 87-°C-Klippe wurde nie erreicht. spike_drops=0 durchgehend.
- **ctl-Zoo aufgeklärt**: 1.6.7-ctl starb 22:04 (AnTuTu-App-Killer), Neustarts
  22:05:38 (Banner „1.6.6" = module.prop-Stale-Read während User-Modul-Churn —
  ctl liest module.prop live via sed), 22:10:56 + 22:13:55 wieder 1.6.7. PID
  5540 (seit 22:13:52) = 1.6.7 ✓, deployed Skript = alle 1.6.7-Features (9 Hits)
  ✓, module.prop = 1.6.7 ✓. Daemon-Stop hielt durchgehend (dump nie TGAME-SS).
  ctl überlebte den zweiten AnTuTu-Lauf diesmal.
- **brene (BRENE-SUSFS v0.0.76) + tricky_store (TEESimulator-RS v6.0.1)** =
  User-Integrity-Stack (22:13/22:16 installiert), keine k6a-Interferenz.
- **k6a_thermal-Floor-Konfig**: en=1, T1/T2/T3 = 85/90/95 °C, hard=100 °C —
  außerhalb Arbeitsbereich, nie wirksam.
- **R2 (CoD-Runde)**: gefroren bis User ausgeruht (User-Müdigkeit; AnTuTu passiv
  substituiert heute). **R3/462 vom User freigegeben, unabhängig von R2**: Test-
  Infra-Build per frozen protocol (Goldens aus eingefrorener 461-Logik, S8a-d +
  smooth-immer-Assertion + S9, S1-S7b bit-identisch, Version bleibt 1.7.6).

## 2026-11-18 VII — Build 462 (Test-Infra: Spike-Filter-Helper + S8/S9, kein Verhalten)
- **Commit `5075b3fcd`, gov-Version bleibt 1.7.6** (deklarierter Test-Infra-Build):
  Inline-Spike-Filter aus gov_thread T-Valid in **`k6a_spike_filter(int *t, int *t_peak,
  s32 *smooth)`** gezogen — bit-identisch (Rate-Klemme → Spread-Klemme → `*smooth = *t`
  immer → Return hit; `spike_drops++` am Call-Site). Bias-Grenzen-Kommentar an 2 Stellen
  (Helper + Call-Site), Regeldoc-Locator-Update (R4: Layer-2 zeigt jetzt den Helper).
- **Host-Test 31 → 149 Checks, 3 CI-Runs grün** (host 44204036011 / build 44204035891 /
  lockdep 44204035836), kein Release (Push-Run). S8a-d = Goldens aus der eingefrorenen
  461-Logik: (a) 2 K/Tick-Rampe 30 Ticks ohne Klemme, (b) +40-K-Sprung → t=55/tp=70
  (smooth 40), (c) Spread-Glitch tp=110 → tp=77 (t=62 real), (d) persistenter +25-K-Bias
  = Tick1 Absorption auf +15, Tick2+ Tracking. Boundary: +15 exakt passiert, +16 geklemmt
  (beide Achsen). **smooth-immer-Assertion** = 2-Tick-Rampe (faengt jeden
  nur-on-clamp-Refactor: smooth bliebe 0 → Tick2 wuerde clampen).
- **S9 Store-Validierung gegen den echten Code** (cd_ladder_store + cd_silver_caps_store
  extrahiert): kstrtoul-Stub mit kstrtox-Semantik **verifiziert an lib/kstrtox.c** —
  '+'-Skip, Basis 10, GENAU ein trailing '\n' toleriert, '-' → EINVAL (`echo N >` schreibt
  "N\n" → A/B-Fallback cd_ladder funktioniert, kein Bug). Tests: 1/0 ok, 2/abc/-1 →
  -EINVAL; silver-caps gueltig/partial-zero/aufsteigend/out-of-range/zu-wenige → -EINVAL,
  `0 0 0` = aus; gov_wake nur bei akzeptierten Writes. Stubs: mutex no-ops, gov_wake
  Counter, `#pragma GCC diagnostic` um extracted.h-Include (unused-parameter aus
  Kernel-Code, nicht global abgedreht).
- **Ferry erledigt**: aosp-ZIP (19 347 214 B, md5 `8e9ee7d87dc5436cb214c90cebe865c8`)
  in `/data/media/0/Download/Badazz-kernel-sweet-v4.14.369-build462-aosp.zip` — Flash +
  Verify beim User (Banner `build462-aosp`, status `version=1.7.6`, `spike_drops=0`).
  Lektion: `gh run download`-Filename hat Bindestrich (`build462-aosp`) ≠ Artifact-Name
  (`build462aosp`) — cp-Fehlermenge, nicht Run-Fehler.

## 2026-11-18 VIII — R2-CoD-Runde auf 461: PASS (L4 0 s vs 460-Baseline 194,5 s)
- **Setup**: 461 (gov 1.7.6, cd_ladder=1) behalten, 462 nicht geflasht (verhaltensidentisch).
  D1-Thresholds 72/80/87/66 + gpu_caps 800/680/500 + poll_adaptive=0, ungesteckt, kein
  Lüfter. T0 @53 C: throttle_events=0, residency cd_l2/3/4=0, spike_drops=0, Dump 9 Zeilen
  (0 TGAME), game_pid=0.
- **Beobachter**: Foreground-Sampler (Termux geschlossen) starb nach 7 min/204 Zeilen →
  verworfen. **Richtige Methode (AGENTS :256-272): badazz_iowatch KSU-Modul** — Watchdog
  PID 1869 (service.sh, Boot-Spawn, killer-immun) + `io_samp4.sh` PID 13715 triggert
  automatisch auf `game_pid != 0` (gp=15867 nach 1078 s Wartezeit), schreibt 1500 Samples
  1-Hz inkl. kstate/ktemp/kpeak/kpm/kth in `/data/local/tmp/io_r60r.log`. Serie = 1103
  Datenzeilen (t=65806..67191 = 1385 s) + `=== done 1500 ===`, rotiert als
  `/data/local/tmp/io_r60r_DONE_1795027416.log` (121 791 B) — **Hintergrund-Methode bewiesen**.
- **Exit-Kriterien (alle PASS)**: L4-Residenz **0 s** (Serie 0/1103 L4-Samples; Post-
  Snapshot residency `cd_l2:406111,cd_l3:779104,cd_l4:0`) vs 460-Baseline **194,5 s**;
  temp max **84 C**, Anteil **>=86 C = 0,00 %** (auch temp_peak max 84); gold_cap_now
  **13 Level** (1 423 543 … 1 708 800 Hz), **326 Cap-Wechsel/1385 s = 14,12/min**
  (Leiter parkt nicht); spike_drops=0; Dump 9 Zeilen/0 TGAME (Vendor-Engine still);
  throttle_events +10 (Post) / +8 (Serie) vs +30 (460). State-Verteilung: cd_l2 315 s,
  cd_l3 616 s, gaming 172 s.
- **User-Feel (R2, subjektiv)**: „sehr gute Runde", nur im **hektischen Finale minimale
  Stutters**. Bei langen Fahrten/Flügen und viel Feuer spürbar sinkende FPS — interpretiert
  als die graduelle Cap-Rampe des Feinleiters (genau der beabsichtigte Trade-off: keine
  194-s-L4-Bang-bang-Klippe mehr, dafür frühes sanftes Absenken). Geschätzter Durchschnitt
  **5-10 FPS niedriger, aber stabiler über die Runde** (subjektive Schätzung, keine FPS-
  Messung in dieser Runde — io_samp4 misst PSI/GPU/IO, nicht fps). Konsistent mit dem
  r14-D1-Eindruck „super flüssig" und dem 460-Runden-Feedback
  „minimale Ruckler zum Ende".
- **Lektion**: data.txt ist Snapshot (35 Zeilen), KEINE Zeitreihe — als 2. Beobachter
  nur io_samp4-Serie + Kernel-Snapshot nutzen. Foreground-Sampler nie bei geschlossenem
  Termux einsetzen.

## 2026-11-19 IX — Build 464 (Consensus-Gate) R2-Validierung: PASS (+5-10 fps subj.)
- **Setup**: Build 464 (gov 1.7.7, `ce7f8759`) geflasht; Verify: ko fresh 20:13,
  vermagic `build464-aosp`, `/sys/module/k6a_gov/version=1.7.7`, status
  `build_hash=ce7f8759`, `[SVC] k6a_gov 1.7.7 geladen` @20:14, Controller 1.6.8,
  `hash_verified=1`. D1-Thresholds 72/80/87/66 (user), cd_ladder=1, silver_caps=0 0 0,
  poll_adaptive=0. T0-Snapshot (`r2_before.txt`): gaming 2 723 673 ms, l2/l3/l4=0,
  throttle_events=0, spike_drops=0, ticks=10677, Temp 42-45 C.
- **Runde** ~21:13-21:36 (hist-Ring, 15 Transitions), Post (`r2_after.txt`):
  throttle_events **+7** (nur L2/L3, direkte gaming->l3@80 = Design), residency
  l2 +247 352 ms / l3 +702 364 ms / **l4 = 0**, spike_drops **0**,
  **dmesg Build463/Backstop: 0 Treffer** (Roh-Peak nie >=87 ueber 8 Ticks), hb-Max
  **81 C**, hist-Max 80, **>=86 C = 0 Samples**, Emissionen exakt an Schwellwerten
  (73>=72, 80>=80), Mindest-Abstand 2,8 s > 2-s-Dwell, kein Crash/Respawn,
  Boundary-Chatter um 72/80 in der Heissphase (11 Transitions/90 s) innerhalb der
  Hysterese — kein Flip-Bug.
- **Gate-Nachweis**: T lief bei echten Heizramps ungebremst durch (Transitions auf
  exaktem Schwellwert = keine Konsens-Blockade/Unterlesung), L4 blieb bei max 81 C
  bei 0 s — weder Unterreaktion noch Ueberreaktion; Klemmen kamen nie zur Anwendung
  (spike_drops=0 wie in 461).
- **User-Feel**: „eine bis zum Ende fluessige Runde, fps hoeher subjektiv 5-10 fps"
  gegenueber 461 (dort: 5-10 fps niedriger, aber stabil) — **R2 damit PASS**,
  subjektive Schaetzung, keine FPS-Messung (badazz_iowatch-Methode).
- **1-Hz-Serie** (`io_r60r_DONE_1795121151.log`, 1500 Samples, t=3583..5485 =
  1902 s, fertig 21:45 = Rundenende): Sampler lief waehrend der Runde mit
  (Watchdog 1739 + io_samp4, game_pid-Trigger). **max ktemp/kpeak = 86 C**,
  **>=86 C = 1/1500 Samples (0,07 %)** — eine 1-s-Exkursion, unter L4-Schwelle
  87, keine Dauerlast; hb-15-s-Sampling sah 0 (Hb-Max 81). State-Anteile
  **gaming 764 / cd_l2 194 / cd_l3 542 / cd_l4 0** — bestaetigt hist/l4=0 bei
  4x hoeherer Aufloesung als die 15-s-hb-Reihe.
- **Vergleich 461-R2**: L4 0 s = 0 s; throttle +7 vs +10; State-Anteile l2 247/l3 702
  vs 315/616 (1385-s-Fenster) — vergleichbare Heisslast, gleiche Null-L4-Bilanz.

## 2026-11-21 XI — Block D Boot-Test: build467 (clang+FORTIFY) = BOOTLOOP
- **Flash**: `/data/local/tmp/build467-clang.zip` md5 `c02acaca445d014137a5d48bc570a73a`
  via OrangeFox. **Bootloop** — Hypothese „Clang umgeht GCC13.3+4.14-FORTIFY-Loop"
  **widerlegt** (mindestens auf diesem Tree/Config). CI war grün (Compile-Pass ≠ Boot-Pass).
- **Recovery**: OrangeFox-Restore `boot-pre465.img` (war Stand **build464**, nicht 465 —
  pre465-Backup wurde vor dem 465-Flash gezogen). Danach kurz Chaos: kein Recents-Swipe,
  Lautstärke-Tasten tot, System träge/laggy — vermutlich Reste des unterbrochenen
  467-Boots + ggf. meines unterbrochenen AK3-465-Reflash-Versuchs (`reflash465.log`
  war bis „flash done" durch, Reboot-Tool-Call abgebrochen), NICHT ein Fund gegen 464/465.
- **Final-Stand**: User hat **build464-aosp** frisch geflasht. Läuft sauber:
  dmesg `build464-aosp`, boot_completed=1, k6a_gov 1.7.7 geladen
  (`/sys/kernel/k6a_gov/status`), k6a-ctl **1.6.10** `mode=gaming`, 0 Oops/Panic,
  SystemUI+Launcher aktiv. Observer io_samp4 neu gestartet (PID 5130).
- **Ergebnis D**: `experiment/clang-fortify` = **nicht mergebar** im aktuellen Stand
  (CI green, Boot fail). Offen: FORTIFY+GCC bleibt verboten; Clang+FORTIFY ebenfalls
  Boot-fail → FORTIFY auf diesem 414-Tree mit beiden Toolchains nicht tragfähig.
  Branch bleibt liegen als Referenz (Commits `3df0ae1be`+`490f20d95`).
- **Backup-Lage**: `boot-465-good.img` (md5 `6d…`) = laufendes 465 von vor dem
  Experiment — intakt für künftigen Restore; `build465.zip` md5 `64df9e8c…` vorhanden.

## 2026-11-21 XII — Build 465 (Hardening) R2-Validierung: PASS (60+ fps subj.)
- **Setup**: Nach Clang-467-Bootloop und kurzem 464-Zwischenstand Build **465-aosp**
  (`35023093f`) erneut geflasht (AK3, boot-new md5 `6cd9f349…` = `boot-465-good.img`).
  Verify: dmesg `build465-aosp`, `heap alloc:on`, `set_task_stack_end_magic`,
  `wbt=75000`, `softlockup_panic=0`, k6a_gov 1.7.7 `build_hash=35023093`,
  k6a-ctl 1.6.10 `mode=gaming`, Observer io_samp4 neu gestartet.
  T0: `/data/local/tmp/r2_465_before.txt`.
- **Runde** ~13:25–13:50 (Serie `io_r60r.log`, 1015 Samples t=515..1808 ≈ 21 min
  aktive Phase). Post: `/data/local/tmp/r2_465_after.txt`.
- **Gates (alle PASS)**:
  - **L4-Residenz 0 ms** (T0 wie Post `cd_l4:0`; Round-Delta gaming +892 s,
    cd_l2 +760 s, cd_l3 +0, cd_l4 +0)
  - **spike_drops=0**
  - **throttle_events +8** (1→9, nur L2-Transitions)
  - **dmesg**: Build463/Backstop/spike **0 Treffer**, 0 Oops/Panic
  - **Temp**: Serie max **79 C** (kpeak 80, 1 Sample ≥80), **≥86 C = 0**;
    hist 16 Transitions, Max **76 C**, kein Eintrag nach cd_l3/l4
  - Verlauf 53→68→79→62 C, thr-Counter 2→9 monoton
- **Vergleich 464-R2**: L4 0=0; throttle +8 vs +7; Tempmax 79 vs 81; ≥86 0 vs 0–1;
  spike 0=0 — keine Regression durch Härtung (INIT_ON_ALLOC/STACK_END/WBT).
- **User-Feel**: „gute Runde, 60+ fps relativ stabil" — **R2 PASS**.
- **Fazit Block C**: Härtung unter Gaming-Last unauffällig; 465 kann Dauerstand bleiben.
  R2-Dateien: `r2_465_{before,after}.txt`, Serie `io_r60r.log`.

## 2026-11-21 XIII — Cleanup: experiment-Branch gelöscht, Tag behalten
- `experiment/clang-fortify` (CI grün, Boot fail) von origin + lokal gelöscht.
  SHAs bleiben unter Tag **`d-clang-fortify-failed`** → `490f20d95`
  (Findings in Block-D-Eintrag, Devlog XI).
- Observer io_samp4 gestoppt; R2-Serie während Auswertung in Devlog XII
  festgehalten (Live-Log danach vom Sampler geleert — Kennzahlen dort).
- Stand Gerät: **build465-aosp** + Härtung, R2 PASS, k6a 1.7.7 / ctl 1.6.10.
- R2-Serie archiviert: `/data/local/tmp/io_r60r_DONE_R2_465.log` (1396 Zeilen).

## 2026-11-21 XIV — FastCharging-Modul deaktiviert (No-Op)
- `/data/adb/modules/FastCharging/` (FastChargingRyujin v1.1) als **disable** markiert:
  kein service.sh/Service-Hooks, system.prop leer, Scripts zielen auf Nodes die auf
  sweet fehlen (`/sys/kernel/fast_charge` etc.), thermald-conf = Stock.
  Kernel-eigenes FastCharge reicht: `BQ2597X_CHARGE_PUMP=y`, `QPNP_SMB5=y`,
  Plug-Log ICL bis 3 A / FCC 4,3 A / bq2597x master+slave.
- Reaktivierung nur zum A/B-Test: `rm …/disable` + Reboot, Ladeleistung messen.

## 2026-11-21 XV — Charge-Thermal A+B (Build 468): flachere FCC-Tabellen + CP bis L25
- **Auslöser**: User-Befund „mit FastCharging-Modul lädt es schneller" → Analyse:
  Modul-Scripts laufen nie (kein service.sh); einziger Effekt = `thermald-devices.conf`
  (`battery select_higher:1→0`). Live-Test: `system_temp_level=1` wird von mi_thermald
  in ~3 s auf 23 zurückgesetzt; `restrict_cur` wirkungslos bei `restrict_chg=0`.
  FC2 aus, weil `thermal_level 23 ≥ MAX_THERMAL_LEVEL 22`; FCC-Tabelle L23 = 1 A;
  charger_temp 57–59 °C (echte Heat, Thermal hat Recht).
- **A+B im Kernel** (Commit `38aef538a`, Run **44915170922** grün, Artefakte
  `…-build468aosp` / `…-build468miui`):
  - **A**: alle 8 DTS-Thermal-Tabellen in `sweet-sdmmagpie.dtsi` ab L14 flacher —
    L22 1.5→3.5 A, L23 1.0→**3.0 A**, L25 0→2.0 A, L26 0→1.0 A, L27 bleibt 0;
    L0–L13 unverändert; monoton nicht-steigend (28 Levels).
  - **B**: `MAX_THERMAL_LEVEL` 22→**26** (`cp_qc30.h`, `pd_policy_manager.h`) —
    Charge-Pump bleibt bis Level 25 aktiv; BYPASS enter/exit 20/22→24/26;
    `MAX_TEMP_LEVEL` 25→**27** (`smb5-lib.h`) — FCC-Vote greift auch auf L25/26.
- **Nicht geändert**: Thermal-Engine bleibt Autorität (mi_thermald schreibt weiter
  system_temp_level); kein Clamp im Kernel (Option C bewusst verworfen).
- Syntax-Check pristine-compared: cp_qc30/pdpm rc=0; smb5-lib 4 Pre-existing identisch.
- Flash-Quelle: `/data/local/tmp/build468.zip` (aosp). A/B erst mit kaltem Gerät
  (charger_temp < 45 °C), dann ibat/stl/FC2 gegen 465-Baseline.

## 2026-11-21 XVI — A/B Lademessung Build 468: PASS (stl 23 → 3 A + FC2 aktiv)
- **Flash ✅** (`/data/local/tmp/build468.zip`, aosp): dmesg `build468-aosp`,
  `git_hash=38aef538`, fsck 465→468, `thermal_levels:28`, k6a_gov 1.7.7 /
  k6a-controller + io-Observer live, cp_qc30_probe ok.
- **Live-DT verifiziert** (`/sys/firmware/devicetree/…/qpnp-smb5/`):
  `qcom,thermal-fcc-qc3-cp`, `thermal-mitigation`, `-dcp` jeweils **28** Einträge;
  L14…L27 = 5500/5000/4800/4600/4400/4200/4000/3800/**3500/3000/2500/2000/1000/0** mA
  (112 B, BE-u32). Damit greift A+B im laufenden Kernel.
- **A/B gegen 465** (gleicher Tag, Kabel USB-PD/PPS, `/data/local/tmp/charge_ab_468_summary.txt`):

  | Kennzahl | 465 (vorher) | 468 (nachher) |
  |---|---|---|
  | stl | 23 | 23 |
  | `thermal_fcc_ua` | 1 000 000 (1 A) | **3 000 000 (3 A)** |
  | ibat unter Last | ~1 A | **2.0–2.3 A** |
  | FC2/CP | aus (`MAX 22`) | **an** (`charge_enabled:1`, L23 < 26) |
  | `effective_fcc_val` | n/a (FC2 tot) | 1600–3000 (PD-Algorithmus) |
  | charger_temp | 57–59 °C | 51–57 °C während FC2 |
  | Exit | — | CV-Taper @ vbat≈4450 („taper charging done"), **nicht** thermisch |

- SoC-Pfad 72 % → 89 % im Capture; batt 33–35 °C; PPS-Request ~9 V / ibus 1.0–1.4 A.
- Baselines: `charge_t0_465.txt`, `charge_t0_468.txt`, Serie-Log im Summary-File.
- **FastCharging-Modul**: `/data/adb/modules/FastCharging/` ist **nicht mehr vorhanden**
  (nur noch disable → komplett entfernt). Kein Re-enable nötig; Kernel-BQ2597X bleibt
  alleinige FastCharge-Quelle (devlog XIV/XV).
- **Repo-Aufräumen**: lokaler NoMount-Stub `fs/nomount/Kconfig` (nur für altes
  olddefconfig; CI klont via setup.sh den echten Baum) → `.gitignore`.
- Offen (bewusst): Lade-Dauer-A/B 20→80 % erst bei entladenem Akku; Stress/R2 auf
  468 noch offen.
