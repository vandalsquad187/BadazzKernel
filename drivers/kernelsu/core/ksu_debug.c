#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "klog.h"
#include "ksu_debug.h"
#include "manager/manager_identity.h"
#include "supercall/supercall.h"

#define KSU_DEBUG_BUF_SIZE 4096

static char ksu_debug_buf[KSU_DEBUG_BUF_SIZE];
static size_t ksu_debug_buf_len;
static DEFINE_SPINLOCK(ksu_debug_lock);

void ksu_debug_printf(const char *fmt, ...)
{
    va_list args;
    char buf[256];
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len <= 0)
        return;

    spin_lock(&ksu_debug_lock);
    if (ksu_debug_buf_len + len < KSU_DEBUG_BUF_SIZE - 1) {
        memcpy(ksu_debug_buf + ksu_debug_buf_len, buf, len + 1);
        ksu_debug_buf_len += len;
    }
    spin_unlock(&ksu_debug_lock);
}

static ssize_t ksu_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
                              char *buf)
{
    size_t len = 0;

    spin_lock(&ksu_debug_lock);
    if (ksu_debug_buf_len > 0)
        len = scnprintf(buf, PAGE_SIZE, "%s", ksu_debug_buf);
    spin_unlock(&ksu_debug_lock);

    len += scnprintf(buf + len, PAGE_SIZE - len,
                     "\n--- snapshot ---\n"
                     "ksu_manager_appid: %d\n"
                     "ksu_manager_appid_valid: %d\n",
                     ksu_get_manager_appid(),
                     ksu_is_manager_appid_valid());

    return len;
}

static struct kobj_attribute ksu_debug_attr = __ATTR_RO(ksu_debug);

static struct attribute *ksu_debug_attrs[] = {
    &ksu_debug_attr.attr,
    NULL,
};

static struct attribute_group ksu_debug_attr_group = {
    .attrs = ksu_debug_attrs,
};

void __init ksu_debug_init(void)
{
    int ret = sysfs_create_group(kernel_kobj, &ksu_debug_attr_group);
    if (ret)
        pr_err("ksu_debug: failed to create sysfs group: %d\n", ret);
    else
        pr_info("ksu_debug: /sys/kernel/ksu_debug created (readable by all)\n");
}

void __exit ksu_debug_exit(void)
{
    sysfs_remove_group(kernel_kobj, &ksu_debug_attr_group);
}
