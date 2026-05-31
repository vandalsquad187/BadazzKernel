#ifndef _UAPI_LINUX_KSU_SUPERCALL_H
#define _UAPI_LINUX_KSU_SUPERCALL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define KSU_IOCTL_GRANT_ROOT _IOW('k', 1, __u32)
#define KSU_IOCTL_GET_INFO _IOR('k', 2, __u32)
#define KSU_IOCTL_REPORT_EVENT _IOW('k', 3, __u32)
#define KSU_IOCTL_SET_SEPOLICY _IOW('k', 4, __u32)
#define KSU_IOCTL_CHECK_SAFEMODE _IOR('k', 5, __u32)
#define KSU_IOCTL_GET_ALLOW_LIST _IOR('k', 6, __u32)
#define KSU_IOCTL_GET_DENY_LIST _IOR('k', 7, __u32)
#define KSU_IOCTL_NEW_GET_ALLOW_LIST _IOWR('k', 8, __u32)
#define KSU_IOCTL_NEW_GET_DENY_LIST _IOWR('k', 9, __u32)
#define KSU_IOCTL_UID_GRANTED_ROOT _IOWR('k', 10, __u32)
#define KSU_IOCTL_UID_SHOULD_UMOUNT _IOWR('k', 11, __u32)
#define KSU_IOCTL_GET_MANAGER_APPID _IOR('k', 12, __u32)
#define KSU_IOCTL_GET_APP_PROFILE _IOWR('k', 13, __u32)
#define KSU_IOCTL_SET_APP_PROFILE _IOW('k', 14, __u32)
#define KSU_IOCTL_GET_FEATURE _IOWR('k', 15, __u32)
#define KSU_IOCTL_SET_FEATURE _IOW('k', 16, __u32)
#define KSU_IOCTL_GET_WRAPPER_FD _IOR('k', 17, __u32)
#define KSU_IOCTL_MANAGE_MARK _IOW('k', 18, __u32)
#define KSU_IOCTL_NUKE_EXT4_SYSFS _IO('k', 19)
#define KSU_IOCTL_ADD_TRY_UMOUNT _IOW('k', 20, __u32)
#define KSU_IOCTL_SET_INIT_PGRP _IOW('k', 21, __u32)
#define KSU_IOCTL_GET_SULOG_FD _IOR('k', 22, __u32)

#endif
