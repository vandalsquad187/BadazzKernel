#ifndef _UAPI_LINUX_KSU_SULOG_H
#define _UAPI_LINUX_KSU_SULOG_H

#include <linux/types.h>

#define KSU_SULOG_EVENT_VERSION 1

#define KSU_SULOG_EVENT_IOCTL_GRANT_ROOT 1
#define KSU_SULOG_EVENT_ROOT_EXECVE      2
#define KSU_SULOG_EVENT_SUCOMPAT         3

#define KSU_SULOG_MAX_ARG_STRINGS 32
#define KSU_SULOG_MAX_ARG_CHUNK 4096
#define KSU_SULOG_MAX_EVENT_STRING_SIZE 4096

#define KSU_SULOG_TASK_COMM_LEN 16

struct ksu_sulog_event {
	__u16 version;
	__u16 event_type;
	__s32 retval;
	__s32 pid;
	__s32 tgid;
	__s32 ppid;
	__u32 uid;
	__u32 euid;
	char comm[KSU_SULOG_TASK_COMM_LEN];
	__u32 filename_len;
	__u32 argv_len;
};

#endif
