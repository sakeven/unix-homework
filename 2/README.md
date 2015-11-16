## unix 系统分析第二题


Linux 内核对于普通进程采用 CFS 算法进行调度。CFS，即 Completely Fair Scheduling，完全公平调度。

CFS 的具体实现在 kernel/sched/fair.c 中，本文仅对 Linux 3.18 kernel/sched/sched.h 中 `cfs_rq` 数据结构进行分析。 


```c
/* 运行队列中 CFS 相关的域 */
/* CFS-related fields in a runqueue */
struct cfs_rq {
    /* CFS 运行队列中所有进程的总负载 */
    struct load_weight load;
    /*
     * nr_running: 调度实体数量
     * h_nr_running: 运行队列下所有进程组的 cfs_rq 的 nr_running 之和
     */
   unsigned int nr_running, h_nr_running;

    
    u64 exec_clock;
    /* 当前CFS队列上最小运行时间，单调递增
     * 两种情况下更新该值: 
     * 1、更新当前运行任务的累计运行时间时
     * 2、当任务从队列删除去，如任务睡眠或退出，这时候会查看剩下的任务的vruntime是否大于min_vruntime，如果是则更新该值。
     */
    u64 min_vruntime;
#ifndef CONFIG_64BIT
    u64 min_vruntime_copy;
#endif

    /* 红黑树的 root 节点 */
    struct rb_root tasks_timeline;
    /* 红黑树最左节点，即下个调度实体 */
    struct rb_node *rb_leftmost;

    /*
     * 'curr' points to currently running entity on this cfs_rq.
     * It is set to NULL otherwise (i.e when none are currently running).
     */
    /*
     * 'curr' 指向现在在这个 cfs_rq 运行的实体。
     * 否则设为 NULL（例如：当没有正在运行的）。
     * next: 表示有些进程急需运行，即使不遵从 CFS 调度也必须运行它，调度时会检查 next 是否需要调度，需要就调度 next。
     */
    struct sched_entity *curr, *next, *last, *skip;

#ifdef  CONFIG_SCHED_DEBUG
    unsigned int nr_spread_over;
#endif

#ifdef CONFIG_SMP
    /*
     * CFS Load tracking
     * Under CFS, load is tracked on a per-entity basis and aggregated up.
     * This allows for the description of both thread and group usage (in
     * the FAIR_GROUP_SCHED case).
     */
    /* CFS 的负载 */
    unsigned long runnable_load_avg, blocked_load_avg;
    atomic64_t decay_counter;
    u64 last_decay;
    atomic_long_t removed_load;

#ifdef CONFIG_FAIR_GROUP_SCHED
    /* Required to track per-cpu representation of a task_group */
    u32 tg_runnable_contrib;
    unsigned long tg_load_contrib;

    /*
     *   h_load = weight * f(tg)
     *
     * Where f(tg) is the recursive weight fraction assigned to
     * this group.
     */
    unsigned long h_load;
    u64 last_h_load_update;
    struct sched_entity *h_load_next;
#endif /* CONFIG_FAIR_GROUP_SCHED */
#endif /* CONFIG_SMP */

#ifdef CONFIG_FAIR_GROUP_SCHED
    /* cfs_rq 相关的 CPU 运行队列 */
    struct rq *rq;  /* cpu runqueue to which this cfs_rq is attached */

    /*
     * leaf cfs_rqs are those that hold tasks (lowest schedulable entity in
     * a hierarchy). Non-leaf lrqs hold other higher schedulable entities
     * (like users, containers etc.)
     *
     * leaf_cfs_rq_list ties together list of leaf cfs_rq's in a cpu. This
     * list is used during load balance.
     */
    int on_list;
    struct list_head leaf_cfs_rq_list;
    struct task_group *tg;  /* group that "owns" this runqueue */

#ifdef CONFIG_CFS_BANDWIDTH
    int runtime_enabled;
    u64 runtime_expires;
    s64 runtime_remaining;

    u64 throttled_clock, throttled_clock_task;
    u64 throttled_clock_task_time;
    int throttled, throttle_count;
    struct list_head throttled_list;
#endif /* CONFIG_CFS_BANDWIDTH */
#endif /* CONFIG_FAIR_GROUP_SCHED */
};
```
