// SPDX-License-Identifier: GPL-2.0
/*
 * k6a Feature Flags - Runtime kernel capability detection for k6a-godmode
 * Exposes /sys/kernel/k6a_features/ with boolean flags and metadata
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/version.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>

#define K6A_KERNEL_VERSION (LINUX_VERSION_CODE)
#define K6A_GIT_HASH "full-synergy"

struct k6a_feature_flags {
	bool cpu_floor;
	bool gpu_floor;
	bool msm_performance;
	bool thermal_writable;
	bool susfs_active;
	u32 kernel_version;
	char git_hash[16];
};

static struct k6a_feature_flags k6a_flags;
static struct kobject *k6a_features_kobj;

static ssize_t cpu_floor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", k6a_flags.cpu_floor);
}

static ssize_t gpu_floor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", k6a_flags.gpu_floor);
}

static ssize_t msm_performance_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", k6a_flags.msm_performance);
}

static ssize_t thermal_writable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", k6a_flags.thermal_writable);
}

static ssize_t susfs_active_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", k6a_flags.susfs_active);
}

static ssize_t kernel_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", k6a_flags.kernel_version);
}

static ssize_t git_hash_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", k6a_flags.git_hash);
}

static struct kobj_attribute cpu_floor_attr = __ATTR_RO(cpu_floor);
static struct kobj_attribute gpu_floor_attr = __ATTR_RO(gpu_floor);
static struct kobj_attribute msm_performance_attr = __ATTR_RO(msm_performance);
static struct kobj_attribute thermal_writable_attr = __ATTR_RO(thermal_writable);
static struct kobj_attribute susfs_active_attr = __ATTR_RO(susfs_active);
static struct kobj_attribute kernel_version_attr = __ATTR_RO(kernel_version);
static struct kobj_attribute git_hash_attr = __ATTR_RO(git_hash);

static struct attribute *k6a_features_attrs[] = {
	&cpu_floor_attr.attr,
	&gpu_floor_attr.attr,
	&msm_performance_attr.attr,
	&thermal_writable_attr.attr,
	&susfs_active_attr.attr,
	&kernel_version_attr.attr,
	&git_hash_attr.attr,
	NULL,
};

static struct attribute_group k6a_features_attr_group = {
	.attrs = k6a_features_attrs,
};

static int __init k6a_features_init(void)
{
	int ret;

	/* Detect all features via config options (compile-time) */
	k6a_flags.cpu_floor = IS_ENABLED(CONFIG_CPU_THERMAL);
	k6a_flags.gpu_floor = 1;  /* BadazzKernel GPU thermal floor always present */
	k6a_flags.msm_performance = IS_ENABLED(CONFIG_MSM_PERFORMANCE);
	k6a_flags.thermal_writable = IS_ENABLED(CONFIG_THERMAL_WRITABLE_TRIPS);
	k6a_flags.susfs_active = IS_ENABLED(CONFIG_KSU_SUSFS);
	k6a_flags.kernel_version = K6A_KERNEL_VERSION;
	strscpy(k6a_flags.git_hash, K6A_GIT_HASH, sizeof(k6a_flags.git_hash));

	/* Create /sys/kernel/k6a_features/ */
	k6a_features_kobj = kobject_create_and_add("k6a_features", kernel_kobj);
	if (!k6a_features_kobj) {
		pr_err("k6a_features: failed to create kobject\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(k6a_features_kobj, &k6a_features_attr_group);
	if (ret) {
		pr_err("k6a_features: failed to create sysfs group: %d\n", ret);
		kobject_put(k6a_features_kobj);
		return ret;
	}

	pr_info("k6a_features: initialized (cpu_floor=%d, gpu_floor=%d, msm_perf=%d, thermal_writable=%d, susfs=%d, version=%u)\n",
		k6a_flags.cpu_floor, k6a_flags.gpu_floor, k6a_flags.msm_performance,
		k6a_flags.thermal_writable, k6a_flags.susfs_active, k6a_flags.kernel_version);

	return 0;
}

static void __exit k6a_features_exit(void)
{
	if (k6a_features_kobj) {
		sysfs_remove_group(k6a_features_kobj, &k6a_features_attr_group);
		kobject_put(k6a_features_kobj);
	}
}

module_init(k6a_features_init);
module_exit(k6a_features_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Badazz89");
MODULE_DESCRIPTION("k6a Feature Flags for k6a-godmode module");
MODULE_VERSION("1.0");