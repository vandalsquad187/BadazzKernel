#include "susfs.h"

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
#define SUSFS_APPEND_FEATURE(s) \
	do { \
		if (SUSFS_ENABLED_FEATURES_SIZE - strlen(info->enabled_features) - 1 > 0) \
			strlcat(info->enabled_features, (s), SUSFS_ENABLED_FEATURES_SIZE); \
	} while (0)

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_SUS_PATH\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MOUNT
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_SUS_MOUNT\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_SUS_KSTAT\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_SPOOF_UNAME\n");
#endif
#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_ENABLE_LOG\n");
#endif
#ifdef CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG\n");
#endif
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_OPEN_REDIRECT\n");
#endif
#ifdef CONFIG_KSU_SUSFS_SUS_MAP
	SUSFS_APPEND_FEATURE("CONFIG_KSU_SUSFS_SUS_MAP\n");
#endif
#undef SUSFS_APPEND_FEATURE
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
