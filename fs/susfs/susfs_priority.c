#include "susfs.h"

#ifdef CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER

#include <linux/atomic.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rcupdate.h>
#include <linux/user_namespace.h>

struct susfs_priority_uid_filter_bitmap {
	struct rcu_head			rcu;
	unsigned long			bits[SUSFS_UID_FILTER_BITMAP_LEN];
};

static struct susfs_priority_uid_filter_bitmap __rcu *priority_uid_filter_bitmap;
static DEFINE_MUTEX(susfs_mutex_lock_priority_uid_filter);
static char *susfs_priority_app_strings = NULL;
static atomic_t susfs_priority_app_strings_running = ATOMIC_INIT(0);

static struct cred *susfs_switch_to_ksu_domain(void) {
	struct cred *orig_cred = NULL;
	struct cred *cred = NULL;

	orig_cred = prepare_creds();
	if (!orig_cred) {
		SUSFS_LOGE("failed to prepare creds!\n");
		return NULL;
	}

	cred = prepare_creds();
	if (!cred) {
		SUSFS_LOGE("failed to prepare creds!\n");
		abort_creds(orig_cred);
		return NULL;
	}

	setup_selinux("u:r:ksu:s0", cred);
	commit_creds(cred);

	if (!susfs_is_current_ksu_domain()) {
		SUSFS_LOGE("domain is not ksu, restoring the original creds\n");
		commit_creds(orig_cred);
		return NULL;
	}

	return orig_cred;
}

static void susfs_restore_creds(struct cred *orig_cred) {
	if (!orig_cred)
		return;

	commit_creds(orig_cred);
}

static bool susfs_is_valid_app_name(const char *app_name) {
	const char *p = app_name;
	size_t len = strlen(app_name);

	if (len == 0 || len > 255)
		return false;

	if (!strcmp(app_name, ".") || !strcmp(app_name, ".."))
		return false;

	for (; *p != '\0'; p++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
			(*p >= '0' && *p <= '9') || *p == '.' || *p == '_')
			continue;
		return false;
	}

	return true;
}

static int susfs_resolve_priority_app_uid(const char *app_name, uid_t *uid) {
	char app_path[512];
	struct path path;
	struct kstat st;
	int len;
	int ret;

	len = scnprintf(app_path, sizeof(app_path), "/data/data/%s", app_name);
	if (len < 0 || len >= sizeof(app_path)) {
		SUSFS_LOGE("the app name '%s' is too long\n", app_name);
		return -EINVAL;
	}

	ret = kern_path(app_path, LOOKUP_FOLLOW, &path);
	if (ret) {
		SUSFS_LOGE("failed to look up '%s': %d\n", app_path, ret);
		return ret;
	}

	ret = vfs_getattr(&path, &st, STATX_BASIC_STATS, 0);
	path_put(&path);
	if (ret) {
		SUSFS_LOGE("failed to get the attributes of '%s': %d\n", app_path, ret);
		return ret;
	}

	*uid = from_kuid(&init_user_ns, st.uid);
	if (*uid < SUSFS_UID_FILTER_MIN_UID ||
		*uid > SUSFS_UID_FILTER_MAX_UID)
		return -EINVAL;

	return 0;
}

static int susfs_parse_priority_app_names(char *app_str, unsigned long *bits, bool skip_failed) {
	char *token = NULL;
	char *saveptr = app_str;
	uid_t uid = 0;
	int ret;

	while ((token = strsep(&saveptr, ",")) != NULL) {
		token = strim(token);
		if (*token == '\0')
			continue;

		if (!susfs_is_valid_app_name(token)) {
			SUSFS_LOGE("invalid app name '%s'\n", token);
			return -EINVAL;
		}

		ret = susfs_resolve_priority_app_uid(token, &uid);
		if (ret) {
			if (skip_failed)
				continue;
			return ret;
		}

		if (uid < SUSFS_UID_FILTER_MIN_UID || uid > SUSFS_UID_FILTER_MAX_UID) {
			SUSFS_LOGE("the uid %u of the app '%s' is out of the allowed range [%d, %d]\n",
					uid, token, SUSFS_UID_FILTER_MIN_UID,
					SUSFS_UID_FILTER_MAX_UID);
			if (!skip_failed)
				return -EINVAL;
			continue;
		}

		set_bit(uid - SUSFS_UID_FILTER_MIN_UID, bits);
		SUSFS_LOGI("app_name '%s', uid '%u' is now added to priority_uid_filter_bitmap\n", token, uid);
	}

	return 0;
}

void susfs_set_priority_uid_filter(void __user **user_info) {
	struct st_susfs_priority_uid_filter *info = NULL;
	struct susfs_priority_uid_filter_bitmap *new_bitmap = NULL;
	struct susfs_priority_uid_filter_bitmap *old_bitmap = NULL;
	char *new_app_strings = NULL;
	struct cred *orig_cred = NULL;
	size_t app_str_len = 0;
	int err = 0;
	static bool is_uid_filter_set_before = false;

	info = kvmalloc(sizeof(*info), GFP_KERNEL | __GFP_ZERO);
	if (!info) {
		err = -ENOMEM;
		goto out_copy_to_user;
	}

	if (copy_from_user(info, (struct st_susfs_priority_uid_filter __user*)*user_info, sizeof(*info))) {
		err = -EFAULT;
		goto out_copy_to_user;
	}

	if (!memchr(info->app_strings, '\0', sizeof(info->app_strings))) {
		SUSFS_LOGE("the app names string is not null terminated\n");
		err = -EINVAL;
		goto out_copy_to_user;
	}

	if (*info->app_strings == '\0') {
		SUSFS_LOGE("the app names string cannot be empty\n");
		err = -EINVAL;
		goto out_copy_to_user;
	}

	app_str_len = strlen(info->app_strings);

	new_app_strings = kvmalloc(app_str_len + 1, GFP_KERNEL | __GFP_ZERO);
	if (!new_app_strings) {
		err = -ENOMEM;
		goto out_copy_to_user;
	}
	memcpy(new_app_strings, info->app_strings, app_str_len + 1);

	new_bitmap = (struct susfs_priority_uid_filter_bitmap *)kzalloc(sizeof(*new_bitmap), GFP_KERNEL);
	if (!new_bitmap) {
		err = -ENOMEM;
		goto out_free_new_app_strings;
	}

	orig_cred = susfs_switch_to_ksu_domain();
	if (!orig_cred) {
		err = -EINVAL;
		goto out_free_new_bitmap;
	}

	if (is_uid_filter_set_before)
		SUSFS_LOGI("renewing the priority uid filter\n");
	err = susfs_parse_priority_app_names(info->app_strings, new_bitmap->bits, false);

	susfs_restore_creds(orig_cred);
	orig_cred = NULL;

	if (err)
		goto out_free_new_bitmap;

	mutex_lock(&susfs_mutex_lock_priority_uid_filter);
	old_bitmap = rcu_dereference_protected(priority_uid_filter_bitmap, lockdep_is_held(&susfs_mutex_lock_priority_uid_filter));
	rcu_assign_pointer(priority_uid_filter_bitmap, new_bitmap);
	new_bitmap = NULL;
	kvfree(susfs_priority_app_strings);
	susfs_priority_app_strings = new_app_strings;
	new_app_strings = NULL;
	if (!is_uid_filter_set_before)
		is_uid_filter_set_before = true;
	mutex_unlock(&susfs_mutex_lock_priority_uid_filter);

	kfree_rcu(old_bitmap, rcu);

	SUSFS_LOGI("the priority uid filter is set\n");
	err = 0;

out_free_new_bitmap:
	if (new_bitmap)
		kfree(new_bitmap);
out_free_new_app_strings:
	if (new_app_strings)
		kvfree(new_app_strings);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_priority_uid_filter __user*)*user_info)->err, &err, sizeof(err))) {
		err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SET_PRIORITY_UID_FILTER -> ret: %d\n", err);
	if (info)
		kvfree(info);
}

static int susfs_priority_app_strings_worker(void *data) {
	struct susfs_priority_uid_filter_bitmap *new_bitmap = NULL;
	struct susfs_priority_uid_filter_bitmap *old_bitmap = NULL;
	struct cred *orig_cred = NULL;
	char *app_strings = NULL;
	size_t app_str_len = 0;
	int ret = 0;

	orig_cred = susfs_switch_to_ksu_domain();
	if (!orig_cred)
		goto out;

	mutex_lock(&susfs_mutex_lock_priority_uid_filter);
	if (susfs_priority_app_strings) {
		app_str_len = strlen(susfs_priority_app_strings);
		app_strings = kvmalloc(app_str_len + 1, GFP_KERNEL | __GFP_ZERO);
		if (app_strings)
			memcpy(app_strings, susfs_priority_app_strings, app_str_len + 1);
	}
	mutex_unlock(&susfs_mutex_lock_priority_uid_filter);

	if (!app_strings)
		goto out_restore_creds;

	new_bitmap = (struct susfs_priority_uid_filter_bitmap *)kzalloc(sizeof(*new_bitmap), GFP_KERNEL);
	if (!new_bitmap)
		goto out_restore_creds;

	ret = susfs_parse_priority_app_names(app_strings, new_bitmap->bits, true);
	if (ret)
		goto out_restore_creds;

	mutex_lock(&susfs_mutex_lock_priority_uid_filter);
	old_bitmap = rcu_dereference_protected(priority_uid_filter_bitmap, lockdep_is_held(&susfs_mutex_lock_priority_uid_filter));
	rcu_assign_pointer(priority_uid_filter_bitmap, new_bitmap);
	new_bitmap = NULL;
	mutex_unlock(&susfs_mutex_lock_priority_uid_filter);

	kfree_rcu(old_bitmap, rcu);

	SUSFS_LOGI("the uids of the priority uid filter are resolved again\n");

out_restore_creds:
	susfs_restore_creds(orig_cred);
	orig_cred = NULL;
out:
	if (new_bitmap)
		kfree(new_bitmap);
	if (app_strings)
		kvfree(app_strings);
	atomic_set(&susfs_priority_app_strings_running, 0);

	return ret;
}

void susfs_parse_priority_app_strings(void) {
	if (atomic_cmpxchg(&susfs_priority_app_strings_running, 0, 1))
		return;

	if (IS_ERR(kthread_run(susfs_priority_app_strings_worker, NULL, "susfs_gpsp_appstr"))) {
		atomic_set(&susfs_priority_app_strings_running, 0);
		SUSFS_LOGE("failed to create the thread susfs_priority_app_strings\n");
	}
}

bool susfs_is_priority_uid_filtered(uid_t uid) {
	struct susfs_priority_uid_filter_bitmap *bitmap;
	uid_t app_id = uid % 100000;

	if (app_id < SUSFS_UID_FILTER_MIN_UID ||
		app_id > SUSFS_UID_FILTER_MAX_UID)
		return false;

	bitmap = rcu_dereference(priority_uid_filter_bitmap);
	if (likely(bitmap)) {
		if (test_bit(app_id - SUSFS_UID_FILTER_MIN_UID, bitmap->bits))
			return true;
	}

	return false;
}

#endif // #ifdef CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER
