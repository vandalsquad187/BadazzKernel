#ifndef _UAPI_LINUX_KSU_APP_PROFILE_H
#define _UAPI_LINUX_KSU_APP_PROFILE_H

#define KSU_MAX_GROUPS 32
#define KSU_SELINUX_DOMAIN_LEN 256
#define KSU_KEY_LEN 256

#define KSU_NS_INHERITED 0

#define KSU_DEFAULT_SELINUX_DOMAIN "ksu"

#define KSU_APP_PROFILE_VER 3

struct root_profile {
    uid_t uid;
    gid_t gid;
    uid_t groups[KSU_MAX_GROUPS];
    int groups_count;
    char selinux_domain[KSU_SELINUX_DOMAIN_LEN];
    struct {
        __u64 effective;
        __u64 permitted;
        __u64 inheritable;
    } capabilities;
    int namespaces;
};

struct non_root_profile {
    bool umount_modules;
};

struct app_profile {
    __u32 version;
    uid_t uid;
    uid_t curr_uid;
    gid_t gid;
    uid_t groups[KSU_MAX_GROUPS];
    int groups_count;

    struct {
        struct root_profile profile;
        bool use_default;
    } rp_config;

    struct {
        struct non_root_profile profile;
        bool use_default;
    } nrp_config;

    char key[KSU_KEY_LEN];
    bool allow_su;
};

#endif
