#include "susfs.h"

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
