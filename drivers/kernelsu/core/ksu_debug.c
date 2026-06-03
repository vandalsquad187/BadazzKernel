#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/string.h>
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

static int ksu_debug_show(struct seq_file *m, void *v)
{
    spin_lock(&ksu_debug_lock);
    seq_printf(m, "%s", ksu_debug_buf);
    spin_unlock(&ksu_debug_lock);

    seq_printf(m, "\n--- snapshot ---\n");
    seq_printf(m, "ksu_manager_appid: %d\n", ksu_get_manager_appid());
    seq_printf(m, "ksu_manager_appid_valid: %d\n", ksu_is_manager_appid_valid());

    return 0;
}

static int ksu_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, ksu_debug_show, NULL);
}

static const struct file_operations ksu_debug_fops = {
    .owner = THIS_MODULE,
    .open = ksu_debug_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static struct proc_dir_entry *ksu_debug_proc;

void __init ksu_debug_init(void)
{
    ksu_debug_proc = proc_create("ksu_debug", 0444, NULL, &ksu_debug_fops);
    if (!ksu_debug_proc)
        pr_err("ksu_debug: failed to create /proc/ksu_debug\n");
    else
        pr_info("ksu_debug: /proc/ksu_debug created (readable by all)\n");
}

void __exit ksu_debug_exit(void)
{
    if (ksu_debug_proc)
        proc_remove(ksu_debug_proc);
}
