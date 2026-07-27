#include "susfs.h"

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
