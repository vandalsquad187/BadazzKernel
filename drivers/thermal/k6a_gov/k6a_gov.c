#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/notifier.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/fs.h>

extern int kgsl_k6a_get_levels(u32 *out, u32 max_n, u32 *cur_idx, u32 *max_idx);
extern int kgsl_k6a_set_max_level_idx(unsigned int level);
extern int k6a_devfreq_get_bw(const char *name, u32 *cur, u32 *min, u32 *max);
extern int k6a_devfreq_set_bw(const char *name, u32 min, u32 max);

#define K6A_GOV_VERSION       "1.3.1"
#define K6A_GOV_KERNEL_VER    KERNEL_VERSION(4,14,369)
#define K6A_GOV_KTHREAD_SLEEP_MS   250
#define K6A_GOV_MAX_FREQS     32
#define K6A_HIST_N            16

enum k6a_state { K6A_OFF=0, K6A_GAMING=1, K6A_CD_L2=2, K6A_CD_L3=3, K6A_CD_L4=4 };
enum k6a_profile { K6A_PROFILE_OFF=0, K6A_PROFILE_GAMING=1, K6A_PROFILE_BATTERY=2, K6A_PROFILE_BADAZZ=3, K6A_PROFILE_CUSTOM=4, K6A_PROFILE_BADAZZ_SAFE=5 };
#define K6A_PROFILE_MAX K6A_PROFILE_BADAZZ_SAFE

struct k6a_hist_entry {
    u64 ts_ms;
    s32 temp;
    u8 from, to;
};

struct k6a_gov {
    struct kobject *kobj;
    struct mutex lock;
    enum k6a_state state;
    u32 temp_celsius;
    u32 profile;
    pid_t game_pid;
    u64 ticks;
    struct task_struct *kthread;
    bool enabled;
    bool legacy_mode;
    bool battery_guard;
    u32 poll_ms;

    u32 cd_l2_temp, cd_l3_temp, cd_l4_temp, cd_recover;
    u32 cd_l2_gold_max, cd_l3_gold_max, cd_l4_gold_max;
    u32 cd_l2_gpu_max, cd_l3_gpu_max, cd_l4_gpu_max;
    u32 cd_l2_bw_gpubw, cd_l3_bw_gpubw, cd_l4_bw_gpubw;
    u32 cd_l2_bw_llcc, cd_l3_bw_llcc, cd_l4_bw_llcc;
    s32 gpu_last_idx;
    u32 hysteresis_fast, hysteresis_normal;

    u64 state_ts;
    s32 prev_temp;
    u64 throttle_events;

    struct k6a_hist_entry hist[K6A_HIST_N];
    u8 hist_pos, hist_len;

    struct thermal_cooling_device *cooling_dev;
    struct notifier_block cpufreq_nb;

    u32 gold_freqs[K6A_GOV_MAX_FREQS];
    u32 gold_num;

    u32 build_hash;

    struct delayed_work freq_init_work;
    int freq_init_retries;
};

struct k6a_gov *gov;

static int legacy_mode = 1;
module_param(legacy_mode, int, 0644);
MODULE_PARM_DESC(legacy_mode, "1 = Kernel Cooldown (CPU-only) (default=1)");

static int profile = K6A_PROFILE_GAMING;
module_param(profile, int, 0644);
MODULE_PARM_DESC(profile, "Default profile: 0=off, 1=gaming, 2=battery, 3=badazz, 4=custom, 5=badazz_safe");

/* ── Profile Defaults (Hz values, auto-mapped to table) ──────────── */
struct k6a_profile_def {
    u32 cd_l2_temp, cd_l3_temp, cd_l4_temp, cd_recover;
    u32 cd_l2_gold_max, cd_l3_gold_max, cd_l4_gold_max;
    u32 cd_l2_gpu_max, cd_l3_gpu_max, cd_l4_gpu_max;   /* 0 = disabled */
    u32 cd_l2_bw_gpubw, cd_l3_bw_gpubw, cd_l4_bw_gpubw;
    u32 cd_l2_bw_llcc, cd_l3_bw_llcc, cd_l4_bw_llcc;
};

static const struct k6a_profile_def profiles[] = {
    [K6A_PROFILE_OFF]        = { 0,0,0,0,   0,0,0,   0,0,0,   0,0,0,   0,0,0 },
    [K6A_PROFILE_GAMING]     = { 83,86,90,78,   1708800,1209600,1094400,
                                 800000000,565000000,430000000,
                                 4000,3000,1500,   0,4000,3000 },
    [K6A_PROFILE_BATTERY]    = { 70,75,80,65,   1400000,1200000,1000000,
                                 585000000,430000000,305000000,
                                 1500,1000,500,   0,3000,2000 },
    [K6A_PROFILE_BADAZZ]     = { 85,90,95,80,   1800000,1600000,1400000,
                                 650000000,430000000,355000000,
                                 2500,2000,1500,   0,5000,4000 },
    [K6A_PROFILE_CUSTOM]     = { 0,0,0,0,       0,0,0,   0,0,0,   0,0,0,   0,0,0 },
    [K6A_PROFILE_BADAZZ_SAFE]= { 82,87,92,78,   1555200,1200000,1000000,
                                 585000000,430000000,305000000,
                                 2000,1500,1000,   0,4000,3000 },
};

/* ── Freq Helpers ────────────────────────────────────────────────── */
static void enforce_max_freq(void);

static u32 clamp_freq(u32 *avail, u32 num, u32 requested) {
    u32 best = 0, i;
    if (!num || !avail) return requested;
    for (i = 0; i < num; i++) {
        if (avail[i] <= requested && avail[i] > best)
            best = avail[i];
        if (avail[i] == requested)
            return requested;
    }
    return best ? best : avail[0];
}

static void clamp_cd_freqs(void) {
    if (!gov->gold_num) return;
    gov->cd_l2_gold_max = clamp_freq(gov->gold_freqs, gov->gold_num, gov->cd_l2_gold_max);
    gov->cd_l3_gold_max = clamp_freq(gov->gold_freqs, gov->gold_num, gov->cd_l3_gold_max);
    gov->cd_l4_gold_max = clamp_freq(gov->gold_freqs, gov->gold_num, gov->cd_l4_gold_max);
}

/* Find a CPU belonging to the "gold" (big) cluster.
 * Heuristic: highest max frequency among online CPUs. */
static int find_gold_cpu(void) {
    struct cpufreq_policy *policy;
    unsigned int max_freq = 0;
    int best_cpu = -1;
    int cpu;

    for_each_online_cpu(cpu) {
        policy = cpufreq_cpu_get(cpu);
        if (policy) {
            if (policy->cpuinfo.max_freq > max_freq) {
                max_freq = policy->cpuinfo.max_freq;
                best_cpu = cpu;
            }
            cpufreq_cpu_put(policy);
        }
    }
    return best_cpu >= 0 ? best_cpu : 6;  /* fallback to CPU 6 */
}

static void freq_init_worker(struct work_struct *work) {
    struct k6a_gov *g = container_of(work, struct k6a_gov, freq_init_work.work);
    struct cpufreq_policy *policy;
    struct cpufreq_frequency_table *pos;
    int n, gold_cpu;

    gold_cpu = find_gold_cpu();
    policy = cpufreq_cpu_get(gold_cpu);
    if (policy && policy->freq_table) {
        n = 0;
        cpufreq_for_each_valid_entry(pos, policy->freq_table) {
            if (pos->frequency != CPUFREQ_ENTRY_INVALID && n < K6A_GOV_MAX_FREQS)
                g->gold_freqs[n++] = pos->frequency;
        }
        mutex_lock(&g->lock);
        g->gold_num = n;
        mutex_unlock(&g->lock);
        pr_info("k6a_gov: Gold freqs loaded via cpufreq API (cpu %d): %u entries\n",
                gold_cpu, n);
    }
    if (policy) cpufreq_cpu_put(policy);

    {
        u32 gtab[K6A_GOV_MAX_FREQS]; u32 gn = 0, tl = 0;
        int tl_ret = kgsl_k6a_get_levels(gtab, K6A_GOV_MAX_FREQS, &gn, &tl);
        if (tl_ret > 0)
            pr_info("k6a_gov: gpu levels=%u cur_idx=%u max_idx=%u\n",
                    tl_ret, gn, tl);
        else
            pr_info("k6a_gov: gpu levels read failed (%d)\n", tl_ret);
    }

    mutex_lock(&g->lock);
    if (g->gold_num > 0) {
        clamp_cd_freqs();
        pr_info("k6a_gov: freq tables loaded: Gold=%u\n", g->gold_num);
        if (g->state != K6A_OFF && g->enabled && g->legacy_mode)
            enforce_max_freq();
        mutex_unlock(&g->lock);
        return;
    }
    mutex_unlock(&g->lock);

    if (++g->freq_init_retries < 30) {
        schedule_delayed_work(&g->freq_init_work, msecs_to_jiffies(500));
    } else {
        pr_warn("k6a_gov: freq init failed after 30 retries, using defaults\n");
    }
}

/* ── CPU Freq Enforcement ────────────────────────────────────────── */
/* Caller must hold gov->lock. */
static u32 get_cd_max_freq(void) {
    if (!gov || !gov->gold_num) return 0;
    switch (gov->state) {
    case K6A_CD_L2: return gov->cd_l2_gold_max;
    case K6A_CD_L3: return gov->cd_l3_gold_max;
    case K6A_CD_L4: return gov->cd_l4_gold_max;
    default: return 0;
    }
}

static void enforce_max_freq(void) {
    struct cpufreq_policy *p;
    u32 max;
    int gold_cpu;

    if (!gov || !gov->enabled || !gov->legacy_mode) return;
    max = get_cd_max_freq();
    if (!max) return;

    gold_cpu = find_gold_cpu();
    p = cpufreq_cpu_get(gold_cpu);
    if (p) {
        if (p->max > max) {
            p->max = max;
            /* cpufreq_update_policy() must NOT be called from notifier/atomic context.
             * We're in kthread (process context) so it's legal, but the notifier
             * cpufreq_notify() runs in atomic context - it only clamps p->max. */
        }
        cpufreq_cpu_put(p);
    }
}

static int cpufreq_notify(struct notifier_block *nb, unsigned long e, void *data) {
    struct cpufreq_policy *p;
    u32 max;
    int gold_cpu;

    if (!gov || !gov->enabled || !gov->legacy_mode || gov->state < K6A_CD_L2)
        return NOTIFY_OK;
    if (e != CPUFREQ_ADJUST && e != CPUFREQ_INCOMPATIBLE)
        return NOTIFY_OK;
    p = data;
    max = get_cd_max_freq();
    if (!max) return NOTIFY_OK;

    gold_cpu = find_gold_cpu();
    if (p->cpu == gold_cpu && p->max > max) {
        p->max = max;
        /* NO cpufreq_update_policy() here — causes recursion! */
    }
    return NOTIFY_OK;
}

/* ── Temp Read ───────────────────────────────────────────────────── */
/* KB8: max over all 4 Gold zones (cpu-1-0..3-usr) */
static int read_temp(void) {
    struct thermal_zone_device *tz; int t = 0, max_t = 0;
    int i;

    for (i = 0; i < 4; i++) {
        char name[16];
        snprintf(name, sizeof(name), "cpu-1-%d-usr", i);
        tz = thermal_zone_get_zone_by_name(name);
        if (!IS_ERR(tz) && !thermal_zone_get_temp(tz, &t) && t > 0) {
            t /= 1000;
            if (t > max_t) max_t = t;
        }
    }

    if (max_t) return max_t;

    /* Fallbacks for non-Gold zones */
    tz = thermal_zone_get_zone_by_name("cpu-0-0-usr");
    if (!IS_ERR(tz) && !thermal_zone_get_temp(tz, &t) && t > 0) return t / 1000;
    tz = thermal_zone_get_zone_by_name("xo-therm");
    if (!IS_ERR(tz) && !thermal_zone_get_temp(tz, &t) && t > 0) return t / 1000;
    tz = thermal_zone_get_zone_by_name("soc-therm");
    if (!IS_ERR(tz) && !thermal_zone_get_temp(tz, &t) && t > 0) return t / 1000;
    tz = thermal_zone_get_zone_by_name("thermal_zone0");
    if (!IS_ERR(tz) && !thermal_zone_get_temp(tz, &t) && t > 0) return t / 1000;
    return 40;
}

/* ── Throttle History (KB7) ──────────────────────────────────────── */
/* caller must hold gov->lock */
static void k6a_hist_push(enum k6a_state from) {
    struct k6a_hist_entry *e = &gov->hist[gov->hist_pos];
    e->ts_ms = ktime_to_ms(ktime_get());
    e->temp = (s32)gov->temp_celsius;
    e->from = (u8)from;
    e->to = (u8)gov->state;
    gov->hist_pos = (gov->hist_pos + 1) % K6A_HIST_N;
    if (gov->hist_len < K6A_HIST_N) gov->hist_len++;
}

/* single entry point for every state transition */
static void set_state_locked(enum k6a_state ns) {
    enum k6a_state from = gov->state;
    if (ns == from) return;
    gov->state = ns;
    gov->state_ts = ktime_to_ms(ktime_get());
    k6a_hist_push(from);
}

/* ── State Machine ───────────────────────────────────────────────── */
static void state_machine(void) {
    u64 now = ktime_to_ms(ktime_get());
    s32 delta = gov->temp_celsius - gov->prev_temp;
    u32 dwell = (delta >= 5) ? gov->hysteresis_fast : gov->hysteresis_normal;

    if (gov->state == K6A_OFF && gov->enabled &&
        gov->profile != K6A_PROFILE_OFF) {
        set_state_locked(K6A_GAMING);
        pr_info("k6a_gov: OFF -> GAMING\n");
    }

    switch (gov->state) {
    case K6A_CD_L4:
        if (gov->temp_celsius < gov->cd_l3_temp && (now - gov->state_ts) >= dwell)
            set_state_locked(K6A_CD_L3);
        break;
    case K6A_CD_L3:
        if (gov->temp_celsius < gov->cd_l2_temp && (now - gov->state_ts) >= dwell)
            set_state_locked(K6A_CD_L2);
        break;
    case K6A_CD_L2:
        if (gov->temp_celsius <= gov->cd_recover && (now - gov->state_ts) >= dwell)
            set_state_locked(K6A_GAMING);
        break;
    case K6A_GAMING:
        if (gov->temp_celsius >= gov->cd_l4_temp) {
            set_state_locked(K6A_CD_L4); gov->throttle_events++;
        } else if (gov->temp_celsius >= gov->cd_l3_temp) {
            set_state_locked(K6A_CD_L3); gov->throttle_events++;
        } else if (gov->temp_celsius >= gov->cd_l2_temp) {
            set_state_locked(K6A_CD_L2); gov->throttle_events++;
        }
        break;
    default:
        break;
    }
    gov->prev_temp = gov->temp_celsius;
}

/* ── Apply Limits ────────────────────────────────────────────────── */
/* GPU cap via native KGSL interface (KB10R-2). Kthread = process
 * context, same locking as kgsl's own sysfs store path. */
static void gpu_reset(void) {
    if (gov->gpu_last_idx >= 0) {
        if (!kgsl_k6a_set_max_level_idx(0))
            pr_info("k6a_gov: gpu cap released\n");
        gov->gpu_last_idx = -1;
    }
}

static u32 get_cd_gpu_cap(void) {
    switch (gov->state) {
    case K6A_CD_L2: return gov->cd_l2_gpu_max;
    case K6A_CD_L3: return gov->cd_l3_gpu_max;
    case K6A_CD_L4: return gov->cd_l4_gpu_max;
    default: return 0;
    }
}

static void enforce_gpu_cap(void) {
    u32 tab[K6A_GOV_MAX_FREQS], cur_idx = 0, max_idx = 0, cap;
    int n, i, best = -1;

    if (!gov || !gov->enabled || !gov->legacy_mode) { gpu_reset(); return; }
    cap = get_cd_gpu_cap();
    if (!cap) { gpu_reset(); return; }

    n = kgsl_k6a_get_levels(tab, K6A_GOV_MAX_FREQS, &cur_idx, &max_idx);
    if (n <= 0) return;

    /* highest available level freq that still fits under the cap */
    for (i = 0; i < n; i++)
        if (tab[i] <= cap && (best < 0 || tab[i] > tab[best]))
            best = i;
    if (best < 0) best = n - 1;   /* nothing fits: lowest level */

    if (best != gov->gpu_last_idx) {
        if (!kgsl_k6a_set_max_level_idx((unsigned int)best)) {
            gov->gpu_last_idx = best;
            pr_info("k6a_gov: gpu cap %uHz -> lvl %d (of %u)\n", cap, best, n);
        }
    }
}

static void get_cd_bw_floors(u32 *gpubw, u32 *llcc) {
    switch (gov->state) {
    case K6A_CD_L2:
        *gpubw = gov->cd_l2_bw_gpubw;
        *llcc = gov->cd_l2_bw_llcc;
        break;
    case K6A_CD_L3:
        *gpubw = gov->cd_l3_bw_gpubw;
        *llcc = gov->cd_l3_bw_llcc;
        break;
    case K6A_CD_L4:
        *gpubw = gov->cd_l4_bw_gpubw;
        *llcc = gov->cd_l4_bw_llcc;
        break;
    default:
        *gpubw = 0;
        *llcc = 0;
        break;
    }
}

static void enforce_bw_floors(void) {
    u32 gpubw, llcc;

    if (!gov || !gov->enabled || !gov->legacy_mode) return;
    get_cd_bw_floors(&gpubw, &llcc);
    if (gpubw)
        k6a_devfreq_set_bw("gpubw", gpubw, 0);
    if (llcc)
        k6a_devfreq_set_bw("cpu-llcc-ddr-bw", llcc, 0);
}

static void reset_bw_floors(void) {
    k6a_devfreq_set_bw("gpubw", 0, 0);
    k6a_devfreq_set_bw("cpu-llcc-ddr-bw", 0, 0);
}

static void apply_limits(void) {
    if (!gov || !gov->legacy_mode) { gpu_reset(); reset_bw_floors(); return; }
    enforce_max_freq();
    enforce_gpu_cap();
    enforce_bw_floors();
}

/* ── Battery Guard ─────────────────────────────────────────────── */
static int read_bat_temp(void) {
    struct power_supply *psy;
    union power_supply_propval val;
    int ret;
    psy = power_supply_get_by_name("battery");
    if (!psy) return 0;
    ret = psy->desc->get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
    power_supply_put(psy);
    if (ret) return 0;
    return val.intval / 10;  /* Zehntelgrad → Celsius */
}

/* ── Kthread ─────────────────────────────────────────────────────── */
static bool hash_verified = false;

static int verify_build_hash(void) {
    /* KB15: Verify build hash against k6a_features/git_hash.
     * The hash is exposed by k6a_features module in sysfs.
     * We read it once and compare with our compile-time hash.
     * On mismatch, disable legacy_mode (enforcement off). */
    struct file *filp;
    char buf[64];
    loff_t pos = 0;
    int ret;

    filp = filp_open("/sys/kernel/k6a_features/git_hash", O_RDONLY, 0);
    if (IS_ERR(filp)) {
        pr_warn("k6a_gov: k6a_features/git_hash not found, skipping hash verify\n");
        return 0;
    }

    ret = kernel_read(filp, buf, sizeof(buf) - 1, &pos);
    filp_close(filp, NULL);
    if (ret <= 0) {
        pr_warn("k6a_gov: failed to read git_hash\n");
        return 0;
    }
    buf[ret] = '\0';

    /* Our compile-time hash (set by build system via Makefile) */
#define K6A_BUILD_HASH "full-synergy"
    if (strncmp(buf, K6A_BUILD_HASH, strlen(K6A_BUILD_HASH)) != 0) {
        pr_warn("k6a_gov: hash mismatch! build=%s runtime=%s -> legacy_mode=0\n",
                K6A_BUILD_HASH, buf);
        return -EINVAL;
    }
    pr_info("k6a_gov: hash verified OK (%s)\n", K6A_BUILD_HASH);
    return 0;
}

static int gov_thread(void *data) {
    while (!kthread_should_stop()) {
        mutex_lock(&gov->lock);
        if (gov->enabled) {
            if (!hash_verified) {
                if (verify_build_hash() != 0)
                    gov->legacy_mode = 0;
                hash_verified = true;
            }
            gov->temp_celsius = read_temp();
            if (gov->battery_guard) {
                int bt = read_bat_temp();
                if (bt >= 45 && gov->state == K6A_GAMING) {
                    set_state_locked(K6A_CD_L2);
                    pr_warn("k6a_gov: battery guard %dC -> CD_L2\n", bt);
                }
            }
            state_machine();
            apply_limits();
        }
        gov->ticks++;
        mutex_unlock(&gov->lock);
        msleep(gov->poll_ms);
    }
    return 0;
}

/* ── Cooling Device ──────────────────────────────────────────────── */
static int cool_max(struct thermal_cooling_device *c, unsigned long *s) { *s = K6A_CD_L4; return 0; }
static int cool_cur(struct thermal_cooling_device *c, unsigned long *s) {
    if (!gov) return -EINVAL;
    mutex_lock(&gov->lock);
    *s = gov->state;
    mutex_unlock(&gov->lock);
    return 0;
}
static int cool_set(struct thermal_cooling_device *c, unsigned long s) {
    if (!gov) return -EINVAL;
    if (s > K6A_CD_L4) s = K6A_CD_L4;   /* thermal-core kann mehr fordern */
    mutex_lock(&gov->lock);
    set_state_locked((enum k6a_state)s);
    mutex_unlock(&gov->lock);
    return 0;
}
static struct thermal_cooling_device_ops cool_ops = {
    .get_max_state = cool_max,
    .get_cur_state = cool_cur,
    .set_cur_state = cool_set,
};

/* ── Sysfs ───────────────────────────────────────────────────────── */
static ssize_t enable_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    int v;
    mutex_lock(&gov->lock);
    v = gov->enabled;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%d\n", v);
}
static ssize_t enable_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->enabled = !!v;
    if (!v) {
        gov->state = K6A_OFF;
        gpu_reset();   /* Not-Aus: GPU-Cap sofort frei */
        reset_bw_floors();
    }
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t profile_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    const char *n[] = {"off","gaming","battery","badazz","custom","badazz_safe"};
    int p;
    mutex_lock(&gov->lock);
    p = gov->profile;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%s\n", n[p]);
}
static ssize_t profile_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    if (v > K6A_PROFILE_MAX) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->profile = v;
    gov->cd_l2_temp=profiles[v].cd_l2_temp;
    gov->cd_l3_temp=profiles[v].cd_l3_temp;
    gov->cd_l4_temp=profiles[v].cd_l4_temp;
    gov->cd_recover=profiles[v].cd_recover;
    gov->cd_l2_gold_max=profiles[v].cd_l2_gold_max;
    gov->cd_l3_gold_max=profiles[v].cd_l3_gold_max;
    gov->cd_l4_gold_max=profiles[v].cd_l4_gold_max;
    gov->cd_l2_gpu_max=profiles[v].cd_l2_gpu_max;
    gov->cd_l3_gpu_max=profiles[v].cd_l3_gpu_max;
    gov->cd_l4_gpu_max=profiles[v].cd_l4_gpu_max;
    gov->cd_l2_bw_gpubw=profiles[v].cd_l2_bw_gpubw;
    gov->cd_l3_bw_gpubw=profiles[v].cd_l3_bw_gpubw;
    gov->cd_l4_bw_gpubw=profiles[v].cd_l4_bw_gpubw;
    gov->cd_l2_bw_llcc=profiles[v].cd_l2_bw_llcc;
    gov->cd_l3_bw_llcc=profiles[v].cd_l3_bw_llcc;
    gov->cd_l4_bw_llcc=profiles[v].cd_l4_bw_llcc;
    clamp_cd_freqs();
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t status_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    static const char *state_names[] = {"off","gaming","cd_l2","cd_l3","cd_l4"};
    unsigned int si;
    u32 temp, profile, legacy, enabled, gold_num, gold_max, gold_max_tbl, ticks;
    u64 throttle_events;
    u32 gpu_caps[3], gpu_last_idx;
    u32 bw_floors_gpubw[3], bw_floors_llcc[3];
    u32 bwc, bwmn, bwmx;
    size_t len;
    int i, hist_len, hist_pos;
    struct k6a_hist_entry hist[K6A_HIST_N];

    mutex_lock(&gov->lock);
    si = gov->state;
    temp = gov->temp_celsius;
    profile = gov->profile;
    legacy = gov->legacy_mode;
    enabled = gov->enabled;
    gold_num = gov->gold_num;
    gold_max = get_cd_max_freq();
    gold_max_tbl = gold_num ? gov->gold_freqs[gold_num - 1] : 0;
    ticks = gov->ticks;
    throttle_events = gov->throttle_events;
    gpu_caps[0] = gov->cd_l2_gpu_max;
    gpu_caps[1] = gov->cd_l3_gpu_max;
    gpu_caps[2] = gov->cd_l4_gpu_max;
    gpu_last_idx = gov->gpu_last_idx;
    bw_floors_gpubw[0] = gov->cd_l2_bw_gpubw;
    bw_floors_gpubw[1] = gov->cd_l3_bw_gpubw;
    bw_floors_gpubw[2] = gov->cd_l4_bw_gpubw;
    bw_floors_llcc[0] = gov->cd_l2_bw_llcc;
    bw_floors_llcc[1] = gov->cd_l3_bw_llcc;
    bw_floors_llcc[2] = gov->cd_l4_bw_llcc;
    hist_len = gov->hist_len;
    hist_pos = gov->hist_pos;
    memcpy(hist, gov->hist, sizeof(hist));
    mutex_unlock(&gov->lock);

    if (si > K6A_CD_L4) si = K6A_OFF;
    len = sprintf(b,
        "version=%s\n"
        "state=%s\n"
        "temp=%d\n"
        "ticks=%u\n"
        "throttle_events=%llu\n"
        "profile=%u\n"
        "legacy_mode=%d\n"
        "enabled=%d\n"
        "gold_freqs=%u\n"
        "gold_max=%u\n"
        "gold_max_tbl=%u\n",
        K6A_GOV_VERSION,
        state_names[si],
        temp,
        ticks,
        throttle_events,
        profile,
        legacy,
        enabled,
        gold_num,
        gold_max,
        gold_max_tbl);

    len += scnprintf(b + len, PAGE_SIZE - len,
        "gpu_caps=%u %u %u\n"
        "gpu_last_idx=%d\n",
        gpu_caps[0], gpu_caps[1], gpu_caps[2],
        gpu_last_idx);

    /* KB12M Phase 1: read-only bandwidth monitoring */
    if (!k6a_devfreq_get_bw("gpubw", &bwc, &bwmn, &bwmx))
        len += scnprintf(b + len, PAGE_SIZE - len,
            "bw_gpubw=%u %u %u\n", bwc, bwmn, bwmx);
    if (!k6a_devfreq_get_bw("cpu-llcc-ddr-bw", &bwc, &bwmn, &bwmx))
        len += scnprintf(b + len, PAGE_SIZE - len,
            "bw_llcc=%u %u %u\n", bwc, bwmn, bwmx);

    /* KB12 Phase 2: BW floors (write) */
    len += scnprintf(b + len, PAGE_SIZE - len,
        "bw_floor_gpubw=%u %u %u\n"
        "bw_floor_llcc=%u %u %u\n",
        bw_floors_gpubw[0], bw_floors_gpubw[1], bw_floors_gpubw[2],
        bw_floors_llcc[0], bw_floors_llcc[1], bw_floors_llcc[2]);

    /* KB15: hash verification */
    len += scnprintf(b + len, PAGE_SIZE - len,
        "hash_verified=%d\n", hash_verified);

    /* KB7: throttle history, oldest first */
    len += scnprintf(b + len, PAGE_SIZE - len, "hist=");
    for (i = 0; i < hist_len && len < PAGE_SIZE - 48; i++) {
        struct k6a_hist_entry *e;
        unsigned int hi = (hist_pos + K6A_HIST_N - hist_len + i)
                          % K6A_HIST_N;
        e = &hist[hi];
        if (e->from > K6A_CD_L4 || e->to > K6A_CD_L4) continue;
        len += scnprintf(b + len, PAGE_SIZE - len,
            "%s%llu:%d:%s>%s", i ? "," : "",
            e->ts_ms, e->temp,
            state_names[e->from], state_names[e->to]);
    }
    len += scnprintf(b + len, PAGE_SIZE - len, "\n");
    return len;
}

static ssize_t game_pid_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    pid_t pid;
    mutex_lock(&gov->lock);
    pid = gov->game_pid;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%d\n", pid);
}
static ssize_t game_pid_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->game_pid = v;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t legacy_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    bool v;
    mutex_lock(&gov->lock);
    v = gov->legacy_mode;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%d\n", v);
}
static ssize_t legacy_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->legacy_mode = !!v;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t hysteresis_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    u32 fast, normal;
    mutex_lock(&gov->lock);
    fast = gov->hysteresis_fast;
    normal = gov->hysteresis_normal;
    mutex_unlock(&gov->lock);
    return sprintf(b, "fast=%u normal=%u\n", fast, normal);
}
static ssize_t hysteresis_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    u32 fast, normal;
    if (sscanf(b, "%u %u", &fast, &normal) != 2) return -EINVAL;
    if (fast < 1 || fast > 100 || normal < 1 || normal > 1000) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->hysteresis_fast = fast;
    gov->hysteresis_normal = normal;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t battery_guard_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    bool v;
    mutex_lock(&gov->lock);
    v = gov->battery_guard;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%d\n", v);
}
static ssize_t battery_guard_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->battery_guard = !!v;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t poll_ms_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    u32 v;
    mutex_lock(&gov->lock);
    v = gov->poll_ms;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%u\n", v);
}
static ssize_t poll_ms_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    if (v < 100 || v > 5000) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->poll_ms = v;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t cd_thresholds_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    u32 l2t, l3t, l4t, rec, l2g, l3g, l4g;
    mutex_lock(&gov->lock);
    l2t = gov->cd_l2_temp; l3t = gov->cd_l3_temp; l4t = gov->cd_l4_temp; rec = gov->cd_recover;
    l2g = gov->cd_l2_gold_max; l3g = gov->cd_l3_gold_max; l4g = gov->cd_l4_gold_max;
    mutex_unlock(&gov->lock);
    return sprintf(b,
        "%u %u %u %u %u %u %u\n",
        l2t, l3t, l4t, rec, l2g, l3g, l4g);
}
static ssize_t cd_thresholds_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    u32 l2t,l3t,l4t,rec,l2g,l3g,l4g;
    if (sscanf(b, "%u %u %u %u %u %u %u",
               &l2t,&l3t,&l4t,&rec,&l2g,&l3g,&l4g) != 7) return -EINVAL;
    if (l2t < 40 || l2t > 110 || l3t < 45 || l3t > 110 ||
        l4t < 50 || l4t > 115 || rec < 30 || rec > 100 ||
        rec >= l2t || l2t >= l3t || l3t >= l4t)
        return -EINVAL;
    mutex_lock(&gov->lock);
    gov->cd_l2_temp=l2t; gov->cd_l3_temp=l3t; gov->cd_l4_temp=l4t; gov->cd_recover=rec;
    gov->cd_l2_gold_max=l2g; gov->cd_l3_gold_max=l3g; gov->cd_l4_gold_max=l4g;
    clamp_cd_freqs();
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t gpu_caps_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    u32 l2g, l3g, l4g;
    mutex_lock(&gov->lock);
    l2g = gov->cd_l2_gpu_max; l3g = gov->cd_l3_gpu_max; l4g = gov->cd_l4_gpu_max;
    mutex_unlock(&gov->lock);
    return sprintf(b, "%u %u %u\n", l2g, l3g, l4g);
}
static ssize_t gpu_caps_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    u32 l2g,l3g,l4g;
    if (sscanf(b, "%u %u %u", &l2g,&l3g,&l4g) != 3) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->cd_l2_gpu_max=l2g; gov->cd_l3_gpu_max=l3g; gov->cd_l4_gpu_max=l4g;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t bw_floors_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    u32 gpubw[3], llcc[3];
    mutex_lock(&gov->lock);
    gpubw[0] = gov->cd_l2_bw_gpubw; gpubw[1] = gov->cd_l3_bw_gpubw; gpubw[2] = gov->cd_l4_bw_gpubw;
    llcc[0] = gov->cd_l2_bw_llcc; llcc[1] = gov->cd_l3_bw_llcc; llcc[2] = gov->cd_l4_bw_llcc;
    mutex_unlock(&gov->lock);
    return sprintf(b,
        "gpubw %u %u %u\n"
        "llcc %u %u %u\n",
        gpubw[0], gpubw[1], gpubw[2],
        llcc[0], llcc[1], llcc[2]);
}
static ssize_t bw_floors_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    u32 gpubw_l2,gpubw_l3,gpubw_l4, llcc_l2,llcc_l3,llcc_l4;
    if (sscanf(b, "%u %u %u %u %u %u",
               &gpubw_l2,&gpubw_l3,&gpubw_l4,&llcc_l2,&llcc_l3,&llcc_l4) != 6)
        return -EINVAL;
    mutex_lock(&gov->lock);
    gov->cd_l2_bw_gpubw=gpubw_l2; gov->cd_l3_bw_gpubw=gpubw_l3; gov->cd_l4_bw_gpubw=gpubw_l4;
    gov->cd_l2_bw_llcc=llcc_l2; gov->cd_l3_bw_llcc=llcc_l3; gov->cd_l4_bw_llcc=llcc_l4;
    mutex_unlock(&gov->lock);
    return c;
}

static struct kobj_attribute attr_enable      = __ATTR_RW(enable);
static struct kobj_attribute attr_profile     = __ATTR_RW(profile);
static struct kobj_attribute attr_status      = __ATTR_RO(status);
static struct kobj_attribute attr_pid         = __ATTR_RW(game_pid);
static struct kobj_attribute attr_legacy      = __ATTR_RW(legacy);
static struct kobj_attribute attr_hysteresis  = __ATTR_RW(hysteresis);
static struct kobj_attribute attr_cd_thresh   = __ATTR_RW(cd_thresholds);
static struct kobj_attribute attr_batt_guard  = __ATTR_RW(battery_guard);
static struct kobj_attribute attr_poll_ms     = __ATTR_RW(poll_ms);
static struct kobj_attribute attr_gpu_caps    = __ATTR_RW(gpu_caps);
static struct kobj_attribute attr_bw_floors   = __ATTR_RW(bw_floors);

static struct attribute *gov_attrs[] = {
    &attr_enable.attr,
    &attr_profile.attr,
    &attr_status.attr,
    &attr_pid.attr,
    &attr_legacy.attr,
    &attr_hysteresis.attr,
    &attr_cd_thresh.attr,
    &attr_batt_guard.attr,
    &attr_poll_ms.attr,
    &attr_gpu_caps.attr,
    &attr_bw_floors.attr,
    NULL,
};
static struct attribute_group gov_attr_group = { .attrs = gov_attrs };

static struct notifier_block k6a_cpufreq_nb = {
    .notifier_call = cpufreq_notify,
    .priority = INT_MAX,
};

/* ── Init / Exit ─────────────────────────────────────────────────── */
static int __init k6a_gov_init(void) {
    int ret;

    if (LINUX_VERSION_CODE != K6A_GOV_KERNEL_VER) {
        pr_warn("k6a_gov: build/run version delta (%d vs %d) — continuing (built-in)\n",
                K6A_GOV_KERNEL_VER, LINUX_VERSION_CODE);
    }

    gov = kzalloc(sizeof(*gov), GFP_KERNEL);
    if (!gov) return -ENOMEM;
    mutex_init(&gov->lock);

    gov->legacy_mode = legacy_mode;
    gov->hysteresis_fast = 3;
    gov->hysteresis_normal = 10;
    gov->poll_ms = K6A_GOV_KTHREAD_SLEEP_MS;
    gov->profile = profile;

    if (profile <= K6A_PROFILE_MAX) {
        const struct k6a_profile_def *p = &profiles[profile];
        gov->cd_l2_temp=p->cd_l2_temp; gov->cd_l3_temp=p->cd_l3_temp;
        gov->cd_l4_temp=p->cd_l4_temp; gov->cd_recover=p->cd_recover;
        gov->cd_l2_gold_max=p->cd_l2_gold_max;
        gov->cd_l3_gold_max=p->cd_l3_gold_max;
        gov->cd_l4_gold_max=p->cd_l4_gold_max;
        gov->cd_l2_gpu_max=p->cd_l2_gpu_max;
        gov->cd_l3_gpu_max=p->cd_l3_gpu_max;
        gov->cd_l4_gpu_max=p->cd_l4_gpu_max;
        gov->cd_l2_bw_gpubw=p->cd_l2_bw_gpubw;
        gov->cd_l3_bw_gpubw=p->cd_l3_bw_gpubw;
        gov->cd_l4_bw_gpubw=p->cd_l4_bw_gpubw;
        gov->cd_l2_bw_llcc=p->cd_l2_bw_llcc;
        gov->cd_l3_bw_llcc=p->cd_l3_bw_llcc;
        gov->cd_l4_bw_llcc=p->cd_l4_bw_llcc;
    }
    gov->gpu_last_idx = -1;

    INIT_DELAYED_WORK(&gov->freq_init_work, freq_init_worker);
    gov->freq_init_retries = 0;
    schedule_delayed_work(&gov->freq_init_work, 0);

    gov->kobj = kobject_create_and_add("k6a_gov", kernel_kobj);
    if (!gov->kobj) { ret = -ENOMEM; goto err; }
    ret = sysfs_create_group(gov->kobj, &gov_attr_group);
    if (ret) { kobject_put(gov->kobj); goto err; }

    ret = cpufreq_register_notifier(&k6a_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
    if (ret) {
        pr_err("k6a_gov: cpufreq_register_notifier failed: %d\n", ret);
        goto err_sysfs;
    }

    gov->cooling_dev = thermal_cooling_device_register("k6a_gov", gov, &cool_ops);
    if (IS_ERR(gov->cooling_dev)) {
        ret = PTR_ERR(gov->cooling_dev);
        pr_err("k6a_gov: cooling device registration failed: %d\n", ret);
        goto err_cpufreq;
    }

    gov->enabled = true;
    gov->kthread = kthread_run(gov_thread, gov, "k6a_gov");
    if (IS_ERR(gov->kthread)) {
        pr_err("k6a_gov: kthread creation failed\n");
        ret = PTR_ERR(gov->kthread);
        goto err_cooling;
    }

    pr_info("k6a_gov v%s loaded (legacy=%d profile=%d freq_init=deferred)\n",
            K6A_GOV_VERSION, gov->legacy_mode, gov->profile);
    return 0;

err_cooling:
    thermal_cooling_device_unregister(gov->cooling_dev);
err_cpufreq:
    cpufreq_unregister_notifier(&k6a_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
err_sysfs:
    sysfs_remove_group(gov->kobj, &gov_attr_group);
    kobject_put(gov->kobj);
err:
    kfree(gov); gov = NULL;
    return ret;
}

static void __exit k6a_gov_exit(void) {
    if (!gov) return;
    gov->enabled = false;
    kgsl_k6a_set_max_level_idx(0);   /* GPU-Cap freigeben */
    cancel_delayed_work_sync(&gov->freq_init_work);
    if (gov->kthread) kthread_stop(gov->kthread);
    if (gov->cooling_dev && !IS_ERR(gov->cooling_dev))
        thermal_cooling_device_unregister(gov->cooling_dev);
    cpufreq_unregister_notifier(&k6a_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
    sysfs_remove_group(gov->kobj, &gov_attr_group);
    kobject_put(gov->kobj);
    kfree(gov); gov = NULL;
    pr_info("k6a_gov unloaded\n");
}

module_init(k6a_gov_init);
module_exit(k6a_gov_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Badazz89");
MODULE_DESCRIPTION("k6a In-Kernel Gaming Governor v" K6A_GOV_VERSION);
MODULE_VERSION(K6A_GOV_VERSION);
