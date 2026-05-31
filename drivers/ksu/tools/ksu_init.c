#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#define KSU_IOCTL_GET_INFO _IOR('k', 2, __u32)
#define KSU_IOCTL_BECOME_MANAGER _IOW('k', 23, __u32)

struct ksu_get_info_cmd {
	__u32 version;
	__u32 flags;
	__u32 features;
};

int main(int argc, char *argv[], char *envp[])
{
	const char *dev_path = "/dev/ksu";
	int fd;
	struct ksu_get_info_cmd cmd;
	int ret;

	fd = open(dev_path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", dev_path, strerror(errno));
		return 1;
	}

	memset(&cmd, 0, sizeof(cmd));
	ret = ioctl(fd, KSU_IOCTL_GET_INFO, &cmd);
	if (ret < 0) {
		fprintf(stderr, "KSU_IOCTL_GET_INFO failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("KernelSU version: %u, flags: 0x%x, features: 0x%x\n",
	       cmd.version, cmd.flags, cmd.features);

	ret = ioctl(fd, KSU_IOCTL_BECOME_MANAGER);
	if (ret < 0) {
		fprintf(stderr, "KSU_IOCTL_BECOME_MANAGER failed: %s\n", strerror(errno));
		close(fd);
		return 1;
	}

	printf("KSU_BECOME_MANAGER: success! uid=%d, euid=%d\n",
	       getuid(), geteuid());

	close(fd);

	const char *shell = argc > 1 ? argv[1] : "/system/bin/sh";
	printf("Executing: %s\n", shell);

	execve(shell, (char *const *)&argv[1], envp);
	fprintf(stderr, "execve failed: %s\n", strerror(errno));
	return 1;
}
