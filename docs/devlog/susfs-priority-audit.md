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

### #4 — Secondary-User-Normalisierung (INFO, Testfall offen)

`uid % 100000` (Z. 317) mappt Work-Profil-/Secondary-User-UIDs (1010000+)
auf den App-ID-Bereich — gewollt (Commit 82837be27: „Sekundaer-User").
Korrekt für alle realen Android-UID-Räume; statische Prüfung OK.
**Offen:** A/B-Test auf dem Gerät (App im Work-Profil → ESRCH-Paar)
fehlt im Testprotokoll — nachzuholen.

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

## Verify (478)

- `clang -fsyntax-only` pristine-compared (Rezept
  `local-build-notes.md`), Include-Set um SUSFS-Pfade ergänzt.
- Live-Checks nach Flash: Banner/`git_hash`, SET rc=0 + LOGI,
  ESRCH-Paar (gefilterte App), Renew-Smoke (SET + pkg-Event →
  „resolved again“ + kein Stale-Overwrite).
