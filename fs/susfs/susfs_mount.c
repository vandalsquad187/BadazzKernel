#include "susfs.h"

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
