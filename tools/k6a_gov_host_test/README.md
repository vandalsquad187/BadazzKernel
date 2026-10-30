# k6a_gov Host-Test

Testet `state_machine()`, `set_state_locked()`, `ramp_next_cap()` und `battery_guard_tick()`
auf dem PC, ohne Kernel. Die Funktionen werden zur Laufzeit aus `k6a_gov.c` extrahiert,
der Test prueft also immer den echten Code.

    tools/k6a_gov_host_test/run.sh                 # gegen drivers/thermal/k6a_gov/k6a_gov.c
    tools/k6a_gov_host_test/run.sh /pfad/k6a_gov.c

Voraussetzung: gcc, python3. Szenarien: Einzelkern-Spitze (L4-Flapping), Aufheizen/Abkuehlen,
Profil off mit User-Schwellen, Battery-Latch, GAMING->L2-Dwell, Gold-Cap-Rampe.

Schlaegt die Extraktion fehl, wurde eine der Funktionen umbenannt: `extract.py` anpassen.
Neues Szenario: eine `sN_...()`-Funktion in `harness.c` schreiben und in `main()` aufrufen.
