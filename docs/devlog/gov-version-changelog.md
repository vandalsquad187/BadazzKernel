# k6a_gov Version-Changelog (README-Archiv)

> Ausgelagert aus `README.md` beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> Fruehere README-Subsections unter Architecture/Kernel Features.

### k6a_gov v1.6.0 hardening fixes (Builds 397/398, Oct 2026)

Fixes 1–10, all in-tree: `build397` shipped 1–7 + unified `pr_fmt`, `build398` added 8–10.

| # | Fix | Where |
|---|-----|-------|
| 1 | Gold clamp honours `user_policy.max` releases | `enforce_max_freq()` |
| 2 | `enable=0` also resets the GPU floor | `enable_store()` |
| 3 | GPU floor: captured user min restored, never re-capped | `gpu_floor_reset()`, `enforce_gpu_floor()` |
| 4 | Battery guard: sane temperature on sensor error, decoupled from `temp_valid` | `read_bat_temp()`, `battery_guard_tick()` |
| 5 | Init: `schedule_delayed_work` only after `kthread_run` (UAF fix) | `k6a_gov_init()` |
| 6 | `game_pid`: strict `kstrtoint` + `PID_MAX_LIMIT` bound; now echoed in `status` | `game_pid_store()`, `status_show()` |
| 7 | Governor struct `static`, module params 0644→0444, unified `pr_fmt` (`k6a_gov: ` prefix) | file-wide |
| 8 | L4 down-step only when `peak + 3 K < cd_l4_temp` — kills the escalate/retreat flap loop | `K6A_L4_MARGIN` |
| 9 | `profile=0/off` forces state OFF immediately (no more resurrection by `thresholds_user`) | `state_machine()`, `profile_store()` |
| 10 | GPU cap: remembers the user `max_pwrlevel` (`gpu_cap_saved_max`) and restores it on release (was a hard idx 0) | `enforce_gpu_cap()`, `gpu_reset()` |

### k6a_gov v1.7.0 + SUSFS round 1/2 (Builds 421–428, 2026-10-31)

| # | Feature / Fix | Where |
|---|-------|-------|
| — | **Time-residency accounting**: `residency_ms=off,gaming,cd_l2..l4` in `status`, `stats_reset` node | `k6a_gov.c` |
| — | **GPU-cap ramping** (thrash killer): tighten 1 OPP/tick, release 1 OPP/4 ticks (`K6A_RAMP_UP_TICKS=4`), L4/entry/table-miss immediate; `gold_cap_now` in `status`; notifier clamps to the *currently ramped* cap; `cap_now` reset at enable/profile/board-reset | `ramp_next_cap()` |
| — | SUSFS round 1: UAF in `susfs_add_sus_kstat/add_sus_path_loop` (added-flag so the user-error path can no longer free published entries), `kern_path`/`path_put` out of RCU, string termination on all user-path fields + uname spoof, **cmdline overflow fix** (fake cmdline truncated to the real `saved_command_line` size) | `fs/susfs/*` |
| — | SUSFS round 2: **KSU-domain passthrough** (sus_path/sus_mount/kstat-spoof see nothing hidden for `susfs_is_current_ksu_domain()` — root/ksud keep the truth, untrusted apps stay blind; **on-device proven**), `SUSFS_LOGI/LOGE` do-while(0), **dup-safe adds**: path-loop idempotent (cap 256, `-ENOSPC`), kstat 1024/`-EEXIST`, open-redirect 256; `update_sus_kstat` → `-ENOENT` on unknown inode; new `CMD_SUSFS_DEL_SUS_PATH` (0x55554) + `susfs_del_sus_path()` (fork wiring `79003ef8`) | `fs/susfs/*`, fork |
| — | KSU fork hardening: atomic diag-bit ORs (`KSU_DIAG_SET`, 10 sites), `fd_idx < max_fds`, ratelimits/caps in supercall dispatch, 1/s throne gate, `linux/ratelimit.h` | fork |
| — | CI: action refs pinned to commit SHAs + dependabot, host test (23 checks) in `tools/k6a_gov_host_test/` with `host-test.yml`, `out/` untracked (repo slimmer), `susfs*.v210` legacy removed | `.github/`, `tools/` |

> Device note: The blackout of 2026-10-31 was diagnosed as a failed display-resume after the device was kept awake ~10 h by an open CODM lobby (kernel ran stable the whole night, L4 never engaged, `residency_ms` proved 9 h gaming state). Not a kernel regression; kill the game before sleeping.

### k6a_gov v1.6.1 / v1.6.2 (Builds 402–404, Oct 2026)

| # | Fix | Where |
|---|-----|-------|
| — | Adaptive polling: `poll_adaptive` (default on), slow-poll OFF/battery-cool, instant fast on +3 K trend | `gov_thread()` |
| — | ftrace tracepoint `k6a_state` (`from`/`to`/`temp`) on every state transition | `trace.h`, `set_state_locked()` |
| 11 | GPU cap/floor **read-back**: live `max_idx`/`min_idx` compared against the target every tick — if KGSL (`opp_notify`) loosened our limit, it is re-applied (ratelimited log); stricter external limits are never relaxed | `enforce_gpu_cap()`, `enforce_gpu_floor()` |
| 12 | Race-free unload: sysfs + cooling device removed first, `enabled=false` under lock, thread sleep is `kthread_stop()`-wakeable (no poll_ms stall on rmmod), all limits released under lock afterwards | `k6a_gov_exit()`, `gov_thread()` |

CPU side (Build 401): `cpu_cooling.c` thermal floor clamp + `k6a_thermal` sysfs (see below).

### k6a_gov v1.7.4 — sensor fail-safe + release hardening (Build 454, Nov 2026)

| # | Feature / Fix | Where |
|---|-------|-------|
| — | **Sensor fail-safe**: 10 consecutive ticks without any temp source while held in GAMING force CD_L2 (the escalation fail-open — a dead sensor previously meant "hold last state", i.e. unthrottled heating). Recovery is automatic once a source returns; a held CD_L2/L3/L4 is already conservative and is kept | `sensor_failsafe_tick()` |
| — | `profile=0` releases the Gold CPU cap immediately (`cap_now=0`, `apply_policy()`) instead of ramping down 1 OPP/tick — matches what `enable=0` has done since Build 446. GPU cap releases via the next plan tick | `profile_store()` |
| — | Hotplug: if the cached gold CPU is offline at readback time, `find_gold_cpu()` re-resolves fresh (previously `p==NULL` silently skipped the verification) | `enforce_max_freq()` |
| — | One-time boot log of every temp source (`cpu-1-0..3-usr`, `cpu-0-0-usr`, `xo-therm`, `soc-therm`, `thermal_zone0`) with id + type — a firmware rename is visible in the first dmesg, not only as a symptom | `freq_init_worker()` |
| — | `hist` moved to its own node (Build 450) and `hysteresis` defaults corrected in this README (`fast` 1..5000, default 3000 since Build 347) | docs |

### k6a_gov v1.7.5 — spike filter + silver caps + quiet telemetry (Build 460, Nov 2026)

| # | Feature / Fix | Where |
|---|-------|-------|
| — | **Spike filter before the state decision**: a single glitchy sensor read with a plausible positive value used to arm L4 immediately (`tp >= cd_l4_temp`). Two clamps — (a) tick-rate vs the filtered reference (> 15 K per 250 ms tick discarded, real ramps are ~0.1–2 K/tick), (b) zone-spread vs the 2nd-highest (an outlier more than 15 K above the vote is clamped). No latency for real ramps; counter `spike_drops` in `status` | `gov_thread()`, `K6A_SPIKE_K` |
| — | **Silver caps in the CD path** (`cd_silver_caps` sysfs, `L2 L3 L4` Hz, `0 0 0` = off): symmetric to Gold — applied immediately (no ramp; the vendor also throttled cpu0 directly), notifier clamp on the CPU-0 policy, released on CD exit. Silver stays in stock/vendor regime while off | `get_cd_silver_max_freq()`, `enforce_max_freq()`, `cpufreq_notify()` |
| — | **quiet_therm telemetry** (`temp_quiet=` in `status`): zone74 is the dominant input of the vendor TGAME ladder (39.0 onset / ~46.5 cliff) — live proximity monitoring, no control decision on it | `status_show()` |

### k6a_gov v1.7.6 — fine-grained CD ladder (Build 461, Nov 2026)

| # | Feature / Fix | Where |
|---|-------|-------|
| — | **Fine-grained CD ladder** (`cd_ladder` sysfs, default on): instead of 3 flat cap steps with dwell flip-flops at the boundaries, the Gold cap interpolates piecewise-linear through all three anchors — `l2g @ cd_l2_temp → l3g @ cd_l3_temp → l4g @ cd_l4_temp` (e.g. 1708800 → 1555200 → 1324800 over 72→80→87 °C). The existing threshold tuning stays meaningful; L4 stays immediate + backstop. `cd_ladder=0` restores the flat Build459 behavior (A/B without re-flash). Cap ramping (1 OPP/tick down, 1 OPP/4 ticks up) still smooths the curve | `get_cd_max_freq()`, `ladder_lerp()` |

The full decision layering (who decides when: sensor acquisition → spike filter → state machine → ladder → ramp → notifier, incl. sensor-loss and filter-limit behavior) lives in [`docs/k6a_gov_rules.md`](docs/k6a_gov_rules.md).

