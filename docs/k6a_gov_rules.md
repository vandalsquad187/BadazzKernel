# k6a_gov Regelwerk — wer entscheidet wann

Gültig für **gov 1.7.7** (Build 463).Dieses Doc beschreibt die
Entscheidungs-Schichten des Governors in **Ausführungsreihenfolge pro Tick**
(250 ms). Code-Referenzen: `drivers/thermal/k6a_gov/k6a_gov.c`. Das Doc wird
mit **jeder Regelungs-Änderung** mitgepflegt (AGENTS-Pflicht R4) — bei Drift
gilt der Code.

## Schichten pro Tick (in Ausführungsreihenfolge)

| # | Schicht | Entgegename | Wo |
|---|---|---|---|
| 1 | **Sensor-Akquisition** | `read_cached_temp()` liest Zonen °C (≤0 verworfen); Gold-Entscheidung = **2nd-highest of 4** Gold-Zonen (`t`), das Max geht separat als `tp` (Peak) weiter — KB8; **Build463** liefert zusätzlich `consensus`: jede lebende Gold-Zone steigt gegenüber dem Vortick (tote Zone zählt nicht, eine hängend-hohe sperrt); Fallback-Quellen: eigener Vortick-Wert | `read_temp()` :~553 |
| 2 | **Spike-Filter** | Rate-Klemme `t > temp_smooth + 15` → `t = smooth+15`; Spread-Klemme `tp > t + 15` → `tp = t+15`; **Build463 Konsens-Gate**: `smooth` zieht **aufwärts nur bei Konsens** (alle lebenden Gold-Zonen steigen gemeinsam), abwärts und beim Bootstrap (`smooth==0`) immer — ohne Konsens friert die Referenz ein und die Rate-Klemme absorbiert einen Bias dauerhaft („Bias eingefroren“ statt mitgetrackt); Treffer zählt `spike_drops++` | `k6a_spike_filter()` :~1422 (gerufen aus dem `gov_thread()` T-Valid-Block) |
| 3 | **State Machine** | Escalation sofort (`tp >= cd_l4_temp` → L4; `t >= cd_l3_temp` → L3), De-escalation nur mit Hysterese (`hysteresis_normal` 2 s; L4-Austritt `tp + 3 < cd_l4_temp` **und** `temp_peak_raw + 3 < cd_l4_temp` — Build463: der Roh-Peak hält L4, bis auch er abgekühlt ist, sonst flaegt die Backstop-Eskalation sofort wieder); **Battery-Guard = Constraint dieser Schicht** (latched `batt_latched` blockiert den CD-L2→GAMING-Exit, kein eigener Regelkreis); Sensor-Verlust: 10 invalid Ticks → failsafe CD_L2 nur aus GAMING (gehaltene L2/L3/L4 bleiben); **Build463 L4-Peak-Backstop**: Roh-Peak (vor dem Filter) `>= cd_l4_temp` in 8 aufeinanderfolgenden Ticks (2 s) erzwingt L4 auch dann, wenn der gefilterte `tp` durch die Klemmen/eingefrorenes `smooth` unter der Schwelle klebt — Reset bei Tick-Abfall, Sensor-Ausfall und Profil-off | `state_machine()` :~665 |
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
Peak, im L4-State keine Interpolation, Austritt mit Margin — plus den
**Peak-Backstop aus dem Roh-Peak** (Build463), damit ein eingefrorenes
`smooth` keine L4-Latenz offenlässt. Der Filter entscheidet also über *ob*
L4 greift, die Leiter nur über *wie tief* die Zwischenstufen decken.

## Was passiert bei …

- **Sensor-Verlust** (kein gültiger Wert): `invalid_ticks` zählt hoch; ab 10
 Ticks aus GAMING → failsafe CD_L2 (`sensor_failsafe_tick()`); gehaltene
 CD_L2/L3/L4 bleiben (bereits konservativ). Recovery automatisch, sobald
 eine Quelle zurückkommt.
- **Ein-Tick-Glitch** (ein Zonenwert springt plausibel-positiv): Spread-Klemme
 hält `tp` bei `t + 15` — L4 kann nicht aus einem einzelnen Ausreißer
 entstehen; der Ausreißer-Bereich kostet maximal `spike_drops++`.
- **Reale schnelle Anstiege**: alle Zonen steigen gemeinsam (< 15 K/Tick
 Spread, ~0,1-2 K/Tick Rate) = **Konsens**, das Gate ist offen, beide
 Klemmen schweigen, keine Latenz.
- **Persistenter Bias (Build463 geschlossen)**: ohne Konsens friert
 `temp_smooth` ein — die Rate-Klemme hält `t` dauerhaft bei `smooth + 15`
 und der Bias wird **nicht** mehr ab Tick 2 mitgetragen (S8d-Golden:
 „Bias eingefroren“). Verbleibende Grenzen: (a) ein Bias, der alle
 Zonen gleichzeitig *steigend* vortäuscht (nicht stillsteht), besteht
 den Konsens-Test — dann greift nur die +15-K/Tick-Ratenklemme;
 (b) eine hängend-hohe Zone sperrt den Konsens (gewollt, sie ist der
 Bias-Verdächtige), tote Zonen blockieren ihn nicht.
- **L4-Latenz (Build463 abgefangen)**: das eingefrorene `smooth` kann den
 gefilterten `tp` dauerhaft unter `cd_l4_temp` halten — der **Peak-Backstop**
 erzwingt L4 nach 8 Tick (2 s) mit Roh-Peak über der Schwelle
 (Ein-Tick-Ausreißer zählen nicht), Reset bei Abfall/Ausfall/Profil-off.
- **Battery heiß**: Guard-Latch blockiert den CD-Exit (Constraint in
 Schicht 3), bis der Akku unter die Entlatch-Temp fällt.

## Update-Pflicht

Bei jeder Änderung an Schichten 1-6 (Threshold-Semantik, Filter, Leiter,
Ramp, Notifier, Failsafe) dieses Doc im selben Commit nachziehen —
AGENTS-Pflicht (R4). Der Bias-Grenzen-Hinweis lebt zusätzlich als
Kommentar im Code (Helper + Call-Site), kann nicht driften.
