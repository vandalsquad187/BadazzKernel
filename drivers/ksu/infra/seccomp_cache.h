#ifndef __KSU_H_KERNEL_COMPAT
#define __KSU_H_KERNEL_COMPAT

#include <linux/fs.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
extern void ksu_seccomp_clear_cache(struct seccomp_filter *filter, int nr);
extern void ksu_seccomp_allow_cache(struct seccomp_filter *filter, int nr);
#else
static inline void ksu_seccomp_clear_cache(void *filter, int nr) {}
static inline void ksu_seccomp_allow_cache(void *filter, int nr) {}
#endif

#endif
