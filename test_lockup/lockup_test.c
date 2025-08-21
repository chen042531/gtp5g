#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Test");
MODULE_DESCRIPTION("Lock competition test module");

/* 测试参数 */
static int test_duration = 30;  /* 测试持续时间(秒) */
static int num_threads = 8;     /* 线程数量 */
static int use_bh_lock = 0;     /* 0=spin_lock, 1=spin_lock_bh, 2=lock_free */
static int work_delay_us = 10;  /* 持锁时间(微秒) */

module_param(test_duration, int, 0644);
module_param(num_threads, int, 0644);
module_param(use_bh_lock, int, 0644);
module_param(work_delay_us, int, 0644);

/* 模拟URR结构 */
struct test_urr {
    /* 有锁版本 */
    spinlock_t lock;
    bool use_vol2;
    u64 vol1_total;
    u64 vol1_uplink;
    u64 vol2_total;
    u64 vol2_uplink;
    
    /* 无锁版本 */
    atomic_t current_idx;
    atomic64_t counters[2][2];  /* [counter_idx][total/uplink] */
    
    /* 统计信息 */
    atomic64_t update_count;
    atomic64_t switch_count;
    atomic64_t lock_contention;
};

static struct test_urr *test_urr;
static struct task_struct **update_threads;
static struct task_struct *switch_thread;
static atomic_t stop_test;

/* 模拟update_period_vol_counter - 有锁版本 */
static void locked_update_counter(struct test_urr *urr, u64 vol)
{
    unsigned long start_time = jiffies;
    
    if (use_bh_lock == 1) {
        spin_lock_bh(&urr->lock);
    } else {
        spin_lock(&urr->lock);
    }
    
    /* 检测锁竞争 */
    if (time_after(jiffies, start_time)) {
        atomic64_inc(&urr->lock_contention);
    }
    
    /* 模拟工作负载 */
    if (work_delay_us > 0) {
        udelay(work_delay_us);
    }
    
    /* 更新计数器 */
    if (urr->use_vol2) {
        urr->vol2_total += vol;
        urr->vol2_uplink += vol / 2;
    } else {
        urr->vol1_total += vol;
        urr->vol1_uplink += vol / 2;
    }
    
    if (use_bh_lock == 1) {
        spin_unlock_bh(&urr->lock);
    } else {
        spin_unlock(&urr->lock);
    }
    
    atomic64_inc(&urr->update_count);
}

/* 模拟update_period_vol_counter - 无锁版本 */
static void lockfree_update_counter(struct test_urr *urr, u64 vol)
{
    int idx = atomic_read(&urr->current_idx);
    
    atomic64_add(vol, &urr->counters[idx][0]);      /* total */
    atomic64_add(vol / 2, &urr->counters[idx][1]);  /* uplink */
    
    atomic64_inc(&urr->update_count);
}

/* 模拟get_and_switch_period_vol_counter - 有锁版本 */
static void locked_switch_counter(struct test_urr *urr)
{
    if (use_bh_lock == 1) {
        spin_lock_bh(&urr->lock);
    } else {
        spin_lock(&urr->lock);
    }
    
    /* 模拟切换逻辑 */
    if (work_delay_us > 0) {
        udelay(work_delay_us * 2);  /* 切换操作稍微耗时一些 */
    }
    
    urr->use_vol2 = !urr->use_vol2;
    
    /* 清零新的计数器 */
    if (urr->use_vol2) {
        urr->vol1_total = 0;
        urr->vol1_uplink = 0;
    } else {
        urr->vol2_total = 0;
        urr->vol2_uplink = 0;
    }
    
    if (use_bh_lock == 1) {
        spin_unlock_bh(&urr->lock);
    } else {
        spin_unlock(&urr->lock);
    }
    
    atomic64_inc(&urr->switch_count);
}

/* 模拟get_and_switch_period_vol_counter - 无锁版本 */
static void lockfree_switch_counter(struct test_urr *urr)
{
    int old_idx = atomic_read(&urr->current_idx);
    int new_idx = 1 - old_idx;
    
    /* 清零新计数器 */
    atomic64_set(&urr->counters[new_idx][0], 0);
    atomic64_set(&urr->counters[new_idx][1], 0);
    
    /* 内存屏障 */
    smp_wmb();
    
    /* 切换 */
    atomic_set(&urr->current_idx, new_idx);
    
    /* 等待完成 */
    smp_rmb();
    cpu_relax();
    
    atomic64_inc(&urr->switch_count);
}

/* 更新线程 */
static int update_thread_fn(void *data)
{
    int thread_id = (long)data;
    u64 vol = 1000 + thread_id;  /* 每个线程使用不同的volume */
    
    printk(KERN_INFO "Update thread %d started on CPU %d\n", thread_id, smp_processor_id());
    
    while (!atomic_read(&stop_test) && !kthread_should_stop()) {
        if (use_bh_lock == 2) {
            lockfree_update_counter(test_urr, vol);
        } else {
            locked_update_counter(test_urr, vol);
        }
        
        /* 模拟高频更新，但不要过于密集 */
        usleep_range(10, 50);
        
        if (need_resched()) {
            schedule();
        }
    }
    
    printk(KERN_INFO "Update thread %d stopping\n", thread_id);
    return 0;
}

/* 切换线程 (模拟15秒报告) */
static int switch_thread_fn(void *data)
{
    printk(KERN_INFO "Switch thread started on CPU %d\n", smp_processor_id());
    
    while (!atomic_read(&stop_test) && !kthread_should_stop()) {
        if (use_bh_lock == 2) {
            lockfree_switch_counter(test_urr);
        } else {
            locked_switch_counter(test_urr);
        }
        
        /* 模拟15秒报告间隔，但测试时缩短到1秒 */
        msleep(1000);
    }
    
    printk(KERN_INFO "Switch thread stopping\n");
    return 0;
}

/* proc文件显示统计信息 */
static int stats_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Lock Competition Test Statistics ===\n");
    seq_printf(m, "Test mode: %s\n", 
               use_bh_lock == 0 ? "spin_lock" :
               use_bh_lock == 1 ? "spin_lock_bh" : "lock_free");
    seq_printf(m, "Threads: %d\n", num_threads);
    seq_printf(m, "Work delay: %d us\n", work_delay_us);
    seq_printf(m, "Update count: %lld\n", atomic64_read(&test_urr->update_count));
    seq_printf(m, "Switch count: %lld\n", atomic64_read(&test_urr->switch_count));
    seq_printf(m, "Lock contentions: %lld\n", atomic64_read(&test_urr->lock_contention));
    
    if (use_bh_lock == 2) {
        seq_printf(m, "Counter[0]: total=%lld, uplink=%lld\n",
                   atomic64_read(&test_urr->counters[0][0]),
                   atomic64_read(&test_urr->counters[0][1]));
        seq_printf(m, "Counter[1]: total=%lld, uplink=%lld\n",
                   atomic64_read(&test_urr->counters[1][0]),
                   atomic64_read(&test_urr->counters[1][1]));
        seq_printf(m, "Current index: %d\n", atomic_read(&test_urr->current_idx));
    } else {
        seq_printf(m, "Vol1: total=%lld, uplink=%lld\n", test_urr->vol1_total, test_urr->vol1_uplink);
        seq_printf(m, "Vol2: total=%lld, uplink=%lld\n", test_urr->vol2_total, test_urr->vol2_uplink);
        seq_printf(m, "Current vol2: %s\n", test_urr->use_vol2 ? "true" : "false");
    }
    
    return 0;
}

static int stats_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, stats_proc_show, NULL);
}

static const struct proc_ops stats_proc_ops = {
    .proc_open = stats_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static struct proc_dir_entry *stats_proc_entry;

static int __init lockup_test_init(void)
{
    int i, ret = 0;
    
    printk(KERN_INFO "Loading lock competition test module\n");
    printk(KERN_INFO "Parameters: duration=%ds, threads=%d, lock_type=%s, delay=%dus\n",
           test_duration, num_threads,
           use_bh_lock == 0 ? "spin_lock" :
           use_bh_lock == 1 ? "spin_lock_bh" : "lock_free",
           work_delay_us);
    
    /* 分配测试结构 */
    test_urr = kzalloc(sizeof(*test_urr), GFP_KERNEL);
    if (!test_urr) {
        return -ENOMEM;
    }
    
    /* 初始化 */
    spin_lock_init(&test_urr->lock);
    atomic_set(&test_urr->current_idx, 0);
    atomic_set(&stop_test, 0);
    
    /* 创建proc文件 */
    stats_proc_entry = proc_create("lockup_test_stats", 0444, NULL, &stats_proc_ops);
    if (!stats_proc_entry) {
        printk(KERN_ERR "Failed to create proc entry\n");
        ret = -ENOMEM;
        goto err_free_urr;
    }
    
    /* 分配线程数组 */
    update_threads = kzalloc(sizeof(struct task_struct *) * num_threads, GFP_KERNEL);
    if (!update_threads) {
        ret = -ENOMEM;
        goto err_remove_proc;
    }
    
    /* 创建更新线程 */
    for (i = 0; i < num_threads; i++) {
        update_threads[i] = kthread_create(update_thread_fn, (void *)(long)i, "locktest_%d", i);
        if (IS_ERR(update_threads[i])) {
            ret = PTR_ERR(update_threads[i]);
            goto err_stop_threads;
        }
        
        /* 绑定到不同的CPU */
        kthread_bind(update_threads[i], i % num_online_cpus());
        wake_up_process(update_threads[i]);
    }
    
    /* 创建切换线程 */
    switch_thread = kthread_run(switch_thread_fn, NULL, "locktest_switch");
    if (IS_ERR(switch_thread)) {
        ret = PTR_ERR(switch_thread);
        goto err_stop_threads;
    }
    
    printk(KERN_INFO "Test started, will run for %d seconds\n", test_duration);
    
    return 0;
    
err_stop_threads:
    atomic_set(&stop_test, 1);
    for (i = 0; i < num_threads; i++) {
        if (update_threads[i] && !IS_ERR(update_threads[i])) {
            kthread_stop(update_threads[i]);
        }
    }
    kfree(update_threads);
    
err_remove_proc:
    proc_remove(stats_proc_entry);
    
err_free_urr:
    kfree(test_urr);
    
    return ret;
}



static void __exit lockup_test_exit(void)
{
    int i;
    
    printk(KERN_INFO "Unloading lock competition test module\n");
    
    /* 停止测试 */
    atomic_set(&stop_test, 1);
    
    /* 停止所有线程 */
    if (switch_thread) {
        kthread_stop(switch_thread);
    }
    
    for (i = 0; i < num_threads; i++) {
        if (update_threads[i]) {
            kthread_stop(update_threads[i]);
        }
    }
    
    /* 清理资源 */
    kfree(update_threads);
    proc_remove(stats_proc_entry);
    kfree(test_urr);
    
    printk(KERN_INFO "Module unloaded\n");
}

module_init(lockup_test_init);
module_exit(lockup_test_exit);
