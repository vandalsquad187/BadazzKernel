#ifndef _UAPI_LINUX_KSU_SUPERCALL_H
#define _UAPI_LINUX_KSU_SUPERCALL_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include "app_profile.h"

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

#define KSU_GET_INFO_FLAG_LKM       (1 << 0)
#define KSU_GET_INFO_FLAG_MANAGER   (1 << 1)
#define KSU_GET_INFO_FLAG_LATE_LOAD (1 << 2)
#define KSU_GET_INFO_FLAG_PR_BUILD  (1 << 3)

#define EVENT_POST_FS_DATA   1
#define EVENT_BOOT_COMPLETED 2
#define EVENT_MODULE_MOUNTED 3

#define KSU_MARK_GET     0
#define KSU_MARK_MARK    1
#define KSU_MARK_UNMARK  2
#define KSU_MARK_REFRESH 3

#define KSU_UMOUNT_WIPE     0
#define KSU_UMOUNT_ADD      1
#define KSU_UMOUNT_DEL      2
#define KSU_UMOUNT_GETSIZE  3
#define KSU_UMOUNT_GETLIST  4

struct ksu_get_info_cmd {
    __u32 version;
    __u32 flags;
    __u32 features;
};

struct ksu_report_event_cmd {
    __u32 event;
};

struct ksu_set_sepolicy_cmd {
    __u64 data;
    __u64 data_len;
};

struct ksu_check_safemode_cmd {
    __u32 in_safe_mode;
};

struct ksu_get_allow_list_cmd {
    __u32 uids[128];
    __u32 count;
};

struct ksu_new_get_allow_list_cmd {
    __s32 count;
    __s32 total_count;
    __s32 uids[];
};

struct ksu_uid_granted_root_cmd {
    __u32 uid;
    __u32 granted;
};

struct ksu_uid_should_umount_cmd {
    __u32 uid;
    __u32 should_umount;
};

struct ksu_get_manager_appid_cmd {
    __u32 appid;
};

struct ksu_get_app_profile_cmd {
    struct app_profile profile;
};

struct ksu_set_app_profile_cmd {
    struct app_profile profile;
};

struct ksu_get_feature_cmd {
    __u32 feature_id;
    __u32 value;
    __u32 supported;
};

struct ksu_set_feature_cmd {
    __u32 feature_id;
    __u32 value;
};

struct ksu_get_wrapper_fd_cmd {
    __s32 fd;
};

struct ksu_manage_mark_cmd {
    __u32 operation;
    __u32 pid;
    __u32 result;
};

struct ksu_nuke_ext4_sysfs_cmd {
    __u64 arg;
};

struct ksu_add_try_umount_cmd {
    __u32 mode;
    __u64 arg;
    __u32 flags;
};

struct ksu_get_sulog_fd_cmd {
    __u32 flags;
};

#endif
