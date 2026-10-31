#include "susfs.h"

#ifdef CONFIG_KSU_SUSFS_SUS_PATH
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
	return !susfs_is_current_ksu_domain();
}

int susfs_get_data_path(struct path *path) {
	return kern_path("/data", LOOKUP_FOLLOW, path);
}
#endif
