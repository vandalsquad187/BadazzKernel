#include "susfs.h"

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
#include <linux/atomic.h>
#include <linux/bitmap.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/rcupdate.h>
#include <linux/user_namespace.h>

static LIST_HEAD(susfs_sus_path_loop_list);
static DEFINE_MUTEX(susfs_sus_path_loop_mutex);
static unsigned int susfs_sus_path_loop_cnt;   /* unter susfs_sus_path_loop_mutex */

void susfs_add_sus_path_loop(void __user **user_info) {
	struct st_susfs_sus_path info = {0};
	struct st_susfs_sus_path_list *new_entry = NULL, *e;
	bool added = false, dup = false;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';   /* nie ungeterminiert in kern_path() */
	if (*info.target_pathname == '\0') { info.err = -EINVAL; goto out; }
	new_entry = kzalloc(sizeof(*new_entry), GFP_KERNEL);
	if (!new_entry) { info.err = -ENOMEM; goto out; }
	memcpy(&new_entry->info, &info, sizeof(info));
	strscpy(new_entry->target_pathname, info.target_pathname, sizeof(new_entry->target_pathname));
	mutex_lock(&susfs_sus_path_loop_mutex);
	list_for_each_entry(e, &susfs_sus_path_loop_list, list) {
		if (!strcmp(e->target_pathname, info.target_pathname)) { dup = true; break; }
	}
	if (dup) {
		info.err = 0;                         /* idempotent: Pfad ist schon eingetragen */
	} else if (susfs_sus_path_loop_cnt >= SUSFS_MAX_SUS_PATH_LOOP) {
		info.err = -ENOSPC;
	} else {
		list_add_tail_rcu(&new_entry->list, &susfs_sus_path_loop_list);
		susfs_sus_path_loop_cnt++;
		added = true;
		info.err = 0;
	}
	mutex_unlock(&susfs_sus_path_loop_mutex);
	if (added)
		SUSFS_LOGI("Added path loop entry: %s\n", info.target_pathname);
out:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
	/* nach list_add_tail_rcu() gehoert der Eintrag der Liste: nur freigeben, was nie drin stand */
	if (new_entry && !added)
		kfree(new_entry);
}

void susfs_run_sus_path_loop(void) {
	struct st_susfs_sus_path_list *entry;
	struct path path;

	/* kern_path()/path_put() koennen schlafen: Mutex statt rcu_read_lock() */
	mutex_lock(&susfs_sus_path_loop_mutex);
	list_for_each_entry(entry, &susfs_sus_path_loop_list, list) {
		if (kern_path(entry->target_pathname, LOOKUP_FOLLOW, &path) == 0) {
			struct inode *inode = d_backing_inode(path.dentry);
			if (inode && inode->i_mapping)
				set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
			path_put(&path);
		}
	}
	mutex_unlock(&susfs_sus_path_loop_mutex);
}

void susfs_add_sus_path(void __user **user_info) {
	struct st_susfs_sus_path info = {0};
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';   /* nie ungeterminiert in kern_path() */
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

/* Gegenstueck zu add: Loop-Eintrag entfernen und das Sus-Flag am Inode loeschen */
void susfs_del_sus_path(void __user **user_info) {
	struct st_susfs_sus_path info = {0};
	struct st_susfs_sus_path_list *e, *tmp;
	struct path path;
	struct inode *inode;
	bool found = false;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out;
	}
	info.target_pathname[sizeof(info.target_pathname) - 1] = '\0';
	if (*info.target_pathname == '\0') { info.err = -EINVAL; goto out; }

	/* Leser der Liste laufen nur unter diesem Mutex (susfs_run_sus_path_loop): list_del + kfree reicht */
	mutex_lock(&susfs_sus_path_loop_mutex);
	list_for_each_entry_safe(e, tmp, &susfs_sus_path_loop_list, list) {
		if (!strcmp(e->target_pathname, info.target_pathname)) {
			list_del(&e->list);
			kfree(e);
			susfs_sus_path_loop_cnt--;
			found = true;
			break;
		}
	}
	mutex_unlock(&susfs_sus_path_loop_mutex);

	if (!kern_path(info.target_pathname, LOOKUP_FOLLOW, &path)) {
		inode = d_backing_inode(path.dentry);
		if (inode && inode->i_mapping) {
			clear_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
			found = true;
		}
		path_put(&path);
	}
	info.err = found ? 0 : -ENOENT;
	SUSFS_LOGI("Removed sus path: %s (%d)\n", info.target_pathname, info.err);
out:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err)))
		info.err = -EFAULT;
}

struct susfs_sus_path_uid_filter_bitmap {
	struct rcu_head			rcu;
	unsigned long			bits[SUSFS_UID_FILTER_BITMAP_LEN];
};

static struct susfs_sus_path_uid_filter_bitmap __rcu *sus_path_uid_filter_bitmap;
static DEFINE_MUTEX(susfs_mutex_lock_sus_path_uid_filter);
static char *susfs_sus_path_app_strings = NULL;
static atomic_t susfs_sus_path_app_strings_running = ATOMIC_INIT(0);

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

static bool susfs_is_valid_sus_path_app_name(const char *app_name) {
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

static int susfs_resolve_sus_path_app_uid(const char *app_name, uid_t *uid) {
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

static int susfs_parse_sus_path_app_names(char *app_str, unsigned long *bits, bool skip_failed) {
	char *token = NULL;
	char *saveptr = app_str;
	uid_t uid = 0;
	int ret;

	while ((token = strsep(&saveptr, ",")) != NULL) {
		token = strim(token);
		if (*token == '\0')
			continue;

		if (!susfs_is_valid_sus_path_app_name(token)) {
			SUSFS_LOGE("invalid app name '%s'\n", token);
			return -EINVAL;
		}

		ret = susfs_resolve_sus_path_app_uid(token, &uid);
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
		SUSFS_LOGI("app_name '%s', uid '%u' is now added to sus_path_uid_filter_bitmap\n", token, uid);
	}

	return 0;
}

void susfs_set_sus_path_uid_filter(void __user **user_info) {
	struct st_susfs_sus_path_uid_filter *info = NULL;
	struct susfs_sus_path_uid_filter_bitmap *new_bitmap = NULL;
	struct susfs_sus_path_uid_filter_bitmap *old_bitmap = NULL;
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

	if (copy_from_user(info, (struct st_susfs_sus_path_uid_filter __user*)*user_info, sizeof(*info))) {
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

	new_bitmap = (struct susfs_sus_path_uid_filter_bitmap *)kzalloc(sizeof(*new_bitmap), GFP_KERNEL);
	if (!new_bitmap) {
		err = -ENOMEM;
		goto out_free_new_app_strings;
	}

	orig_cred = susfs_switch_to_ksu_domain();
	if (!orig_cred) {
		err = -EINVAL;
		goto out_free_new_bitmap;
	}

	mutex_lock(&susfs_mutex_lock_sus_path_uid_filter);
	if (is_uid_filter_set_before)
		SUSFS_LOGI("renewing the sus path uid filter\n");
	mutex_unlock(&susfs_mutex_lock_sus_path_uid_filter);
	err = susfs_parse_sus_path_app_names(info->app_strings, new_bitmap->bits, false);

	susfs_restore_creds(orig_cred);
	orig_cred = NULL;

	if (err)
		goto out_free_new_bitmap;

	mutex_lock(&susfs_mutex_lock_sus_path_uid_filter);
	old_bitmap = rcu_dereference_protected(sus_path_uid_filter_bitmap, lockdep_is_held(&susfs_mutex_lock_sus_path_uid_filter));
	rcu_assign_pointer(sus_path_uid_filter_bitmap, new_bitmap);
	new_bitmap = NULL;
	kvfree(susfs_sus_path_app_strings);
	susfs_sus_path_app_strings = new_app_strings;
	new_app_strings = NULL;
	if (!is_uid_filter_set_before)
		is_uid_filter_set_before = true;
	mutex_unlock(&susfs_mutex_lock_sus_path_uid_filter);

	if (old_bitmap)
		kfree_rcu(old_bitmap, rcu);

	SUSFS_LOGI("the sus path uid filter is set\n");
	err = 0;

out_free_new_bitmap:
	if (new_bitmap)
		kfree(new_bitmap);
out_free_new_app_strings:
	if (new_app_strings)
		kvfree(new_app_strings);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_path_uid_filter __user*)*user_info)->err, &err, sizeof(err))) {
		err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SET_SUS_PATH_UID_FILTER -> ret: %d\n", err);
	if (info)
		kvfree(info);
}

void susfs_del_sus_path_uid_filter(void __user **user_info) {
	struct susfs_sus_path_uid_filter_bitmap *old_bitmap = NULL;
	int err = 0;

	mutex_lock(&susfs_mutex_lock_sus_path_uid_filter);
	old_bitmap = rcu_dereference_protected(sus_path_uid_filter_bitmap, lockdep_is_held(&susfs_mutex_lock_sus_path_uid_filter));
	rcu_assign_pointer(sus_path_uid_filter_bitmap, NULL);
	kvfree(susfs_sus_path_app_strings);
	susfs_sus_path_app_strings = NULL;
	mutex_unlock(&susfs_mutex_lock_sus_path_uid_filter);

	if (old_bitmap)
		kfree_rcu(old_bitmap, rcu);

	SUSFS_LOGI("the sus path uid filter is deleted\n");

	if (copy_to_user(&((struct st_susfs_uid_filter_del __user*)*user_info)->err, &err, sizeof(err)))
		err = -EFAULT;
	SUSFS_LOGI("CMD_SUSFS_DEL_SUS_PATH_UID_FILTER -> ret: %d\n", err);
}

static int susfs_sus_path_app_strings_worker(void *data) {
	struct susfs_sus_path_uid_filter_bitmap *new_bitmap = NULL;
	struct susfs_sus_path_uid_filter_bitmap *old_bitmap = NULL;
	struct cred *orig_cred = NULL;
	char *app_strings = NULL;
	char *parse_buf = NULL;
	size_t app_str_len = 0;
	int ret = 0;

	orig_cred = susfs_switch_to_ksu_domain();
	if (!orig_cred)
		goto out;

	mutex_lock(&susfs_mutex_lock_sus_path_uid_filter);
	if (susfs_sus_path_app_strings) {
		app_str_len = strlen(susfs_sus_path_app_strings);
		app_strings = kvmalloc(app_str_len + 1, GFP_KERNEL | __GFP_ZERO);
		if (app_strings)
			memcpy(app_strings, susfs_sus_path_app_strings, app_str_len + 1);
	}
	mutex_unlock(&susfs_mutex_lock_sus_path_uid_filter);

	if (!app_strings)
		goto out_restore_creds;

	new_bitmap = (struct susfs_sus_path_uid_filter_bitmap *)kzalloc(sizeof(*new_bitmap), GFP_KERNEL);
	if (!new_bitmap)
		goto out_restore_creds;

	parse_buf = kvmalloc(app_str_len + 1, GFP_KERNEL | __GFP_ZERO);
	if (!parse_buf)
		goto out_restore_creds;
	memcpy(parse_buf, app_strings, app_str_len + 1);

	ret = susfs_parse_sus_path_app_names(parse_buf, new_bitmap->bits, true);
	kvfree(parse_buf);
	parse_buf = NULL;
	if (ret)
		goto out_restore_creds;

	mutex_lock(&susfs_mutex_lock_sus_path_uid_filter);
	if (!susfs_sus_path_app_strings ||
	    strcmp(susfs_sus_path_app_strings, app_strings) != 0) {
		mutex_unlock(&susfs_mutex_lock_sus_path_uid_filter);
		goto out_restore_creds;
	}
	old_bitmap = rcu_dereference_protected(sus_path_uid_filter_bitmap, lockdep_is_held(&susfs_mutex_lock_sus_path_uid_filter));
	rcu_assign_pointer(sus_path_uid_filter_bitmap, new_bitmap);
	new_bitmap = NULL;
	mutex_unlock(&susfs_mutex_lock_sus_path_uid_filter);

	if (old_bitmap)
		kfree_rcu(old_bitmap, rcu);

	SUSFS_LOGI("the uids of the sus path uid filter are resolved again\n");

out_restore_creds:
	susfs_restore_creds(orig_cred);
	orig_cred = NULL;
out:
	if (new_bitmap)
		kfree(new_bitmap);
	if (app_strings)
		kvfree(app_strings);
	atomic_set(&susfs_sus_path_app_strings_running, 0);

	return ret;
}

void susfs_parse_sus_path_app_strings(void) {
	if (atomic_cmpxchg(&susfs_sus_path_app_strings_running, 0, 1))
		return;

	if (IS_ERR(kthread_run(susfs_sus_path_app_strings_worker, NULL, "susfs_spuid_appstr"))) {
		atomic_set(&susfs_sus_path_app_strings_running, 0);
		SUSFS_LOGE("failed to create the thread susfs_sus_path_app_strings\n");
	}
}

bool susfs_is_uid_allowed_for_sus_path(uid_t uid) {
	struct susfs_sus_path_uid_filter_bitmap *bitmap;
	uid_t app_id = uid % 100000;

	if (app_id >= 90000)
		return true;

	if (app_id < SUSFS_UID_FILTER_MIN_UID ||
		app_id > SUSFS_UID_FILTER_MAX_UID)
		return false;

	rcu_read_lock();
	bitmap = rcu_dereference(sus_path_uid_filter_bitmap);
	if (likely(bitmap)) {
		if (test_bit(app_id - SUSFS_UID_FILTER_MIN_UID, bitmap->bits)) {
			rcu_read_unlock();
			return true;
		}
	}
	rcu_read_unlock();
	return false;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
bool susfs_is_inode_sus_path(struct mnt_idmap* idmap, struct inode *inode)
#else
bool susfs_is_inode_sus_path(struct inode *inode)
#endif
{
	struct fuse_inode *fi;
	struct address_space *mapping = inode ? inode->i_mapping : NULL;

	if (!mapping) return false;
	if (inode->i_sb->s_magic == FUSE_SUPER_MAGIC) {
		fi = get_fuse_inode(inode);
		if (!fi || !fi->inode.i_mapping) return false;
		mapping = fi->inode.i_mapping;
	}
	if (!test_bit(AS_FLAGS_SUS_PATH, &mapping->flags))
		return false;
	/* Verstecken trifft untrusted Apps: root/ksud (KSU-Domain) sehen die Wahrheit.
	 * Erst nach dem billigen Flag-Test pruefen, das hier ist der Hot Path der Pfadaufloesung. */
	if (susfs_is_current_ksu_domain())
		return false;
	return !susfs_is_uid_allowed_for_sus_path(current_uid().val);
}

int susfs_get_data_path(struct path *path) {
	return kern_path("/data", LOOKUP_FOLLOW, path);
}
#endif
