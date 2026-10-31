#ifndef _FS_SUSFS_SUSFS_H
#define _FS_SUSFS_SUSFS_H

#include <linux/version.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/jump_label.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/susfs.h>
#include <linux/utsname.h>
#include <linux/init.h>
#include <linux/srcu.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/hashtable.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include "../fuse/fuse_i.h"

extern bool susfs_is_current_ksu_domain(void);
extern struct cred *ksu_cred;

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
extern struct static_key_true susfs_is_log_enabled;
#define SUSFS_LOGI(fmt, ...) if (static_branch_likely(&susfs_is_log_enabled)) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#define SUSFS_LOGE(fmt, ...) if (static_branch_likely(&susfs_is_log_enabled)) pr_err("susfs:[%u][%d][%s]" fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#else
#define SUSFS_LOGI(fmt, ...)
#define SUSFS_LOGE(fmt, ...)
#endif

void susfs_enable_log(void __user **user_info);
void susfs_init(void);

#endif
