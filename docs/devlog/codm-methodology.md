# codm methodology (Archiv)

> Aus `AGENTS.md` ausgelagert beim Docs-Umbau 2026-11-19. Inhalt unverändert.
> CODM-FPS-Messmethodik r24-r45 (Varianz, Gates, Mess-Werkzeuge)

## CODM-Fps-Messmethodik (r24–r40, gemessen 2026-10-11/12)

Ziel ist **„gesicherte 60 fps"** — entscheidend sind die Täler, nicht der Mittelwert.
Auswertung: `~/tmp/cmp6.py` (Varianz), `cmp7.py` (Regression), `cmp9.py` (Tal-Analyse).
Alle Läufe auf `build348`, gleiche Karte, Lüfter aus, Sampler `fps_samp2.sh`,
Filter `t0+120 s` (`SKIP=120`, **keine** fps-Schwelle).

### Varianz der laufgleichen Messung — 4× identisch

| Run | AkkuTemp | Ø fps | Täler <40 fps | tp-Start |
|---|---|---|---|---|
| r33 | 21,9 °C | **65,8** | 0 | 53 °C |
| r35 | 26,1 °C | **61,8** | 1 | 54 °C |
| r36 | 27,5 °C | **62,3** | 3 | 52 °C |
| r37 | 29,4 °C | **60,3** | 8 | 56 °C |

Ø 62,54 · **s = 2,36 fps** · Spannweite 5,6 fps · 95 %-Band 57,9…67,2

### Haupttreiber: Akkutemperatur (nicht Streuung)

```
fps = 81,22 − 0,71 · AkkuTemp_C        r = −0,96   r² = 0,926
```

**−0,71 fps pro Grad Akkutemperatur.** Nach Temperatur-Korrektur sinkt die
Streuung von **s = 2,36 auf s = 0,64 fps** (3,7×). Tälerzahl steigt
monoton mit der Starttemperatur (0→1→3→8).

**Messbarkeitsgrenze (80 % Power, α = 0,05, zwei Gruppen):**

| Aufbau | n=4 | n=6 |
|---|---|---|
| ohne Starttemperatur-Kontrolle | **4,7 fps** | 3,8 fps |
| mit Starttemperatur-Kontrolle | **1,3 fps** | 1,0 fps |

### DDR-Overclock: kein Effekt nachweisbar

`r34` (min_freq 6881 erzwungen, 264/264 Samples) = **60,1 fps** → −2,4 fps zum
Kontrollmittel = **1,0 σ → innerhalb der Varianz**. Fairster Paarvergleich bei
gleicher Temperatur: r34 (26,0 °C) 60,1 vs. r35 (26,1 °C) 61,8 → **−1,66 fps**,
unterhalb der Nachweisgrenze. DDR lag ohnehin bei 89–96 % am Deckel.

**Ableitung:** ohne Starttemperatur-Kontrolle ist die Methode **unfähig, DDR-OC
zu bewerten** — sie löst nur Effekte ≥ ~5 fps auf. Vor jeder weiteren
A/B-Aussage muss die Starttemperatur fixiert werden.

### Was die Täler sind — und was nicht

Tal-Fenster (10 s, <45 fps, n=19) gegen Hoch-Fenster (n=44):

| Größe | Tal | Hoch | Befund |
|---|---|---|---|
| `th` (Throttle-Ticks) | 0 | 0 | **kein CPU-Throttle** |
| `ql` (cpu_limits) | leer | leer | **Thermal-Framework greift nicht** |
| `p6` / `pm` | 1708800 | 1708800 | Kappe unverändert |
| `f6` | 1 632 000 (min) | 1 698 327 | Kappe fast durchgehend gehalten |
| `gb` (GPU busy) | 50 % | 71 % | **GPU unterfordert** |
| `gc` (GPU-Takt) | 360 MHz | 497 MHz | GPU-Governor rampet nicht hoch |
| `psi` (/proc/pressure/cpu) | 37,6 | 29,9 | **+26 % CPU-Druck** |

Die GPU bekommt keinen Feed — der Stall liegt **vor** der GPU, bei
CPU-Queue/Scheduler oder seitenständig im Spiel. Tiefster Einzelwert
r36 510 s = **7,2 fps** bei `gb=52 %`, `gpuclk=355 MHz`.

⚠️ **`qx` ist kumulativ** (`utime+stime` des `app.shanwan.gamepad`-Pinnern)
und damit reines Zeitartefakt — Täler liegen später im Lauf. **Nicht
auswerten.**

### Verbindliche Methode ab dem nächsten Lauf

1. **Ausschließlich Discharging.** Kabel ziehen, dann starten. r43 (Charging
   11 %) 58,6 fps gegen r44 (Discharging) 79,9 fps bei sonst identischen
   Settings — das ist der größte Konfound, kein Merkposten.
2. **Kein Starttemperatur-Gate.** Das Gate senkt die Streuung nicht (s = 2,34
   gemanagt vs 2,36 frei, r33/r39/r40) und kostet nur Wartezeit. Start-`bt`
   und `zone26` protokollieren, fertig.
3. **Auswertung über `validate.py`:** `med180` (Median Fenster rel. 180–570 s,
   v>0-Filter gegen Surface-Resets) + `p60` (%≥60) + Tälerzahl. Das
   180–570-Fenster schlitzt die frühen Drop-in-Täler (120–160 s) ab, die
   r41/r42/r43/r45 zeigen und r44 nie — Roh-Ø vergleicht Match-Fußabdruck,
   nicht Behandlung.
4. **Nur Rundenfenster 0–600 s** der io-Spur auswerten; danach verfälscht die
   eigene Analyselast (Termux läuft auf dem Gerät) die Thermik.
5. **≥ 3 Läufe pro Bedingung**, nie Einzelpaare — selbst die Robustmetrik
   streute r44↔r45 um 5,2 fps bei identischen Settings. Effekte unter ~5 fps
   brauchen mehr Läufe oder konsistente Block-/Tälermuster.

### Gated Läufe (r39, r40) — das Gate hilft NICHT

Mit `arm.sh` (Gate: AkkuTemp ≤ 23,5 °C **und** zone26 ≤ 52 °C, danach sofortiger
Spielstart) entstanden drei **saubere** Kontrollläufe gleicher Behandlung
(`nice-10+trips93-96-99-90+NOFAN+p6cap1708`, build348, gleiche Karte, Lüfter aus):

| Run | bt-Start | Ø fps | med | Täler | n |
|---|---|---|---|---|---|
| r33 | 21,9 °C | **65,84** | 62,0 | **0** | 207 |
| r39 | 23,6 °C | 62,56 | 58,3 | 2 | 177 |
| r40 | 23,0 °C | 61,31 | 59,0 | **6** | 179 |

Ø 63,24 · **s = 2,34 fps** · Spannweite 4,53

**s = 2,34 ist praktisch identisch mit dem unkontrollierten Wert (2,36).** Das
Temperatur-Gate senkt die Lauf-zu-Lauf-Streuung **nicht**; Akkutemperatur war nie
der Haupttreiber. Daraus folgt: **MDE ≈ 4,6 fps** (n=4/4 gepaart) — jede
Mikro-Optimierung (Pinner, `cdt`, Taktkurven, DDR) liegt unterhalb der
Nachweisgrenze und ist mit diesem Aufwand **nicht beweisbar**.

Das Rauschen ist zudem **nicht** eine gleichmäßige Absenkung, sondern
**sporadische Stalls**: r33/r39/r40 hatten 0/2/6 Täler, ohne Korrelation zur
Starttemperatur (23,0 °C → 6 Täler, 23,6 °C → 2).

### Stall-Ursache: spielseitig — CD_L2 war ein Nachrunden-Artefakt (r41–r45, 2026-10-17)

Die Live-Auslesung nach r40 (`state=cd_l2`, `batt_latched=1`, `hist`-Alter 728 s) war
**kein Runden-Zustand**, sondern zeigt den Nachrunden-Lade- und Befehlszustand: Termux
läuft **auf dem Gerät**, unsere eigene Auswertungs-Last hebt `zone26` nach der Runde auf
~94 °C. Gemessen in den Runden selbst (rel. 0–600 s) gilt — `k6a_gov` bleibt **in allen
fünf Läufen zu 100 % in `GAMING`**:

| Run | Bedingung | k6a-Zustand (Runde) | fps Ø | Täler |
|---|---|---|---|---|
| r41 | Kappe, kein Lüfter, Discharging | **gaming 133/133** | 64,2 | 13 |
| r42 | Kappe, Lüfter, Discharging | **gaming 152/152** | 60,6 | 8 |
| r43 | Volle Freq., Lüfter, **Charging 11 %** | **gaming 290/290** | 58,6 | 29 |
| r44 | Volle Freq., Lüfter, Discharging | **gaming 354/354** | **79,9** | **0** |
| r45 | Volle Freq., Lüfter, Discharging | **gaming 416/416** | 59,7 (63,0 bereinigt) | 35 |

Das einzige `cd_l2`-Ereignis der Serie (r45, rel. 638–889 s) liegt **komplett nach dem
600-s-Fenster**. **Regel: nur das Runden-Fenster 0–600 s auswerten.**

Drei Bedingungsvergleiche mit jeweils genau einer geänderten Variable:

| Vergleich | ΔØ fps | Befund |
|---|---|---|
| Charging → Discharging (r43→r44) | **+21,3** | Einzelpaar, eindeutige Richtung: **Laden während der Runde ist der größte Konfound.** |
| kein Lüfter → Lüfter (r41→r42) | −3,7 (−5 Täler) | **Lüfter-Hypothese tot.** SoC-Temp praktisch identisch (78,2/83 vs 79,5/85 °C) — der Lüfter kühlt nur den Akku (−6,7 °C), der nie engpass war. r32 (67,5/0) war ein Glückslauf. |
| Kappe 1708800 → 2304000 (r42→r44/r45) | r44 +19,4 · r45 −0,8 | **unbestätigt.** r44 ist der Ausreißer (0 Täler, 91 % ≥60); der Kontrolllauf r45 unter **identischen** Settings liefert das gekappte Niveau (59,7). **Entscheidung: Kappe bleibt 1708800.** |

Wo die Kappe real wirkt: **Temperatur und Schwellenabstand** —
ktemp Ø 74,9–79,5 °C gekappt vs 81,2–86,7 °C ungekappt (max 94 °C in r45), also
7–12 K Abstand zur 93 °C-Schwelle gespart, ohne messbaren Median-Gewinn
(med 180–570: 64,4/55,0 gekappt (r41/r42) vs 65,2/75,7/69,2 ungekappt
(r43/r44/r45) — innerhalb der Lauf-zu-Lauf-Streuung).

**Täler-Anatomie (bleibt: spielseitig).** Die GPU ist in den Stalls **nicht idled**,
sondern bei 50–75 % busy — sie bekommt zu wenig Arbeit. r45s 34 Täler entfallen ohne
jede Korrelation zu `cd_l2` (0/34 innerhalb ±3 s). Die frühen Täler bei 120–160 s
(Drop-in) tauchen in r41/r42/r43/r45 auf, in r44 nie — Match-Fußabdruck statt
Behandlungseffekt. k6a braucht für CD_L2 t≥93 °C **durchgehend 3000 ms** (`entry_since`
wird bei jedem Unterschreiten zurückgesetzt) — bei 24 erneuten ≥93-Beobachtungen in
r45 reichte das nie: beabsichtigte Anti-Thrash-Hysterese, kein Bug.

### Mess-Werkzeuge und ihre Fallen (r40–r45)

`io_samp.sh` zeichnet PSI, Block-I/O-Deltas, `pgmajfault`, GPU-Busy/-Takt,
DDR-Bandbreite **und den k6a_gov-Status pro Iteration** mit
(`kstate ktemp kpeak kpm kth kbl ksage`, 18 Spalten). Für r41–r45 lief damit die
Zuordnung Stall ↔ Zustand direkt.

⚠️ Drei Fallen, erst deutlich an r44/r45 gemessen:

1. **Kadenz ist NICHT 1 s.** Der Status-Read (`timeout 2` + restliche Pipeline)
   macht pro Iteration real **1,4–2,1 s** — die `t`-Spalte ist Timestamp, keine
   Linien-Nummer. Deltas per Nearest-Match zuordnen, nicht per Zeilenindex.
2. **Frame-Zähler kann zurückgesetzt werden.** r43 meldet `min 0,0 fps`, r45
   `min −530,8 fps` — in beiden Fällen wurde die SurfaceView des Spiels neu
   erstellt (Spiel-/Surface-Recreation). **Alle `v∉(0,∞)`-Intervalle vor Auswerten
   verwerfen** (macht `validate.py` seit dem selbst).
3. **Nachrunden-Samples sind vergiftet.** Die letzte ~1/3 der io-Spur kann
   Nachrunden-Werte enthalten (Analyselast). **Nur rel. 0–600 s bewerten.**

