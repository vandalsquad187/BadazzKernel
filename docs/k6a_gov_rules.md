# k6a_gov Regelwerk — wer entscheidet wann

Gültig für **gov 1.7.6** (Build 460/461).Dieses Doc beschreibt die
Entscheidungs-Schichten des Governors in **Ausführungsreihenfolge pro Tick**
(250 ms). Code-Referenzen: `drivers/thermal/k6a_gov/k6a_gov.c`. Das Doc wird
mit **jeder Regelungs-Änderung** mitgepflegt (AGENTS-Pflicht R4) — bei Drift
gilt der Code.

## Schichten pro Tick (in Ausführungsreihenfolge)

| # | Schicht | Entgegename | Wo |
|---|---|---|---|
| 1 | **Sensor-Akquisition** | `read_cached_temp()` liest Zonen °C (≤0 verworfen); Gold-Entscheidung = **2nd-highest of 4** Gold-Zonen (`t`), das Max geht separat als `tp` (Peak) weiter — KB8 | `read_temp()` :~480 |
| 2 | **Spike-Filter** | Rate-Klemme `t > temp_smooth + 15` → `t = smooth+15`; Spread-Klemme `tp > t + 15` → `tp = t+15`; `smooth` wird **immer** aktualisiert; Treffer zählt `spike_drops++` | `gov_thread()` T-Valid-Block :~1360 |
| 3 | **State Machine** | Escalation sofort (`tp >= cd_l4_temp` → L4; `t >= cd_l3_temp` → L3), De-escalation nur mit Hysterese (`hysteresis_normal` 2 s; L4-Austritt `tp + 3 < cd_l4_temp`); **Battery-Guard = Constraint dieser Schicht** (latched `batt_latched` blockiert den CD-L2→GAMING-Exit, kein eigener Regelkreis); Sensor-Verlust: 10 invalid Ticks → failsafe CD_L2 nur aus GAMING (gehaltene L2/L3/L4 bleiben) | `state_machine()` :~610 |
| 4 | **Feinleiter** | Nur in Zwischenstufen: Gold-Cap-**ZIEL** interpoliert piecewise-linear durch alle drei Anker — `l2g@cd_l2_temp → l3g@cd_l3_temp → l4g@cd_l4_temp` (72→80→87 °C). Im L4-State flach `l4g`. `cd_ladder=0` = flache 3-Stufen (Build459-Verhalten) | `get_cd_max_freq()`, `ladder_lerp()` :~345 |
| 5 | **Ramp** | Gold: `cap_now` 1 OPP/Tick runter, 1 OPP/4 Ticks hoch, L4/Entry sofort; Silver: sofort (kein Ramp) | `ramp_next_cap()`, `enforce_max_freq()` :~385 |
| 6 | **Notifier-Klammer** | gold: `cap_now` klammert Gold-Policies; silver: `silver_cap_now` klammert die CPU-0-Policy; **kein `cpufreq_update_policy()` im Notifier** (Rekursionsverbot) | `cpufreq_notify()` :~470 |

Parallel dazu (nicht sequenziell): GPU-Cap/Floor-Plan, BW-Floors, Battery-Guard-Sync
(ctl-seitig), Hash-Verifikation, Vendor-Mirror (`vendor_sconfig`/`vendor_cpu_limits`,
blind gegen den TGAME-Direkt-Pfad — Vendor-Probe = `thermal.dump`-TGAME-Cluster).

## Feinleiter ↔ Spike-Filter — Semantik

Der **Spike-Filter schützt den Eingang** (`t`, `tp`) vor Sensor-Artefakten.
Die **Feinleiter formt nur den Zwischenstufen-ZIELwert** der Gold-Caps.
**L4 bleibt von beiden unberührt**: Eskalation sofort aus dem (gefilterten)
Peak, im L4-State keine Interpolation, Austritt mit Margin. Der Filter
entscheidet also über *ob* L4 greift, die Leiter nur über *wie tief* die
Zwischenstufen decken.

## Was passiert bei …

- **Sensor-Verlust** (kein gültiger Wert): `invalid_ticks` zählt hoch; ab 10
 Ticks aus GAMING → failsafe CD_L2 (`sensor_failsafe_tick()`); gehaltene
 CD_L2/L3/L4 bleiben (bereits konservativ). Recovery automatisch, sobald
 eine Quelle zurückkommt.
- **Ein-Tick-Glitch** (ein Zonenwert springt plausibel-positiv): Spread-Klemme
 hält `tp` bei `t + 15` — L4 kann nicht aus einem einzelnen Ausreißer
 entstehen; der Ausreißer-Bereich kostet maximal `spike_drops++`.
- **Reale schnelle Anstiege**: alle Zonen steigen gemeinsam (< 15 K/Tick
 Spread, ~0,1-2 K/Tick Rate) → beide Klemmen schweigen, keine Latenz.
- **L4-Overflow / Filter-Overflow-Grenze (ehrliche Grenze!)**: Die Klemmen
 schützen vor **Ein-Tick-Sprüngen und Ein-Zonen-Ausreißern** — **nicht**
 vor persistentem Bias aller Zonen: ein +15-K-Bias wird beim ersten
 Tick absorbiert, danach trackt der Filter den Bias mit; ein L4-Eintritt
 auf biased `t` ist möglich. Cross-Sensor-Plausibilität wäre der Weg,
 das zu schließen — bewusst nicht gebaut (Aufwand vs. realer Fehlerklasse).
- **Battery heiß**: Guard-Latch blockiert den CD-Exit (Constraint in
 Schicht 3), bis der Akku unter die Entlatch-Temp fällt.

## Update-Pflicht

Bei jeder Änderung an Schichten 1-6 (Threshold-Semantik, Filter, Leiter,
Ramp, Notifier, Failsafe) dieses Doc im selben Commit nachziehen —
AGENTS-Pflicht (R4). Der Bias-Grenzen-Hinweis lebt zusätzlich als
Kommentar im Code (Helper + Call-Site), kann nicht driften.
