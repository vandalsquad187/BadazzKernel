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
#include <asm/setup.h>
#include "fuse/fuse_i.h"

extern bool susfs_is_current_ksu_domain(void);
extern struct cred *ksu_cred;

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
DEFINE_STATIC_KEY_TRUE(susfs_is_log_enabled);
#define SUSFS_LOGI(fmt, ...) if (static_branch_likely(&susfs_is_log_enabled)) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#define SUSFS_LOGE(fmt, ...) if (static_branch_likely(&susfs_is_log_enabled)) pr_err("susfs:[%u][%d][%s]" fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#else
#define SUSFS_LOGI(fmt, ...)
#define SUSFS_LOGE(fmt, ...)
#endif

void susfs_enable_log(void __user **user_info) {
	struct st_susfs_log info = {0};
	if (copy_from_user(&info, (struct st_susfs_log __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (info.enabled) static_branch_enable(&susfs_is_log_enabled);
	else static_branch_disable(&susfs_is_log_enabled);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_log __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
static LIST_HEAD(susfs_sus_path_loop_list);
static DEFINE_MUTEX(susfs_sus_path_loop_mutex);

void susfs_add_sus_path_loop(void __user **user_info) {
	struct st_susfs_sus_path info = {0};
	struct st_susfs_sus_path_list *new_entry = NULL;
	struct path path = {};

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (*info.target_pathname == '\0') { info.err = -EINVAL; goto out; }
	new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) { info.err = -ENOMEM; goto out; }
	memcpy(&new_entry->info, &info, sizeof(info));
	strscpy(new_entry->target_pathname, info.target_pathname, sizeof(new_entry->target_pathname));
	mutex_lock(&susfs_sus_path_loop_mutex);
	list_add_tail_rcu(&new_entry->list, &susfs_sus_path_loop_list);
	mutex_unlock(&susfs_sus_path_loop_mutex);
	SUSFS_LOGI("Added path loop entry: %s\n", info.target_pathname);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
	if (info.err && new_entry)
		kfree(new_entry);
}

void susfs_run_sus_path_loop(void) {
	struct st_susfs_sus_path_list *entry;
	struct path path;

	rcu_read_lock();
	list_for_each_entry_rcu(entry, &susfs_sus_path_loop_list, list) {
		if (kern_path(entry->target_pathname, LOOKUP_FOLLOW, &path) == 0) {
			struct inode *inode = d_backing_inode(path.dentry);
			if (inode && inode->i_mapping)
				set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
			path_put(&path);
		}
	}
	rcu_read_unlock();
}

void susfs_add_sus_path(void __user **user_info) {
	struct st_susfs_sus_path info = {0};
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	info.err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
	if (info.err) goto out;
	inode = d_backing_inode(path.dentry);
	if (!inode || !inode->i_mapping) {
		info.err = -ENOENT;
		goto out_path;
	}
	set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
	info.err = 0;
out_path:
	path_put(&path);
out:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
bool susfs_is_inode_sus_path(struct mnt_idmap* idmap, struct inode *inode)
#else
bool susfs_is_inode_sus_path(struct inode *inode)
#endif
{
	struct fuse_inode *fi;
	if (!inode || !inode->i_mapping) return false;
	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) return false;
		return !!test_bit(AS_FLAGS_SUS_PATH, &fi->inode.i_mapping->flags);
	}
	return !!test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
}

int susfs_get_data_path(struct path *path) {
	return kern_path("/data", LOOKUP_FOLLOW, path);
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
DEFINE_STATIC_KEY_FALSE(susfs_is_hide_sus_mnts_for_non_su_procs_enabled);

void susfs_set_hide_sus_mnts_for_non_su_procs(void __user **user_info) {
	struct st_susfs_hide_sus_mnts_for_non_su_procs info = {0};
	if (copy_from_user(&info, (struct st_susfs_hide_sus_mnts_for_non_su_procs __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (info.enabled)
		static_branch_enable(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled);
	else
		static_branch_disable(&susfs_is_hide_sus_mnts_for_non_su_procs_enabled);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_hide_sus_mnts_for_non_su_procs __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
static DEFINE_HASHTABLE(susfs_sus_kstat_hlist, 10);
static DEFINE_MUTEX(susfs_sus_kstat_mutex);

void susfs_add_sus_kstat(void __user **user_info) {
	struct st_susfs_sus_kstat info = {0};
	struct st_susfs_sus_kstat_hlist *new_entry = NULL;
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_kstat __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (*info.target_pathname == '\0') { info.err = -EINVAL; goto out; }
	info.err = kern_path(info.target_pathname, 0, &path);
	if (info.err) goto out;
	inode = d_backing_inode(path.dentry);
	if (!inode || !inode->i_mapping) { info.err = -ENOENT; goto out_path; }
	new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) { info.err = -ENOMEM; goto out_path; }
	new_entry->target_ino = inode->i_ino;
	new_entry->target_dev = inode->i_sb->s_dev;
	new_entry->is_fuse = (inode->i_sb->s_magic == FUSE_SUPER_MAGIC);
	memcpy(&new_entry->info, &info, sizeof(info));
	set_bit(AS_FLAGS_SUS_KSTAT, &inode->i_mapping->flags);
	mutex_lock(&susfs_sus_kstat_mutex);
	hash_add_rcu(susfs_sus_kstat_hlist, &new_entry->node, new_entry->target_ino);
	mutex_unlock(&susfs_sus_kstat_mutex);
	info.err = 0;
out_path:
	path_put(&path);
out:
	if (copy_to_user(&((struct st_susfs_sus_kstat __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
	if (info.err && new_entry)
		kfree(new_entry);
}

void susfs_update_sus_kstat(void __user **user_info) {
	struct st_susfs_sus_kstat info = {0};
	struct st_susfs_sus_kstat_hlist *entry;

	if (copy_from_user(&info, (struct st_susfs_sus_kstat __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (!info.target_ino) { info.err = -EINVAL; goto out; }
	mutex_lock(&susfs_sus_kstat_mutex);
	hash_for_each_possible(susfs_sus_kstat_hlist, entry, node, info.target_ino) {
		if (entry->target_ino == info.target_ino) {
			memcpy(&entry->info, &info, sizeof(info));
			break;
		}
	}
	mutex_unlock(&susfs_sus_kstat_mutex);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_sus_kstat __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

void susfs_sus_kstat_spoof_generic_fillattr(struct inode *inode, struct kstat *stat) {
	struct st_susfs_sus_kstat_hlist *entry;
	if (!inode || !inode->i_mapping)
		return;
	if (!test_bit(AS_FLAGS_SUS_KSTAT, &inode->i_mapping->flags))
		return;
	rcu_read_lock();
	hash_for_each_possible_rcu(susfs_sus_kstat_hlist, entry, node, inode->i_ino) {
		if (entry->target_ino == inode->i_ino && entry->target_dev == inode->i_sb->s_dev) {
			if (entry->info.flags & KSTAT_SPOOF_INO)
				stat->ino = entry->info.spoofed_ino;
			if (entry->info.flags & KSTAT_SPOOF_DEV)
				stat->dev = entry->info.spoofed_dev;
			if (entry->info.flags & KSTAT_SPOOF_NLINK)
				stat->nlink = entry->info.spoofed_nlink;
			if (entry->info.flags & KSTAT_SPOOF_SIZE)
				stat->size = entry->info.spoofed_size;
			if (entry->info.flags & KSTAT_SPOOF_BLOCKS)
				stat->blocks = entry->info.spoofed_blocks;
			if (entry->info.flags & KSTAT_SPOOF_BLKSIZE)
				stat->blksize = entry->info.spoofed_blksize;
			break;
		}
	}
	rcu_read_unlock();
}
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
void susfs_set_uname(void __user **user_info) {
	struct st_susfs_uname info = {0};
	if (copy_from_user(&info, (struct st_susfs_uname __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (*info.release == '\0' || *info.version == '\0') { info.err = -EFAULT; goto out; }
	down_write(&uts_sem);
	if (strcmp(info.release, "default"))
		memcpy(utsname()->release, info.release, __NEW_UTS_LEN);
	if (strcmp(info.version, "default"))
		memcpy(utsname()->version, info.version, __NEW_UTS_LEN);
	up_write(&uts_sem);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_uname __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}
void susfs_spoof_uname(struct new_utsname* tmp) { }
#endif

#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
static char fake_cmdline[SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE];
DEFINE_STATIC_KEY_FALSE(susfs_is_cmdline_spoof_buffer_set);

void susfs_set_cmdline_or_bootconfig(void __user **user_info) {
	struct st_susfs_spoof_cmdline_or_bootconfig info = {0};
	if (copy_from_user(&info, (struct st_susfs_spoof_cmdline_or_bootconfig __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (*info.fake_cmdline_or_bootconfig == '\0') { info.err = -EFAULT; goto out; }
	memcpy(fake_cmdline, info.fake_cmdline_or_bootconfig, sizeof(fake_cmdline));
	fake_cmdline[SUSFS_FAKE_CMDLINE_OR_BOOTCONFIG_SIZE - 1] = '\0';
	strlcpy(saved_command_line, fake_cmdline, COMMAND_LINE_SIZE);
	if (!static_key_enabled(&susfs_is_cmdline_spoof_buffer_set))
		static_branch_enable(&susfs_is_cmdline_spoof_buffer_set);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_spoof_cmdline_or_bootconfig __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}
void susfs_spoof_cmdline_or_bootconfig(struct seq_file *m) { }
#endif

#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
static DEFINE_MUTEX(susfs_mutex_lock_open_redirect);
static DEFINE_HASHTABLE(OPEN_REDIRECT_HLIST, 10);

void susfs_add_open_redirect(void __user **user_info) {
	struct st_susfs_open_redirect info = {0};
	struct st_susfs_open_redirect_hlist *new_entry = NULL;
	struct path target_path;
	struct inode *target_inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_open_redirect __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (*info.target_pathname == '\0') { info.err = -EINVAL; goto out; }
	if (info.uid_scheme < UID_NON_APP_PROC || info.uid_scheme > UID_UMOUNTED_PROC) {
		info.err = -EINVAL; goto out;
	}
	info.err = kern_path(info.target_pathname, 0, &target_path);
	if (info.err) goto out;
	target_inode = d_backing_inode(target_path.dentry);
	if (!target_inode || !target_inode->i_mapping) {
		info.err = -ENOENT; goto out_path;
	}
	new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) { info.err = -ENOMEM; goto out_path; }
	new_entry->target_ino = target_inode->i_ino;
	memcpy(&new_entry->info, &info, sizeof(info));
	set_bit(AS_FLAGS_OPEN_REDIRECT, &target_inode->i_mapping->flags);
	mutex_lock(&susfs_mutex_lock_open_redirect);
	hash_add_rcu(OPEN_REDIRECT_HLIST, &new_entry->node, new_entry->target_ino);
	mutex_unlock(&susfs_mutex_lock_open_redirect);
	info.err = 0;
out_path:
	path_put(&target_path);
out:
	if (copy_to_user(&((struct st_susfs_open_redirect __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}
#endif

#ifdef CONFIG_KSU_SUSFS_SUS_MAP
void susfs_add_sus_map(void __user **user_info) {
	struct st_susfs_sus_map info = {0};
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_map __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	info.err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
	if (info.err) goto out;
	inode = d_backing_inode(path.dentry);
	if (!inode || !inode->i_mapping) { info.err = -ENOENT; goto out_path; }
	set_bit(AS_FLAGS_SUS_MAP, &inode->i_mapping->flags);
	info.err = 0;
out_path:
	path_put(&path);
out:
	if (copy_to_user(&((struct st_susfs_sus_map __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}
#endif

DEFINE_STATIC_KEY_FALSE(susfs_is_avc_log_spoofing_enabled);

void susfs_set_avc_log_spoofing(void __user **user_info) {
	struct st_susfs_avc_log_spoofing info = {0};
	if (copy_from_user(&info, (struct st_susfs_avc_log_spoofing __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	if (info.enabled) static_branch_enable(&susfs_is_avc_log_spoofing_enabled);
	else static_branch_disable(&susfs_is_avc_log_spoofing_enabled);
	info.err = 0;
out:
	if (copy_to_user(&((struct st_susfs_avc_log_spoofing __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

void susfs_get_enabled_features(void __user **user_info) {
	struct st_susfs_enabled_features *info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) return;
	if (copy_from_user(info, (struct st_susfs_enabled_features __user*)*user_info, sizeof(*info))) {
		info->err = -EFAULT;
		goto out;
	}
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_PATH\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_MOUNT\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_KSTAT\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_UNAME\n");
#endif
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_ENABLE_LOG\n");
#endif
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG\n");
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_OPEN_REDIRECT\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	strcat(info->enabled_features, "CONFIG_KSU_SUSFS_SUS_MAP\n");
#endif
	info->err = 0;
out:
	if (copy_to_user((struct st_susfs_enabled_features __user*)*user_info, info, sizeof(*info)))
		info->err = -EFAULT;
	kfree(info);
}

void susfs_show_variant(void __user **user_info) {
	struct st_susfs_variant info = {0};
	if (copy_from_user(&info, (struct st_susfs_variant __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	strscpy(info.susfs_variant, SUSFS_VARIANT, sizeof(info.susfs_variant)-1);
	info.err = 0;
out:
	if (copy_to_user((struct st_susfs_variant __user*)*user_info, &info, sizeof(info)))
		info.err = -EFAULT;
}

void susfs_show_version(void __user **user_info) {
	struct st_susfs_version info = {0};
	if (copy_from_user(&info, (struct st_susfs_version __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	strscpy(info.susfs_version, SUSFS_VERSION, sizeof(info.susfs_version)-1);
	info.err = 0;
out:
	if (copy_to_user((struct st_susfs_version __user*)*user_info, &info, sizeof(info)))
		info.err = -EFAULT;
}

static void susfs_sdcard_monitor_work_fn(struct work_struct *work);
static DECLARE_DELAYED_WORK(susfs_sdcard_monitor_dwork, susfs_sdcard_monitor_work_fn);

static void susfs_sdcard_monitor_work_fn(struct work_struct *work) {
	struct path path_sdcard, path_android;
	if (kern_path("/sdcard", LOOKUP_FOLLOW, &path_sdcard) == 0) {
		struct inode *inode = d_backing_inode(path_sdcard.dentry);
		if (inode && inode->i_mapping && !test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags)) {
			set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
		}
		path_put(&path_sdcard);
	}
	if (kern_path("/sdcard/Android", LOOKUP_FOLLOW, &path_android) == 0) {
		struct inode *inode = d_backing_inode(path_android.dentry);
		if (inode && inode->i_mapping && !test_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags)) {
			set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
		}
		path_put(&path_android);
	}
	schedule_delayed_work(&susfs_sdcard_monitor_dwork, msecs_to_jiffies(5000));
}

void susfs_start_sdcard_monitor_fn(void) {
	schedule_delayed_work(&susfs_sdcard_monitor_dwork, msecs_to_jiffies(1000));
}
void susfs_init(void) {
	SUSFS_LOGI("SUSFS initialized\n");
}
