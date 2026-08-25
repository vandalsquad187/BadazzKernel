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

extern int kgsl_k6a_get_levels(u32 *out, u32 max_n, u32 *cur_idx, u32 *max_idx);

#define K6A_GOV_VERSION       "1.1.2"
#define K6A_GOV_KERNEL_VER    KERNEL_VERSION(4,14,369)
#define K6A_GOV_KTHREAD_SLEEP_MS   250
#define K6A_GOV_MAX_FREQS     32

enum k6a_state { K6A_OFF=0, K6A_GAMING=1, K6A_CD_L2=2, K6A_CD_L3=3, K6A_CD_L4=4 };
enum k6a_profile { K6A_PROFILE_OFF=0, K6A_PROFILE_GAMING=1, K6A_PROFILE_BATTERY=2, K6A_PROFILE_BADAZZ=3, K6A_PROFILE_CUSTOM=4 };

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
    u32 hysteresis_fast, hysteresis_normal;

    u64 state_ts;
    s32 prev_temp;
    u64 throttle_events;

    struct thermal_cooling_device *cooling_dev;
    struct notifier_block cpufreq_nb;

    u32 gold_freqs[K6A_GOV_MAX_FREQS];
    u32 gold_num;

    struct delayed_work freq_init_work;
    int freq_init_retries;
};

struct k6a_gov *gov;

static int legacy_mode = 1;
module_param(legacy_mode, int, 0644);
MODULE_PARM_DESC(legacy_mode, "1 = Kernel Cooldown (CPU-only) (default=1)");

static int profile = K6A_PROFILE_GAMING;
module_param(profile, int, 0644);
MODULE_PARM_DESC(profile, "Default profile: 0=off, 1=gaming, 2=battery, 3=badazz, 4=custom");

/* ── Profile Defaults (Hz values, auto-mapped to table) ──────────── */
struct k6a_profile_def {
    u32 cd_l2_temp, cd_l3_temp, cd_l4_temp, cd_recover;
    u32 cd_l2_gold_max, cd_l3_gold_max, cd_l4_gold_max;
};

static const struct k6a_profile_def profiles[] = {
    [K6A_PROFILE_OFF]     = { 0,0,0,0,   0,0,0 },
    [K6A_PROFILE_GAMING]  = { 80,82,88,76,   1555200,1200000,1000000 },
    [K6A_PROFILE_BATTERY] = { 70,75,80,65,   1400000,1200000,1000000 },
    [K6A_PROFILE_BADAZZ]  = { 85,90,95,80,   1800000,1600000,1400000 },
    [K6A_PROFILE_CUSTOM]  = { 0,0,0,0,       0,0,0 },
};

/* ── Freq Helpers ────────────────────────────────────────────────── */
static void enforce_max_freq(void);

static u32 clamp_freq(u32 *avail, u32 num, u32 requested) {
    u32 best, i;
    if (!num || !avail) return requested;
    best = avail[0];
    for (i = 0; i < num; i++) {
        if (avail[i] <= requested && avail[i] > best)
            best = avail[i];
        if (avail[i] == requested) return requested;
    }
    return best;
}

static void clamp_cd_freqs(void) {
    if (!gov->gold_num) return;
    gov->cd_l2_gold_max = clamp_freq(gov->gold_freqs, gov->gold_num, gov->cd_l2_gold_max);
    gov->cd_l3_gold_max = clamp_freq(gov->gold_freqs, gov->gold_num, gov->cd_l3_gold_max);
    gov->cd_l4_gold_max = clamp_freq(gov->gold_freqs, gov->gold_num, gov->cd_l4_gold_max);
}

static void freq_init_worker(struct work_struct *work) {
    struct k6a_gov *g = container_of(work, struct k6a_gov, freq_init_work.work);
    struct cpufreq_policy *policy;
    struct cpufreq_frequency_table *pos;
    int n;

    policy = cpufreq_cpu_get(6);
    if (policy && policy->freq_table) {
        n = 0;
        cpufreq_for_each_valid_entry(pos, policy->freq_table) {
            if (pos->frequency != CPUFREQ_ENTRY_INVALID && n < K6A_GOV_MAX_FREQS)
                g->gold_freqs[n++] = pos->frequency;
        }
        g->gold_num = n;
        pr_info("k6a_gov: Gold freqs loaded via cpufreq API: %u entries\n", n);
    }
    if (policy) cpufreq_cpu_put(policy);

    {
        u32 gtab[K6A_GOV_MAX_FREQS]; u32 gn = 0, tl = 0;
        int tl_ret = kgsl_k6a_get_levels(gtab, K6A_GOV_MAX_FREQS, &gn, &tl);
        if (tl_ret > 0)
            pr_info("k6a_gov: gpu levels=%u cur_idx=%u max_idx=%u\n",
                    gn, tl, tl_ret > 0 ? (u32)tl_ret : 0);
        else
            pr_info("k6a_gov: gpu levels read failed (%d)\n", tl_ret);
    }

    if (g->gold_num > 0) {
        clamp_cd_freqs();
        pr_info("k6a_gov: freq tables loaded: Gold=%u\n", g->gold_num);

        mutex_lock(&g->lock);
        if (g->state != K6A_OFF && g->enabled && g->legacy_mode)
            enforce_max_freq();
        mutex_unlock(&g->lock);
        return;
    }

    if (++g->freq_init_retries < 30) {
        schedule_delayed_work(&g->freq_init_work, msecs_to_jiffies(500));
    } else {
        pr_warn("k6a_gov: freq init failed after 30 retries, using defaults\n");
    }
}

/* ── CPU Freq Enforcement ────────────────────────────────────────── */
/* policy6 deckt CPU6+7 ab (Cluster-Policy) — ein Clamp gilt fuer beide */
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
    if (!gov || !gov->enabled || !gov->legacy_mode) return;
    max = get_cd_max_freq();
    if (!max) return;
    p = cpufreq_cpu_get(6);
    if (p) {
        if (p->max > max) p->max = max;
        cpufreq_cpu_put(p);
        cpufreq_update_policy(6);   /* legal here: Kthread = process context */
    }
}

/* policy6 deckt CPU6+7 ab (Cluster-Policy) — ein Clamp gilt fuer beide */
static int cpufreq_notify(struct notifier_block *nb, unsigned long e, void *data) {
    struct cpufreq_policy *p;
    u32 max;
    if (!gov || !gov->enabled || !gov->legacy_mode || gov->state < K6A_CD_L2)
        return NOTIFY_OK;
    if (e != CPUFREQ_ADJUST && e != CPUFREQ_INCOMPATIBLE)
        return NOTIFY_OK;
    p = data;
    max = get_cd_max_freq();
    if (!max) return NOTIFY_OK;
    if (p->cpu >= 6 && p->max > max) {
        p->max = max;
        /* NO cpufreq_update_policy() here — causes recursion! */
    }
    return NOTIFY_OK;
}

/* ── Temp Read ───────────────────────────────────────────────────── */
static int read_temp(void) {
    struct thermal_zone_device *tz; int t = 0;
    tz = thermal_zone_get_zone_by_name("cpu-1-0-usr");
    if (!IS_ERR(tz) && !thermal_zone_get_temp(tz, &t) && t > 0) return t / 1000;
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

/* ── State Machine ───────────────────────────────────────────────── */
static void state_machine(void) {
    u64 now = ktime_to_ms(ktime_get());
    s32 delta = gov->temp_celsius - gov->prev_temp;
    u32 dwell = (delta >= 5) ? gov->hysteresis_fast : gov->hysteresis_normal;

    if (gov->state == K6A_OFF && gov->enabled &&
        gov->profile != K6A_PROFILE_OFF) {
        gov->state = K6A_GAMING;
        gov->state_ts = now;
        pr_info("k6a_gov: OFF -> GAMING\n");
    }

    switch (gov->state) {
    case K6A_CD_L4:
        if (gov->temp_celsius < gov->cd_l3_temp && (now - gov->state_ts) >= dwell) {
            gov->state = K6A_CD_L3; gov->state_ts = now;
        }
        break;
    case K6A_CD_L3:
        if (gov->temp_celsius < gov->cd_l2_temp && (now - gov->state_ts) >= dwell) {
            gov->state = K6A_CD_L2; gov->state_ts = now;
        }
        break;
    case K6A_CD_L2:
        if (gov->temp_celsius <= gov->cd_recover && (now - gov->state_ts) >= dwell) {
            gov->state = K6A_GAMING; gov->state_ts = now;
        }
        break;
    case K6A_GAMING:
        if (gov->temp_celsius >= gov->cd_l4_temp) {
            gov->state = K6A_CD_L4; gov->state_ts = now; gov->throttle_events++;
        } else if (gov->temp_celsius >= gov->cd_l3_temp) {
            gov->state = K6A_CD_L3; gov->state_ts = now; gov->throttle_events++;
        } else if (gov->temp_celsius >= gov->cd_l2_temp) {
            gov->state = K6A_CD_L2; gov->state_ts = now; gov->throttle_events++;
        }
        break;
    default:
        break;
    }
    gov->prev_temp = gov->temp_celsius;
}

/* ── Apply Limits ────────────────────────────────────────────────── */
static void apply_limits(void) {
    if (!gov || !gov->legacy_mode) return;
    if (!gov->gold_num) return;
    enforce_max_freq();
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
static int gov_thread(void *data) {
    while (!kthread_should_stop()) {
        mutex_lock(&gov->lock);
        if (gov->enabled) {
            gov->temp_celsius = read_temp();
            if (gov->battery_guard) {
                int bt = read_bat_temp();
                if (bt >= 45 && gov->state == K6A_GAMING) {
                    gov->state = K6A_CD_L2;
                    gov->state_ts = ktime_to_ms(ktime_get());
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
    *s = gov ? gov->state : 0;
    return 0;
}
static int cool_set(struct thermal_cooling_device *c, unsigned long s) {
    if (!gov) return -EINVAL;
    if (s > K6A_CD_L4) s = K6A_CD_L4;   /* thermal-core kann mehr fordern */
    mutex_lock(&gov->lock);
    gov->state = s;
    gov->state_ts = ktime_to_ms(ktime_get());
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
    return sprintf(b, "%d\n", gov->enabled);
}
static ssize_t enable_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->enabled = !!v;
    if (!v) gov->state = K6A_OFF;
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t profile_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    const char *n[] = {"off","gaming","battery","badazz","custom"};
    return sprintf(b, "%s\n", n[gov->profile]);
}
static ssize_t profile_store(struct kobject *k, struct kobj_attribute *a, const char *b, size_t c) {
    unsigned long v;
    if (kstrtoul(b, 10, &v)) return -EINVAL;
    if (v > K6A_PROFILE_CUSTOM) return -EINVAL;
    mutex_lock(&gov->lock);
    gov->profile = v;
    gov->cd_l2_temp=profiles[v].cd_l2_temp;
    gov->cd_l3_temp=profiles[v].cd_l3_temp;
    gov->cd_l4_temp=profiles[v].cd_l4_temp;
    gov->cd_recover=profiles[v].cd_recover;
    gov->cd_l2_gold_max=profiles[v].cd_l2_gold_max;
    gov->cd_l3_gold_max=profiles[v].cd_l3_gold_max;
    gov->cd_l4_gold_max=profiles[v].cd_l4_gold_max;
    clamp_cd_freqs();
    mutex_unlock(&gov->lock);
    return c;
}

static ssize_t status_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    static const char *state_names[] = {"off","gaming","cd_l2","cd_l3","cd_l4"};
    unsigned int si = gov->state;
    if (si > K6A_CD_L4) si = K6A_OFF;
    return sprintf(b,
        "version=%s\n"
        "state=%s\n"
        "temp=%d\n"
        "ticks=%llu\n"
        "throttle_events=%llu\n"
        "profile=%u\n"
        "legacy_mode=%d\n"
        "enabled=%d\n"
        "gold_freqs=%u\n"
        "gold_max=%u\n"
        "gold_max_tbl=%u\n",
        K6A_GOV_VERSION,
        state_names[si],
        gov->temp_celsius,
        gov->ticks,
        gov->throttle_events,
        gov->profile,
        gov->legacy_mode,
        gov->enabled,
        gov->gold_num,
        get_cd_max_freq(),
        gov->gold_num ? gov->gold_freqs[gov->gold_num - 1] : 0);
}

static ssize_t game_pid_show(struct kobject *k, struct kobj_attribute *a, char *b) {
    return sprintf(b, "%d\n", gov->game_pid);
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
    return sprintf(b, "%d\n", gov->legacy_mode);
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
    return sprintf(b, "fast=%u normal=%u\n", gov->hysteresis_fast, gov->hysteresis_normal);
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
    return sprintf(b, "%d\n", gov->battery_guard);
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
    return sprintf(b, "%u\n", gov->poll_ms);
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
    return sprintf(b,
        "%u %u %u %u %u %u %u\n",
        gov->cd_l2_temp, gov->cd_l3_temp, gov->cd_l4_temp, gov->cd_recover,
        gov->cd_l2_gold_max, gov->cd_l3_gold_max, gov->cd_l4_gold_max);
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

static struct kobj_attribute attr_enable      = __ATTR_RW(enable);
static struct kobj_attribute attr_profile     = __ATTR_RW(profile);
static struct kobj_attribute attr_status      = __ATTR_RO(status);
static struct kobj_attribute attr_pid         = __ATTR_RW(game_pid);
static struct kobj_attribute attr_legacy      = __ATTR_RW(legacy);
static struct kobj_attribute attr_hysteresis  = __ATTR_RW(hysteresis);
static struct kobj_attribute attr_cd_thresh   = __ATTR_RW(cd_thresholds);
static struct kobj_attribute attr_batt_guard  = __ATTR_RW(battery_guard);
static struct kobj_attribute attr_poll_ms     = __ATTR_RW(poll_ms);

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

    if (profile <= K6A_PROFILE_CUSTOM) {
        const struct k6a_profile_def *p = &profiles[profile];
        gov->cd_l2_temp=p->cd_l2_temp; gov->cd_l3_temp=p->cd_l3_temp;
        gov->cd_l4_temp=p->cd_l4_temp; gov->cd_recover=p->cd_recover;
        gov->cd_l2_gold_max=p->cd_l2_gold_max;
        gov->cd_l3_gold_max=p->cd_l3_gold_max;
        gov->cd_l4_gold_max=p->cd_l4_gold_max;
    }

    INIT_DELAYED_WORK(&gov->freq_init_work, freq_init_worker);
    gov->freq_init_retries = 0;
    schedule_delayed_work(&gov->freq_init_work, 0);

    gov->kobj = kobject_create_and_add("k6a_gov", kernel_kobj);
    if (!gov->kobj) { ret = -ENOMEM; goto err; }
    ret = sysfs_create_group(gov->kobj, &gov_attr_group);
    if (ret) { kobject_put(gov->kobj); goto err; }

    cpufreq_register_notifier(&k6a_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);

    gov->cooling_dev = thermal_cooling_device_register("k6a_gov", gov, &cool_ops);
    if (IS_ERR(gov->cooling_dev))
        pr_warn("k6a_gov: cooling device registration failed\n");

    gov->enabled = true;
    gov->kthread = kthread_run(gov_thread, gov, "k6a_gov");
    if (IS_ERR(gov->kthread)) {
        pr_err("k6a_gov: kthread creation failed\n");
        ret = PTR_ERR(gov->kthread);
        goto err_kthread;
    }

    pr_info("k6a_gov v%s loaded (legacy=%d profile=%d freq_init=deferred)\n",
            K6A_GOV_VERSION, gov->legacy_mode, gov->profile);
    return 0;

err_kthread:
    if (gov->cooling_dev && !IS_ERR(gov->cooling_dev))
        thermal_cooling_device_unregister(gov->cooling_dev);
    cpufreq_unregister_notifier(&k6a_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);
    sysfs_remove_group(gov->kobj, &gov_attr_group);
    kobject_put(gov->kobj);
err:
    kfree(gov); gov = NULL;
    return ret;
}

static void __exit k6a_gov_exit(void) {
    if (!gov) return;
    gov->enabled = false;
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
