kernelsu-objs := core/init.o

kernelsu-objs += feature/kernel_umount.o
kernelsu-objs += feature/sulog.o
kernelsu-objs += feature/sucompat.o

kernelsu-objs += hook/lsm_hook.o
kernelsu-objs += hook/setuid_hook.o
kernelsu-objs += hook/syscall_event_bridge.o
kernelsu-objs += hook/syscall_hook_manager.o
kernelsu-objs += hook/tp_marker.o
ifeq ($(CONFIG_ARM64),y)
kernelsu-objs += hook/arm64/patch_memory.o
kernelsu-objs += hook/arm64/syscall_hook.o
else ifeq ($(CONFIG_X86_64),y)
kernelsu-objs += hook/x86_64/patch_memory.o
kernelsu-objs += hook/x86_64/syscall_hook.o
endif

kernelsu-objs += infra/file_wrapper.o
kernelsu-objs += infra/event_queue.o
kernelsu-objs += infra/seccomp_cache.o
kernelsu-objs += infra/su_mount_ns.o
kernelsu-objs += infra/symbol_resolver.o

ifneq ($(CONFIG_KSU_DISABLE_MANAGER),y)
kernelsu-objs += manager/apk_sign.o
kernelsu-objs += manager/pkg_observer.o
kernelsu-objs += manager/throne_tracker.o
endif

kernelsu-objs += policy/allowlist.o
kernelsu-objs += policy/app_profile.o
kernelsu-objs += policy/feature.o

kernelsu-objs += runtime/boot_event.o
kernelsu-objs += runtime/ksud_integration.o

kernelsu-objs += selinux/selinux.o
kernelsu-objs += selinux/rules.o
kernelsu-objs += selinux/sepolicy.o

kernelsu-objs += sulog/event.o
kernelsu-objs += sulog/fd.o

kernelsu-objs += supercall/dispatch.o
kernelsu-objs += supercall/perm.o
kernelsu-objs += supercall/supercall.o

kernelsu-objs += feature/adb_root.o

kernelsu-objs += extras.o

obj-$(CONFIG_KSU) += kernelsu.o

ccflags-y += -I$(srctree)/security/selinux -I$(srctree)/security/selinux/include
ccflags-y += -I$(objtree)/security/selinux -include $(srctree)/include/uapi/asm-generic/errno.h
ccflags-y += -I$(src) -I$(src)/include

ccflags-y += -DKSU_VERSION=16

KSU_NEW_DCACHE_FLUSH := $(shell grep -q __flush_dcache_area $(srctree)/arch/arm64/include/asm/cacheflush.h ; echo $$?)
$(info -- KSU_NEW_DCACHE_FLUSH: $(KSU_NEW_DCACHE_FLUSH))
ccflags-y += -DKSU_NEW_DCACHE_FLUSH=$(KSU_NEW_DCACHE_FLUSH)

ccflags-y += -Wno-strict-prototypes -Wno-int-conversion -Wno-gcc-compat -Wno-missing-prototypes
ccflags-y += -Wno-declaration-after-statement -Wno-unused-function -Wno-unused-variable
