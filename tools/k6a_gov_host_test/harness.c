/* Host-Test fuer die k6a_gov-State-Machine (PC, kein Kernel). */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
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

struct tr { u64 ms; unsigned from, to; int temp; };
static struct tr tr_log[4096]; static int tr_n;
static void trace_k6a_state(unsigned from, unsigned to, int temp) {
    if (tr_n < 4096) tr_log[tr_n++] = (struct tr){ fake_ms, from, to, temp };
}

static int fake_batt_c = 30; static int fake_batt_err;
static int read_bat_temp(int *c) { if (fake_batt_err) return -1; *c = fake_batt_c; return 0; }

#include "extracted.h"

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

int main(void) {
    s1_single_core_spike(); s2_heat_cool(); s3_profile_off(); s4_battery_latch(); s5_entry_dwell(); s6_ramp();
    printf("\n%d Pruefungen, %d fehlgeschlagen\n", checks, fails);
    return fails ? 1 : 0;
}
