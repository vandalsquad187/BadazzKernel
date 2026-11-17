/* Host-Test fuer die k6a_gov-State-Machine (PC, kein Kernel). */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int32_t s32;
struct kobject {}; struct mutex { int x; }; struct task_struct {};
struct thermal_cooling_device {}; struct notifier_block {};
struct thermal_zone_device {}; struct delayed_work { int x; };
typedef struct { unsigned long bits; } cpumask_t;

static u64 fake_ms;
#define ktime_get()      (fake_ms)
#define ktime_to_ms(x)   (x)
#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define max_t(t, a, b)   (((t)(a) > (t)(b)) ? (t)(a) : (t)(b))
#define pr_info(...)             ((void)0)
#define pr_warn(...)             ((void)0)
#define pr_warn_ratelimited(...) ((void)0)

/* Build462: Stubs fuer die Sysfs-Store-Extraktion (S9).
 * kstrtoul reimplementiert die Kernel-Semantik (lib/kstrtox.c:
 * '+'-Skip, Basis 10, genau ein '\n' toleriert, sonst -EINVAL,
 * Overflow hier nicht getestet -> -ERANGE unberuehrt). */
struct kobj_attribute {};
static void mutex_lock(struct mutex *m) { (void)m; }
static void mutex_unlock(struct mutex *m) { (void)m; }
static int fake_wakes; static void gov_wake(void) { fake_wakes++; }
static int kstrtoul(const char *s, unsigned int base, unsigned long *v) {
    unsigned long long r = 0; int seen = 0;
    if (base != 10) return -EINVAL;
    if (s[0] == '+') s++;
    while (*s >= '0' && *s <= '9') { r = r * 10 + (*s - '0'); seen++; s++; }
    if (!seen) return -EINVAL;
    if (*s == '\n') s++;
    if (*s) return -EINVAL;
    *v = (unsigned long)r;
    return 0;
}

struct tr { u64 ms; unsigned from, to; int temp; };
static struct tr tr_log[4096]; static int tr_n;
static void trace_k6a_state(unsigned from, unsigned to, int temp) {
    if (tr_n < 4096) tr_log[tr_n++] = (struct tr){ fake_ms, from, to, temp };
}

struct power_supply;   /* Build441: nur Pointer im struct k6a_gov */

static int fake_batt_c = 30; static int fake_batt_err;
static int read_bat_temp(int *c) { if (fake_batt_err) return -1; *c = fake_batt_c; return 0; }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "extracted.h"
#pragma GCC diagnostic pop

static struct k6a_gov G;
static const char *SN[] = {"off","gaming","L2","L3","L4"};
static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void setup(int profile) {
    memset(&G, 0, sizeof(G)); gov = &G; tr_n = 0; fake_ms = 100000; fake_batt_c = 30; fake_batt_err = 0;
    const struct k6a_profile_def *d = &profiles[profile];
    G.profile = profile; G.enabled = true; G.legacy_mode = true;
    G.cd_l2_temp = d->cd_l2_temp; G.cd_l3_temp = d->cd_l3_temp; G.cd_l4_temp = d->cd_l4_temp; G.cd_recover = d->cd_recover;
    G.hysteresis_fast = 3000; G.hysteresis_normal = 2000; G.battery_guard_temp = 45; G.battery_guard = true;
    G.state_ts = fake_ms;
}
/* ein Kthread-Tick (250 ms): Battery-Guard, dann State Machine, wie in gov_thread() */
static void tick(u32 t_second, u32 t_peak) {
    fake_ms += 250; G.temp_celsius = t_second; G.temp_peak = t_peak;
    G.invalid_ticks = 0;   /* Build454: wie gov_thread im t_valid-Zweig */
    battery_guard_tick(); state_machine();
}
static int count_trans(unsigned a, unsigned b) { int n = 0; for (int i = 0; i < tr_n; i++) if (tr_log[i].from == a && tr_log[i].to == b) n++; return n; }
static void dump(void) { for (int i = 0; i < tr_n; i++) printf("    %6.1fs %s->%s @%d\n", (tr_log[i].ms - 100000) / 1000.0, SN[tr_log[i].from], SN[tr_log[i].to], tr_log[i].temp); }

static void s1_single_core_spike(void) {
    printf("S1 Einzelkern-Spitze: second=90, peak pendelt 100/103 (GAMING l4=102), 120 s\n");
    setup(K6A_PROFILE_GAMING);
    for (int i = 0; i < 480; i++) tick(90, (i % 2) ? 103 : 100);
    int to_l4 = count_trans(K6A_GAMING, K6A_CD_L4) + count_trans(K6A_CD_L3, K6A_CD_L4);
    int from_l4 = count_trans(K6A_CD_L4, K6A_CD_L3);
    printf("    Eintritte in L4: %d, Austritte L4->L3: %d (Margin=%d)\n", to_l4, from_l4, K6A_L4_MARGIN);
    CHECK(to_l4 <= 2, "L4 flappt: %d Eintritte", to_l4);
}
static void s2_heat_cool(void) {
    printf("S2 Aufheizen 70->105 C (90 s), Abkuehlen 105->60 C (120 s), GAMING\n");
    setup(K6A_PROFILE_GAMING);
    for (int i = 0; i < 360; i++) { u32 t = 70 + (35.0 * i / 359); tick(t - 2, t); }
    for (int i = 0; i < 480; i++) { u32 t = 105 - (45.0 * i / 479); tick(t - 2, t); }
    dump();
    CHECK(count_trans(K6A_GAMING, K6A_CD_L2) + count_trans(K6A_GAMING, K6A_CD_L3) >= 1, "nie gedrosselt");
    CHECK(count_trans(K6A_CD_L4, K6A_CD_L2) == 0 && count_trans(K6A_CD_L4, K6A_GAMING) == 0, "L4 sprang nicht stufenweise zurueck");
    CHECK(count_trans(K6A_CD_L3, K6A_GAMING) == 0, "L3 sprang direkt nach GAMING");
    CHECK(G.state == K6A_GAMING, "Endzustand %s statt gaming", SN[G.state]);
}
static void s3_profile_off(void) {
    printf("S3 Profil off mit hohen Temperaturen und gesetzten User-Schwellen\n");
    setup(K6A_PROFILE_GAMING); G.thresholds_user = true;
    for (int i = 0; i < 40; i++) tick(80, 90);
    CHECK(G.state == K6A_GAMING, "Startzustand %s", SN[G.state]);
    G.profile = K6A_PROFILE_OFF;
    for (int i = 0; i < 40; i++) tick(105, 110);
    CHECK(G.state == K6A_OFF, "profile=off aber state=%s", SN[G.state]);
    G.profile = K6A_PROFILE_GAMING;
    for (int i = 0; i < 8; i++) tick(80, 85);
    CHECK(G.state == K6A_GAMING, "kein Wiedereinstieg in gaming (%s)", SN[G.state]);
}
static void s4_battery_latch(void) {
    printf("S4 Battery-Guard: Akku 46 C latcht, L2 darf erst nach Entlatchen (<40 C) zurueck\n");
    setup(K6A_PROFILE_GAMING);
    for (int i = 0; i < 8; i++) tick(60, 62);
    fake_batt_c = 46; tick(60, 62);
    CHECK(G.batt_latched && G.state == K6A_CD_L2, "Latch/L2 fehlt (latched=%d state=%s)", G.batt_latched, SN[G.state]);
    for (int i = 0; i < 80; i++) tick(60, 62);
    CHECK(G.state == K6A_CD_L2, "L2 trotz Latch verlassen (%s)", SN[G.state]);
    fake_batt_c = 41; for (int i = 0; i < 40; i++) tick(60, 62);
    CHECK(G.batt_latched && G.state == K6A_CD_L2, "Latch zu frueh geloest (41 C ist nicht < 40)");
    fake_batt_c = 39; for (int i = 0; i < 40; i++) tick(60, 62);
    CHECK(!G.batt_latched && G.state == K6A_GAMING, "kein Rueckweg nach Entlatchen (%s)", SN[G.state]);
    fake_batt_c = 46; G.batt_latched = true; fake_batt_err = 1; tick(60, 62);
    CHECK(G.batt_latched, "Lesefehler hat Latch geloescht");
}
static void s5_entry_dwell(void) {
    printf("S5 GAMING->L2 braucht hysteresis_fast (3 s) anhaltend ueber l2 (96 C)\n");
    setup(K6A_PROFILE_GAMING);
    for (int i = 0; i < 4; i++) tick(80, 82);
    for (int i = 0; i < 10; i++) tick(97, 98);   /* 2,5 s */
    CHECK(G.state == K6A_GAMING, "L2 schon nach 2,5 s (%s)", SN[G.state]);
    for (int i = 0; i < 2; i++) tick(80, 82);
    for (int i = 0; i < 14; i++) tick(97, 98);   /* 3,5 s */
    CHECK(G.state == K6A_CD_L2, "kein L2 nach 3,5 s (%s)", SN[G.state]);
}
static void s6_ramp(void) {
    printf("S6 Gold-Cap-Rampe (Beispieltabelle)\n");
    setup(K6A_PROFILE_GAMING);
    u32 tab[] = {300000,576000,768000,1017600,1209600,1324800,1555200,1708800,1900800,2208000};
    for (int i = 0; i < 10; i++) { G.gold_freqs[i] = tab[i]; }
    G.gold_num = 10;
    u32 cur = 0; G.state = K6A_CD_L2; int steps = 0, prev = 2208000;
    for (int i = 0; i < 12; i++) { cur = ramp_next_cap(cur, 1209600); if (cur) { CHECK((int)cur <= prev, "Cap stieg beim Anziehen"); prev = cur; } if (cur == 1209600) break; steps++; }
    CHECK(cur == 1209600 && steps >= 3, "Ziel nicht stufenweise erreicht (cur=%u, steps=%d)", cur, steps);
    G.state = K6A_CD_L4; cur = ramp_next_cap(cur, 1017600);
    CHECK(cur == 1017600, "L4 nicht sofort (%u)", cur);
    G.state = K6A_GAMING; int ticks = 0; while (cur && ticks < 100) { cur = ramp_next_cap(cur, 0); ticks++; }
    CHECK(cur == 0 && ticks > 8, "Release zu schnell/haengt (cur=%u, ticks=%d)", cur, ticks);
}

static void s7_sensor_loss(void) {
    printf("S7 Sensor-Tod: 10 invalid Ticks erzwingen CD_L2 nur aus GAMING, Recovery automatisch\n");
    setup(K6A_PROFILE_GAMING);
    for (int i = 0; i < 4; i++) tick(45, 47);
    CHECK(G.state == K6A_GAMING, "Startzustand %s", SN[G.state]);
    for (int i = 0; i < 9; i++) sensor_failsafe_tick();
    CHECK(G.state == K6A_GAMING, "vorzeitiger fail-safe nach 9 Ticks (%s)", SN[G.state]);
    sensor_failsafe_tick();
    CHECK(G.state == K6A_CD_L2, "fail-safe hat L2 nicht erzwungen (%s)", SN[G.state]);
    CHECK(count_trans(K6A_GAMING, K6A_CD_L2) == 1, "fail-safe-Transition fehlt im hist");
    for (int i = 0; i < 5; i++) sensor_failsafe_tick();
    CHECK(G.state == K6A_CD_L2, "gehaltener CD_L2 wurde falsch weiterveraendert (%s)", SN[G.state]);
    tick(60, 62);
    CHECK(G.invalid_ticks == 0, "gueltiger Tick hat invalid_ticks nicht resettet");
    for (int i = 0; i < 40; i++) tick(60, 62);
    CHECK(G.state == K6A_GAMING, "kein Wiedereinstieg nach Recovery (%s)", SN[G.state]);
    printf("S7b: gehaltener CD_L3 bleibt CD_L3 (nur GAMING ist fail-open)\n");
    setup(K6A_PROFILE_GAMING);
    for (int i = 0; i < 4; i++) tick(45, 47);
    G.state = K6A_CD_L3;
    for (int i = 0; i < 10; i++) sensor_failsafe_tick();
    CHECK(G.state == K6A_CD_L3, "CD_L3 wurde durch fail-safe veraendert (%s)", SN[G.state]);
}

/* Build462: S8 — Spike-Filter-Goldens aus der eingefrorenen 461-Logik
 * (Tick-fuer-Tick-Expected-Values, deterministische Integer-Arithmetik
 * = Paritaetsbeweis). Helper ist rein (kein gov-Zugriff), direkte Calls. */
static void s8_spike_filter(void) {
    int t, tp, s, r;

    printf("S8a Reale Rampen 2 K/Tick: keine Klemme, smooth folgt\n");
    s = 0; t = 40; tp = 40;
    for (int i = 0; i < 30; i++) {
        r = k6a_spike_filter(&t, &tp, &s);
        CHECK(r == 0, "Tick %d: Klemme bei realem Anstieg (t=%d)", i, t);
        CHECK(s == t, "Tick %d: smooth != t (%d/%d)", i, s, t);
        CHECK(tp == t, "Tick %d: tp veraendert (%d/%d)", i, tp, t);
        t += 2; tp += 2;
    }

    printf("S8b Einzel-Sprung +40 K: t auf smooth+15, tp auf t+15\n");
    s = 40; t = 40; tp = 40; (void)k6a_spike_filter(&t, &tp, &s);
    t = 80; tp = 80;
    r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 1, "Sprung nicht als Spike erkannt (r=%d)", r);
    CHECK(t == 55 && tp == 70 && s == 55, "Klemmwerte falsch (t=%d tp=%d s=%d)", t, tp, s);

    printf("S8c Spread-Glitch: tp=110, t real 62 -> tp auf t+15\n");
    s = 60; t = 60; tp = 60; (void)k6a_spike_filter(&t, &tp, &s);
    t = 62; tp = 110;
    r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 1 && t == 62 && tp == 77 && s == 62,
          "Spread-Klemme falsch (t=%d tp=%d s=%d r=%d)", t, tp, s, r);

    printf("S8d Persistenter Bias +25 K: Tick1 +15 absorbiert, Tick2+ getrackt\n");
    s = 40; t = 40; tp = 40; (void)k6a_spike_filter(&t, &tp, &s);
    t = 65; tp = 65; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 1 && s == 55, "Tick1: Bias nicht absorbiert (s=%d r=%d)", s, r);
    t = 65; tp = 65; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 0 && s == 65, "Tick2: Bias nicht getrackt (s=%d r=%d)", s, r);
    t = 65; tp = 65; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 0 && s == 65, "Tick3: smooth driftet (s=%d)", s);

    printf("S8-smooth: smooth-Update auch ohne Klemme (Refactor-Schutz)\n");
    s = 0; t = 42; tp = 42; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 0 && s == 42, "erster Tick: smooth=%d r=%d", s, r);
    t = 44; tp = 44; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 0 && s == 44, "nur-on-clamp-Refactor erkannt (smooth=%d r=%d)", s, r);

    printf("S8-Grenzen: smooth+15 / tp=t+15 exakt passiert, +1 darueber geklemmt\n");
    s = 40; t = 55; tp = 55; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 0 && s == 55, "smooth+15 exakt geklemmt (t=%d r=%d)", t, r);
    s = 40; t = 56; tp = 56; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 1 && t == 55, "smooth+16 nicht geklemmt (t=%d r=%d)", t, r);
    s = 55; t = 60; tp = 75; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 0 && tp == 75, "tp=t+15 exakt geklemmt (tp=%d r=%d)", tp, r);
    s = 55; t = 60; tp = 76; r = k6a_spike_filter(&t, &tp, &s);
    CHECK(r == 1 && tp == 75, "tp=t+16 nicht geklemmt (tp=%d r=%d)", tp, r);
}

/* Build462: S9 — Sysfs-Store-Validierung gegen den echten Code. */
static void s9_store_validation(void) {
    ssize_t r;
    int w0, w1;
    char buf[64];
    struct kobject k; struct kobj_attribute a;

    printf("S9 cd_ladder-Store: 1/0 ok, 2/Garbage/-abc -EINVAL\n");
    fake_wakes = 0;
    r = cd_ladder_store(&k, &a, "1\n", 2);
    CHECK(r == 2 && gov->cd_ladder, "1 nicht akzeptiert (r=%zd)", r);
    CHECK(fake_wakes == 1, "kein gov_wake bei gueltigem Write");
    r = cd_ladder_store(&k, &a, "0\n", 2);
    CHECK(r == 2 && !gov->cd_ladder, "0 nicht akzeptiert (r=%zd)", r);
    w0 = fake_wakes;
    r = cd_ladder_store(&k, &a, "2\n", 2);
    CHECK(r == -EINVAL, "2 nicht abgelehnt (r=%zd)", r);
    r = cd_ladder_store(&k, &a, "abc\n", 4);
    CHECK(r == -EINVAL, "abc nicht abgelehnt (r=%zd)", r);
    r = cd_ladder_store(&k, &a, "-1\n", 3);
    CHECK(r == -EINVAL, "-1 nicht abgelehnt (r=%zd)", r);
    CHECK(fake_wakes == w0, "gov_wake bei abgelehntem Write");
    gov->cd_ladder = true;

    printf("S9 cd_silver_caps-Store: gueltig, partial-zero, aufsteigend, Bereich, 0 0 0\n");
    snprintf(buf, sizeof(buf), "1708800 1555200 1324800\n");
    fake_wakes = 0;
    r = cd_silver_caps_store(&k, &a, buf, strlen(buf));
    CHECK(r == (ssize_t)strlen(buf), "gueltige Caps abgelehnt (r=%zd)", r);
    CHECK(gov->cd_l2_silver_max == 1708800 && gov->cd_l3_silver_max == 1555200 &&
          gov->cd_l4_silver_max == 1324800, "Caps nicht angewandt (%u %u %u)",
          gov->cd_l2_silver_max, gov->cd_l3_silver_max, gov->cd_l4_silver_max);
    CHECK(fake_wakes == 1, "kein gov_wake bei gueltigen Caps");
    w1 = fake_wakes;
    snprintf(buf, sizeof(buf), "1708800 0 1324800\n");
    r = cd_silver_caps_store(&k, &a, buf, strlen(buf));
    CHECK(r == -EINVAL, "partial-zero nicht abgelehnt (r=%zd)", r);
    snprintf(buf, sizeof(buf), "1000 2000 3000\n");
    r = cd_silver_caps_store(&k, &a, buf, strlen(buf));
    CHECK(r == -EINVAL, "aufsteigend nicht abgelehnt (r=%zd)", r);
    snprintf(buf, sizeof(buf), "3000000000 2000000000 1000000000\n");
    r = cd_silver_caps_store(&k, &a, buf, strlen(buf));
    CHECK(r == -EINVAL, "out-of-range nicht abgelehnt (r=%zd)", r);
    snprintf(buf, sizeof(buf), "1 2\n");
    r = cd_silver_caps_store(&k, &a, buf, strlen(buf));
    CHECK(r == -EINVAL, "zu wenige Werte nicht abgelehnt (r=%zd)", r);
    CHECK(fake_wakes == w1, "gov_wake bei abgelehnten Caps");
    snprintf(buf, sizeof(buf), "0 0 0\n");
    r = cd_silver_caps_store(&k, &a, buf, strlen(buf));
    CHECK(r == (ssize_t)strlen(buf) && !gov->cd_l2_silver_max &&
          !gov->cd_l3_silver_max && !gov->cd_l4_silver_max,
          "0 0 0 (aus) nicht akzeptiert (r=%zd)", r);
}

int main(void) {
    s1_single_core_spike(); s2_heat_cool(); s3_profile_off(); s4_battery_latch(); s5_entry_dwell(); s6_ramp(); s7_sensor_loss();
    s8_spike_filter(); s9_store_validation();
    printf("\n%d Pruefungen, %d fehlgeschlagen\n", checks, fails);
    return fails ? 1 : 0;
}
