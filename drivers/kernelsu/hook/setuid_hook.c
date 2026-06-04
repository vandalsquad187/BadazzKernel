#include <linux/compiler.h>
#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/thread_info.h>
#include <linux/seccomp.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <trace/events/sched.h>
#include <uapi/asm-generic/unistd.h>

#include "policy/allowlist.h"
#include "hook/setuid_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "supercall/supercall.h"
#include "hook/tp_marker.h"
#include "feature/kernel_umount.h"
#include "ksu_debug.h"
#include "arch.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
#define KSU_TWA_FLAG TWA_RESUME
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
#define KSU_TWA_FLAG TWA_SIGNAL
#else
#define KSU_TWA_FLAG 1
#endif

static void ksu_kretprobe_install_fd_work(struct callback_head *cb)
{
    kfree(cb);
    ksu_debug_printf("kretprobe: install_fd for manager pid=%d\n", current->pid);
    ksu_install_fd();
}

static int setresuid_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    long ret = PT_REGS_RC(regs);
    if (ret < 0)
        return 0;

    uid_t uid = current_uid().val;
    if (uid >= 10000) {
        {
            struct task_struct *p;
            rcu_read_lock();
            for_each_thread(current, p) {
                clear_ti_thread_flag(task_thread_info(p), TIF_SECCOMP);
            }
            rcu_read_unlock();
        }
        ksu_debug_printf("kretprobe: install fd for uid=%d pid=%d\n", uid, current->pid);

        struct callback_head *cb = kzalloc(sizeof(*cb), GFP_ATOMIC);
        if (cb) {
            cb->func = ksu_kretprobe_install_fd_work;
            task_work_add(current, cb, KSU_TWA_FLAG);
        } else {
            ksu_debug_printf("kretprobe: kzalloc failed for uid=%d\n", uid);
        }
    } else {
        ksu_debug_printf("kretprobe: setresuid uid=%d pid=%d (system uid)\n", uid, current->pid);
    }
    return 0;
}

static struct kretprobe setresuid_krp = {
    .kp.symbol_name = SETRESUID_SYMBOL,
    .handler = setresuid_ret_handler,
    .data_size = 0,
    .maxactive = 20,
};

static void ksu_fork_tracepoint_handler(void *data,
    struct task_struct *parent, struct task_struct *child)
{
    if (parent->cred->uid.val >= 10000)
        clear_ti_thread_flag(task_thread_info(child), TIF_SECCOMP);
}

int ksu_handle_setresuid(uid_t old_uid, uid_t new_uid)
{
    ksu_debug_printf("handle_setresuid from %d to %d\n", old_uid, new_uid);

    if (new_uid >= 10000 && unlikely(!ksu_is_manager_appid_valid())) {
        ksu_set_manager_appid(new_uid % KSU_PER_USER_RANGE);
        pr_info("handle_setresuid: crowned uid=%d as manager (first app)\n", new_uid);
    }

    if (unlikely(is_uid_manager(new_uid))) {
        ksu_debug_printf("handle_setresuid: install fd for manager %d\n", new_uid);
        ksu_install_fd();
        return 0;
    }

    if (new_uid >= 10000) {
        {
            struct task_struct *p;
            rcu_read_lock();
            for_each_thread(current, p) {
                clear_ti_thread_flag(task_thread_info(p), TIF_SECCOMP);
            }
            rcu_read_unlock();
        }
        ksu_debug_printf("handle_setresuid: install fd for uid=%d\n", new_uid);
        ksu_install_fd();
    }

    if (ksu_is_allow_uid_for_current(new_uid)) {
        ksu_set_task_tracepoint_flag(current);
    } else {
        ksu_clear_task_tracepoint_flag_if_needed(current);
    }

    ksu_handle_umount(old_uid, new_uid);

    return 0;
}

void __init ksu_setuid_hook_init(void)
{
    int ret;

    ret = register_kretprobe(&setresuid_krp);
    if (ret) {
        pr_err("kretprobe on %s failed: %d\n", SETRESUID_SYMBOL, ret);
        ksu_debug_printf("kretprobe: FAILED on %s ret=%d\n", SETRESUID_SYMBOL, ret);
    } else {
        pr_info("kretprobe on %s registered\n", SETRESUID_SYMBOL);
        ksu_debug_printf("kretprobe: registered on %s OK\n", SETRESUID_SYMBOL);
    }

    ret = register_trace_sched_process_fork(ksu_fork_tracepoint_handler, NULL);
    if (ret)
        pr_err("register_trace_sched_process_fork failed: %d\n", ret);
    else
        pr_info("trace_sched_process_fork registered\n");

    ksu_kernel_umount_init();
}

void __exit ksu_setuid_hook_exit(void)
{
    unregister_trace_sched_process_fork(ksu_fork_tracepoint_handler, NULL);
    unregister_kretprobe(&setresuid_krp);
    pr_info("ksu_setuid_hook_exit: all probes unregistered\n");
    ksu_kernel_umount_exit();
}
