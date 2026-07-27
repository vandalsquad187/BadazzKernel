#include "susfs.h"

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
DEFINE_STATIC_KEY_TRUE(susfs_is_log_enabled);
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
