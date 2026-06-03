#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
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
	if (ksu_debug_buf_len > 0)
		seq_printf(m, "%s", ksu_debug_buf);
	spin_unlock(&ksu_debug_lock);

	seq_printf(m, "\n--- snapshot ---\n"
		      "ksu_manager_appid: %d\n"
		      "ksu_manager_appid_valid: %d\n",
		      ksu_get_manager_appid(),
		      ksu_is_manager_appid_valid());

	return 0;
}

static int ksu_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, ksu_debug_show, NULL);
}

static const struct file_operations ksu_debug_fops = {
	.open = ksu_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void __init ksu_debug_init(void)
{
	if (!proc_create("ksu_debug", 0444, NULL, &ksu_debug_fops))
		pr_err("ksu_debug: failed to create /proc/ksu_debug\n");
	else
		pr_info("ksu_debug: /proc/ksu_debug created (world-readable)\n");
}

void __exit ksu_debug_exit(void)
{
	remove_proc_entry("ksu_debug", NULL);
}
