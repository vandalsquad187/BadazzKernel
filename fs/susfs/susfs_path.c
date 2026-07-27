#include "susfs.h"

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
