# ctl companion (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> k6a-ctl-Kapitel: Magisk-Modul-Rolle, Companion-Details, v1.4.9-Runde

## k6a-ctl (Magisk Modul — integraler Bestandteil des Kernel-Pakets, User-OK 2026-10-29)
- **Pfad**: `/data/adb/modules/k6a-ctl/` (v1.4.6, id=`k6a-ctl`, Autor BadaZz89).
  **Gehoert ausdruecklich zum Kernel dazu** (User-Anweisung: "merke dir k6a-ctl, es
  gehoert zum kernel dazu") — beim Kernel-Bau/-Flash denken, nicht nur badazz_iowatch.
  **GATE-FALLE (2026-10-30 real passiert, build421)**: ko-Version uebersteigt das Gate
  -> Modul laedt NICHT, /sys/kernel/k6a_gov fehlt, k6a-ctl faellt in "reiner Legacy-
  Betrieb" ohne Fehler-toaster. Nach jedem k6a_gov-Version-Bump: service.sh `kver_ok()`
  erweitern (z.B. `1.5.*|1.6.*|1.7.*`) + `GOV_KO_VER` setzen; Notfall-Reload:
  `insmod /data/adb/modules/k6a-ctl/k6a_gov.ko` (Controller-HB merkt es im naechsten
  Tick, "legacy -> 1 (delegated=1)").
- **Rolle**: Userspace-Begleiter des in-kernel `k6a_gov`:
  - `service.sh`: **laedt k6a_gov.ko** beim Boot (`_load_gov`, Kandidatenpfade
    `$MODDIR/k6a_gov.ko` -> /data/adb -> /system/lib/modules -> /vendor/lib/modules) mit
    **Versions-Gate**: akzeptiert nur ko-Version 1.5.x/1.6.x (`strings`-Check) — bei
    neueren ko-Versionen gate anpassen (GOV_KO_VER).
  - schreibt persistente/idempotente `cd_thresholds 96/99/102/93` (-> status
    `thresholds_source=user`), Gold-Cluster-Cap-UI-Toggle, schedutil-Tuning,
    `thermal_protect` (pausiert Gaming-Deckel).
  - `bin/`: `k6a-controller`, `k6a-lib.sh`, `webui-server.sh`/`webui-handler.sh`
    (localhost **WebUI**), Backups `*.bak-v142..146`.
  - `config/settings.conf` (single-source config), `config/service.log(.1/.2)` (100 KB
    Rotation), `webroot/`, `run/`, `sepolicy.rule` (kernel -> sysfs read/open etc.).
  - ships eigenes `k6a_gov.ko` (wird bei jedem Kernel-Release-Flash mit der ZIP-Version
    aktualisiert — mit insmod durch service.sh).
- **Aufteilung klären, wenn State-Fragen kommen**: State-Maschine/Game-Detect/Floors/Temp
  = **Kernel-Modul** (k6a_gov.ko, /sys/kernel/k6a_gov); Konfig/Thresholds/Schedutil/WebUI
  = **k6a-ctl** (Userspace). "gaming ohne Match" = Game-Detect sieht offene Lobby (z.B.
  com.activision.callofduty.shooter PID offen) — korrekt, kein Phaenom.

## k6a-ctl Companion
- **Repo**: `vandalsquad187/k6a-ctl` branch `main` @ **`768d1bc` (v1.3.2, versionCode 132)**,
  release `v1.3.2` → asset `k6a-ctl-v1.3.2.zip` (23 456 B), local clone `~/k6a-ctl`.
  Previous: `eec3d96` (v1.3.1, sepolicy.rule + gate `[5c]`), `6317a27` (drop the stale
  `LINUX_VERSION_CODE` hint), `1513fd8` v1.3.0 (130),
  `426b516` v1.2.0 (120) → `k6a-ctl-v1.2.0.zip` (20 543 B).
  Recent commits: `768d1bc` **v1.3.2** (F1 force `schedutil` + `_ensure_gov` drift re-assert,
  F8 `w_checked`, F9 `cfg_load` single pass, F10 `read_temp` real return code, F11 F12 F13
  per-tick cost cuts), `eec3d96` v1.3.1 (sepolicy.rule + gate `[5c]`), `6317a27` (warning text),
  `1513fd8` v1.3.0 (Build 341: insmod + version lock), `426b516` v1.2.0 (Phase 5),
  `a049fac` v1.1.6 version format,
  `a9170b6` v1.1.5 **removed the USB autosuspend workaround** (so userspace no longer masks a
  kernel bug), `de3c037` v1.1.4 dwc3 autosuspend off, `18cf392` v1.1.3 log rotation +
  `battery_guard_temp`, `a639e7c` v1.1.2 robustheit, `e43ccf6` v1.1.1 whitelisted handler.
- **v1.3.2 is on the device** (deployed 2026-10-03, six files piped through
  `su -c "cat > …"` so SELinux contexts survive; backup `/data/adb/k6a-ctl-v1.3.1.bak`):
  `module.prop` `version=1.3.2` / `versionCode=132`, controller logged `k6a-ctl v1.3.2 start`,
  gate `[1]`–`[6]` all OK (incl. the new governor checks), `scaling_governor → schedutil` on
  `p0`/`p6`, a manual flip to `performance` was back on `schedutil` within one tick, and the
  WebUI mode switch (cfg hot-reload) works end to end.
- **v1.3.1 is on the device** (read 2026-10-03): `module.prop` `version=1.3.1` /
  `versionCode=131`, `sepolicy.rule` root-owned 94 B, controller logged
  `k6a-ctl v1.3.1 start`, `/ping` → `OK`.
  **ksud's `module install` is unusable here** — `su -c` runs with `CapBnd=0`, so it fails with
  `Error: Permission denied (os error 13)`; upgrades are done by unpacking to `/data/local/tmp`
  and **`mv`-ing each file over** (root-owned, so `chmod` works), which also leaves the running
  scripts' inodes intact. Backup of v1.2.0: `/data/local/tmp/k6a-ctl-v1.2.0.bak`.
  `data.txt` carries `gov_policy_max`, `gov_state_age`, `gov_temp_src=1`, `gov_temp_valid=1`,
  `gov_batt_guard=1`, `gov_game_pid=0`, `gov_version=1.5.0`, `gov_hash_state=1`.
- **`check_module.sh` is a build gate, not a shipped tool** — `build.sh:17` deliberately excludes
  it (with `build.sh` itself) from the ZIP. Earlier notes framed its absence on the device as a
  defect; that was wrong. Running it by hand is repo-side only:
  `sh <repo>/bin/check_module.sh <repo>`. Gate result 2026-10-03 (v1.3.1): **green, 0 warns**
  including `[5b] Version-Lock k6a_gov` and the new `[5c] sepolicy.rule` check (file present,
  `allow kernel sysfs file {read open getattr}` present, no `:` anywhere in it).
  **v1.3.2 renumbered the gates to `[1]`–`[6]`** (the old `[5*]` numbering had gaps and the
  new governor checks needed room): result 2026-10-03 (v1.3.2) **green, 0 warns**, `[6]` being
  the governor block (`_ensure_gov()` in `k6a-lib.sh` *and* `k6a-controller`, the
  `scaling_governor` write, `w_checked()`, no `echo 40` in `read_temp`). Mentions of `[5b]`/
  `[5c]` further down describe the v1.3.0/v1.3.1 numbering and are kept as written.
- **Phase 5 shipped (`426b516`, v1.2.0)**:
  - **BW-floor payload order bug found + fixed.** `applyBwFloors()` built the string interlaced
    `[gpubw_L2, llcc_L2, gpubw_L3, llcc_L3, gpubw_L4, llcc_L4]` while `bw_floors_store()` parses
    grouped `sscanf("%u %u %u %u %u %u", &gpubw_l2..llcc_l4)` → **4 of 6 values landed wrong**,
    silently (`bw_floors` has no monotonicity validation). Now `g.concat(l).join("_")`.
    `resetBwFloors()` defaults were the same interlaced nonsense → replaced by the gaming profile
    `dg=[4000,3000,1500] dl=[0,4000,3000]`, verified against the live node
    (`bw_floor_gpubw=4000 3000 1500`, `bw_floor_llcc=0 4000 3000`).
  - **Single authority**: k6a-ctl now writes **only** `legacy` (`delegated=1` → 1, else 0) and
    never races `scaling_max_freq`/`bw_floors` against the kernel. `_LEGACY_SYNCED` guards rewrites.
  - **`battery_guard` honoured**: config key `battery_guard=on|off` (default `on`) syncs the node;
    WebUI toggle via `/battguard?e=0|1` alongside the existing `?t=35..60`. Kernel default is
    **0** (`kzalloc`), so until now the threshold was stored but inert. Wire-up is
    `k6a_gov.c:576`, gated on `state == K6A_GAMING`.
  - **`game_pid`**: k6a-ctl writes `pidof $game_pkg` only on change (not per 1s tick) and `0` on
    leaving GAMING. Kernel side is store+echo only (`k6a_gov.c:808/:815`), never acted upon.
  - **Build-340 status surfaced**: `policy_max`, `state_age_ms`, `temp_src`, `temp_valid` parsed
    from `/status` into `data.txt` and the WebUI (Gold cap in MHz, State age in s, temp-source
    code table 0 none / 1 Gold / 2 Silver / 3 xo / 4 soc / 5 zone0, plus an explicit
    `temp_valid=0` → "keine Quelle, State gehalten" state). All four are empty on a pre-340
    kernel — extraction regexes checked against the live 1.3.1 `status`, no false matches.
  - `_startup` logs `k6a_gov <version> legacy=<n>` and warns unless the version matches `1.5.*`.
- **Phase 6 shipped (`1513fd8`, v1.3.0)**:
  - `service.sh` runs `_load_gov()` before the boot_completed wait: `insmod` from
    `$MODDIR` / `/data/adb` / `/system/lib/modules` / `/vendor/lib/modules`, then a
    three-stage version lock (`.ko` `.modinfo` → runtime `status` → kernel vermagic).
    `GOV_KO_VER=1.5.0`; every failure logs one WARN and returns, so the controller's own
    CD_L2/L3/L4 state machine is the permanent fallback — never a brick.
  - `gov_hash_state` (0/1/2/3) surfaced in `data.txt` and in the WebUI "Build Hash" row,
    which can now say "nicht geprüft" instead of pretending `hash_verified=1` means checked.
  - `bin/check_module.sh` gate `[5b]` fails the build when `service.sh`'s `GOV_KO_VER` and
    the controller's `1.5.*` expectation drift apart.
- **Mode**: `delegated=1` — Kernel handles thermal (CPU/GPU/BW), module handles game detection +
  sched tuning + auto `badazz_safe` @85°C
- **WebUI**: Gov-Status (+ policy_max/state_age/temp_src/temp_valid), BW-Floors, GPU-Caps,
  History timeline, Akku-Guard toggle
- **Do not re-enable before Build 340** *(historical — done)*: on v1.3.1 the Gold cap and the
  BW floors were never released (see *Pre-fix evidence*), so a userspace cooldown would have
  fought the kernel. The `disable` file was dropped after the Build 340 acceptance run.
- **TEIL 3 done**: the governor version string is now burned into both sides —
  `GOV_KO_VER=1.5.0` in `service.sh`, `1.5.*` in the controller, `K6A_GOV_VERSION "1.5.0"`
  in the kernel — with `check_module.sh` gate `[5b]` keeping the two shells in sync, plus
  vermagic as the kernel-side lock.

## k6a-ctl v1.4.9 (2026-11-05, von P1.4.6/P4-Runde + Reviewer-Befund deployed)
- **Anlass**: Reviewrundes Anti-Nachfrage (5 Befunde, geprüft, teils am Gerät gelöst):
  1. Versionsgate im **Repo** (1.5.*|1.6.*) veraltet — am Gerät seit 2026-11-01 P1 (dynamisch 1.[5-9].*) bewiesen; **Repo-Sync steht noch aus** (local snapshot ~/tmp/k6actl/).
  2. WebUI-ohne-Auth: localhost-Status toppen (`any app` mit INTERNET kann) → **Token-Gate** (`config/webui.token` root-only, `&k=` an jede state-changing route; `app.js` Fallback-Kette mit Token, `api()` single wrapper; sed-Bake durch service.sh beim Boot).
  3. Controller-Hitze ~100 proc/s (36 Subshells + 46 pipes pro data.tx) → **Fork-Diet**: Status-while-read direkt (0 forks), cfg_load-Diet-Stat-Sig, direct Node-Reads (scaling_cur/battery/gov_read-ersatz), data.txt 2s-Takt (5s-Takt im daily, force bei State-Wechsel), nice 0.
  4. setcfg-Misssteckbar in Cats → **`mv -f`-Atomik** (Fallback cat bleibt).
  5. mode=auto gemäßReviewer: `pidof + /proc/<pid>/cgroup top-app` + Hysterese (2 Ticks rein, 10s ohne top-app raus → DAILY). am Gerät bewiesen: `-> DAILY` @ 11:56:53 beim Mode-Wechsel, enable=0, game_pid=0 ✓.
- **device-deploy 11:53–11:57 (kein reboot)**: backups `.bak-v148-p9` (instant old prior `/bak-v146-*`), controller/webui-handler/service/app.js/module.prop settings `profile=badazz_safe` durch Test-GET (gleich mit R3-A-Profil), `mode=auto` verifiziert `→ settings` geschrieben.
- **R3-arm-Kontext vor Spiel**: thresholds `96 99 102 93 1708800 1555200 1324800`, gpu_caps `585/430/305`, hysteresis `500/2000`, battery_guard_temp=47 / guard=off, mode=auto (residency misst jetzt nur echtes Game/top-app), cod_run.sh Sampler lebt.
- **Repo-Sync erledigt (2026-11-05)**: k6a-ctl GitHub repo `vandalsquad187/k6a-ctl` main @ `275aa4d` (= v1.4.9), Release `v1.4.9` + zip `k6a-ctl-v1.4.9.zip` (39.613 B, GATE OK 0 warns). Release-Defaults: repo settings.conf bleibt v1.4.8-Nummern mit mode=gaming + Auto-Doku-Kommentar; webroot/app.js im Release mit `const KTOKEN=''` (service.sh bäckt das gerätespezifische Token beim Boot). check_module.sh: mode-case akzeptiert auto + Version-Lock-Dynamik (`grep -qF '1.[5-9].*'` — grep-Klasse-Falle minus -F schlug bei literalen [5-9] fehl).
- **Watchout**: auto-Eval nutzt je Tick `pidof` (1 fork/1s im idle — ok); token-flush nicht zyklisch, bei Reboot erneuert service.sh nur wenn File fehlt; Controller-Enter setzt `profile=badazz_safe (idx5)` — valide bis R3 ABGESPIELT word (kein manuelles echo mehr nötig).
- kQ-tests: handler `-n sh -n` via on-device sh-OK, handler OK via termux sh/na token-Tests: /ping 200 ohne; /mode /profile/Falsch-Token 403 richtig; Profil-Toggle mit k= 200 setzende-Tests sind echte Button — Profil+mode finals in settings (verifiziert).
