#include "susfs.h"

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
