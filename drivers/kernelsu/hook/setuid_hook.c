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
#include <linux/rcupdate.h>
#include <uapi/asm-generic/unistd.h>

#include "policy/allowlist.h"
#include "hook/setuid_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "infra/seccomp_cache.h"
#include "supercall/supercall.h"
#include "hook/tp_marker.h"
#include "feature/kernel_umount.h"
#include "ksu_debug.h"
#include "arch.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
/* TWA_RESUME available since 5.9 */
#define KSU_TWA_FLAG TWA_RESUME
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
/* TWA_SIGNAL available since 4.15 */
#define KSU_TWA_FLAG TWA_SIGNAL
#else
/* Pre-4.15: task_work_add takes bool */
#define KSU_TWA_FLAG 1
#endif

static void ksu_clear_seccomp_current(void)
{
    struct task_struct *t;

    clear_thread_flag(TIF_SECCOMP);
    rcu_read_lock();
    for_each_thread(current, t) {
        if (t->stack)
            clear_ti_thread_flag(task_thread_info(t), TIF_SECCOMP);
    }
    rcu_read_unlock();
}

/* Bypass __secure_computing for app processes calling __NR_reboot.
 * This handles the case where libksud runs in a forked child process
 * that inherited the seccomp filter from the Manager.
 */
static int seccomp_bypass_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    int syscallno;

    if (current_uid().val < 10000)
        return 0;

    syscallno = task_pt_regs(current)->syscallno;
    if (syscallno != __NR_reboot)
        return 0;

    /* Skip __secure_computing: set return value (x0) to 0, jump to LR */
    regs->regs[0] = 0;
    regs->pc = regs->regs[30];
    return 1;
}

static struct kprobe seccomp_bypass_kp = {
    .symbol_name = "__secure_computing",
    .pre_handler = seccomp_bypass_pre_handler,
};

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
        ksu_clear_seccomp_current();
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

/* Clear TIF_SECCOMP in child processes/threads forked from app processes */
static int fork_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct task_struct *child;
    long ret = PT_REGS_RC(regs);

    if (ret <= 0)
        return 0;
    if (current_uid().val < 10000)
        return 0;

    rcu_read_lock();
    child = find_task_by_vpid(ret);
    if (child && child->stack)
        clear_ti_thread_flag(task_thread_info(child), TIF_SECCOMP);
    rcu_read_unlock();
    return 0;
}

static struct kretprobe fork_krp = {
    .kp.symbol_name = "_do_fork",
    .handler = fork_ret_handler,
    .data_size = 0,
    .maxactive = 20,
};

int ksu_handle_setresuid(uid_t old_uid, uid_t new_uid)
{
    // we rely on the fact that zygote always call setresuid(3) with same uids

    ksu_debug_printf("handle_setresuid from %d to %d\n", old_uid, new_uid);

    if (unlikely(is_uid_manager(new_uid))) {
        spin_lock_irq(&current->sighand->siglock);
        ksu_seccomp_allow_cache(current->seccomp.filter, __NR_reboot);
        ksu_set_task_tracepoint_flag(current);
        spin_unlock_irq(&current->sighand->siglock);

        ksu_debug_printf("handle_setresuid: install fd for manager %d\n", new_uid);
        ksu_install_fd();
        return 0;
    }

    // Always install fd + disable seccomp so Manager can use supercalls
    if (new_uid >= 10000) {
        ksu_clear_seccomp_current();
        ksu_debug_printf("handle_setresuid: install fd for uid=%d\n", new_uid);
        ksu_install_fd();
    }

    if (ksu_is_allow_uid_for_current(new_uid)) {
        ksu_set_task_tracepoint_flag(current);
    } else {
        ksu_clear_task_tracepoint_flag_if_needed(current);
    }

    // Handle kernel umount
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

    ret = register_kprobe(&seccomp_bypass_kp);
    if (ret) {
        pr_err("kprobe on __secure_computing failed: %d\n", ret);
        ksu_debug_printf("kprobe: __secure_computing FAILED ret=%d\n", ret);
    } else {
        pr_info("kprobe on __secure_computing registered\n");
        ksu_debug_printf("kprobe: __secure_computing OK\n");
    }

    ret = register_kretprobe(&fork_krp);
    if (ret) {
        pr_err("kretprobe on _do_fork failed: %d\n", ret);
        ksu_debug_printf("kretprobe: _do_fork FAILED ret=%d\n", ret);
    } else {
        pr_info("kretprobe on _do_fork registered\n");
        ksu_debug_printf("kretprobe: _do_fork OK\n");
    }

    ksu_kernel_umount_init();
}

void __exit ksu_setuid_hook_exit(void)
{
    unregister_kprobe(&seccomp_bypass_kp);
    unregister_kretprobe(&fork_krp);
    unregister_kretprobe(&setresuid_krp);
    pr_info("ksu_setuid_hook_exit: all probes unregistered\n");
    ksu_kernel_umount_exit();
}
