# CFS Scheduler

## 1.  概览

CFS 意为 “完全公平调度器”，是一种新的“桌面”进程调度器，由 Ingo Molnar 实现，在 linux 2.6.23 中并入内核。
它是对之前的 vanilla 调度器的分时调度策略的替代。

80% 的 CFS 设计可以被总结为一句话：CFS 从根本上模仿一种在真实硬件上的“理想的精确的多任务 CPU ”。

“理想多任务 CPU ”是一种拥有 100% 物理能力的（不存在的 :-) ）的 CPU ，在它上面运行的每个任务都有着精确相同的速度，并且是并行的。
例如：如果这个 CPU 上运行着 2 个任务，那么每个任务都使用 50% 的物理能力等...确实并行地运行。

在真实的硬件上，我们仅仅只能一次运行一个任务，所以我们需要介绍下“虚拟运行时”的概念。
在上述的理想多任务 CPU 上，一个任务的虚拟运行时说明了该任务的下一个时间片将要开始执行的时间。
在实践中，一个任务的虚拟运行时等于它的真实运行时均分到所有的正在运行的任务。

## 2. 少数实现细节 

在 CFS 中虚拟运行时由每个任务的 `p->se.vruntime`（单位：纳秒）表述和记录。
这样，它就有可能精确的标记和衡量一个任务应该要得到的“期望 CPU 时间”。

[ 小细节：在“理想”硬件上，在任何时间所有任务都应当拥有相同的 `p->se.vruntime` 值－－例如，所有任务都同时地执行并且没有任务会从 CPU 时间的理想分享中得到“超出均衡”这一信息。]

CFS 的任务挑选逻辑是基于 `p->se.vruntime`值的，因此这个逻辑很简单：它总是尝试运行具有最小的 `p->se.vruntime` 的任务（例如：到目前为止最少执行的任务）。
CFS 总是试着在可运行任务间分割 CPU 时间，使之尽可能接近“理想多任务硬件”。

其余的 CFS 设计大部分仅仅脱离这个非常简单的概念，加入了一些附加的修饰如优先级，多进程和用于识别休眠任务的不同算法变体。

## 3. 红黑树 

CFS 在设计上是非常彻底的：它不在运行队列中使用旧的结构，而是使用一个时序的红黑树来构建一个未来任务执行的时间线，因此没有“队列切换”这种原始结构（一个影响了之前的 vanilla 调度器和 RSDL/SD 的东西）。

CFS 也维护着 `rq->cfs.min_vruntime` 值，这个值是单调递增的，追踪了在运行队列中的所有任务中最小的 vruntime 值。
我们用 `min_vruntime` 追踪系统完成的工作总量；这个值用于尽可能把新的活跃实体放置在这棵树的左侧。

我们用 `rq->cfs.load` 计算运行队列中所有正在运行的任务总数，这个值等于队列中所有任务的权值之和。

CFS 维护着一棵时序的红黑树，即所有可运行的任务按照 `p->se.vruntime` 排序。
CFS 从这棵树挑选最左侧的任务并附着在这个任务上。
随着系统的运行，执行过的任务被扔到这棵树里，更多地移动到树的右侧－－慢慢并肯定地给予了每个任务成为“最左侧任务”的机会，因此能在一个确定的时间内使用 CPU 。

综上所述，CFS 这样工作：它运行一个任务一小会儿，并且当这个任务调度（或者一个调度器嘀嗒发生）时，它的 CPU 使用量通过这样计算：
把它刚刚使用物理 CPU 的（小的）时间加到 `p->se.vruntime` 上。
当 `p->se.vruntime` 变得足够高，以至于另外的任务成为由这个值维护的时序红黑树的“最左端”任务，（加上一个小的与最左侧任务相关的一个“间隔”距离量，因此我们不至于过度调度任务和损坏缓存)，然后最新的最左侧任务被选中，接着取代当前的任务。

## 4. CFS 的一些特性 

CFS 使用纳秒粒度进行计算，并且不依赖任何时钟周期和其他赫兹细节。
因此，CFS 调度器没有任何之前调度器所有的“时间片”概念，并且没有任何的启发式算法。
这里仅有一个可调的中心（你必须打开 `CONFIG_SCHED_DEBUG`）：

```
   /proc/sys/kernel/sched_min_granularity_ns
```

这个可以用于把调度器从“桌面级”（例如低延迟）负荷调整到“服务器级”（例如良好的批处理）负荷。
`SCHED_BATCH` 也是由CFS调度器模块处理的。

由于CFS调度器的设计，它不会使任何现在用于对付旧的调度器的启发式策略的“攻击”变得容易：`fiftyp.c`，`thud.c`，`chew.c`，`ring-test.c`，`massive_intr.c` 都能很好地工作，没有交互上的冲击，能出现预期的行为。

CFS 调度器有着比之前 vanilla 调度器更强壮的对优先级和 `SCHED_BATCH` 的处理能力：两种负荷都是更加激烈地独立的。

SMP load-balancing has been reworked/sanitized: the runqueue-walking
assumptions are gone from the load-balancing code now, and iterators of the
scheduling modules are used.  The balancing code got quite a bit simpler as a
result.



## 5. 调度策略

CFS 实现了三种调度策略：

    - `SCHED_NORMAL` （传统上被称为 `SCHED_OTHER` ）：这个调度策略用于普通任务。

    - `SCHED_BATCH` ：没有像普通任务那样经常的抢占，由此可以让任务运行更长，能够更好地使用缓存，但会损失交互能力。这能很好地适应批处理任务。

    - `SCHED_IDLE` ：这个比优先级 19 更弱，但是为了避免陷入产生死锁的优先级逆序问题，这个并不是一个真正理想的时间调度器。

`SCHED_FIFO/_RR` are implemented in sched/rt.c and are as specified by
POSIX.

The command chrt from util-linux-ng 2.13.1.1 can set all of these except
SCHED_IDLE.



## 6. 调度类 

The new CFS scheduler has been designed in such a way to introduce "Scheduling
Classes," an extensible hierarchy of scheduler modules.  These modules
encapsulate scheduling policy details and are handled by the scheduler core
without the core code assuming too much about them.

sched/fair.c implements the CFS scheduler described above.

sched/rt.c implements SCHED_FIFO and SCHED_RR semantics, in a simpler way than
the previous vanilla scheduler did.  It uses 100 runqueues (for all 100 RT
priority levels, instead of 140 in the previous scheduler) and it needs no
expired array.

Scheduling classes are implemented through the sched_class structure, which
contains hooks to functions that must be called whenever an interesting event
occurs.

This is the (partial) list of the hooks:

 - enqueue_task(...)

   Called when a task enters a runnable state.
   It puts the scheduling entity (task) into the red-black tree and
   increments the nr_running variable.

 - dequeue_task(...)

   When a task is no longer runnable, this function is called to keep the
   corresponding scheduling entity out of the red-black tree.  It decrements
   the nr_running variable.

 - yield_task(...)

   This function is basically just a dequeue followed by an enqueue, unless the
   compat_yield sysctl is turned on; in that case, it places the scheduling
   entity at the right-most end of the red-black tree.

 - check_preempt_curr(...)

   This function checks if a task that entered the runnable state should
   preempt the currently running task.

 - pick_next_task(...)

   This function chooses the most appropriate task eligible to run next.

 - set_curr_task(...)

   This function is called when a task changes its scheduling class or changes
   its task group.

 - task_tick(...)

   This function is mostly called from time tick functions; it might lead to
   process switch.  This drives the running preemption.




## 7.  CFS 的组调度器扩展

Normally, the scheduler operates on individual tasks and strives to provide
fair CPU time to each task.  Sometimes, it may be desirable to group tasks and
provide fair CPU time to each such task group.  For example, it may be
desirable to first provide fair CPU time to each user on the system and then to
each task belonging to a user.

CONFIG_CGROUP_SCHED strives to achieve exactly that.  It lets tasks to be
grouped and divides CPU time fairly among such groups.

CONFIG_RT_GROUP_SCHED permits to group real-time (i.e., SCHED_FIFO and
SCHED_RR) tasks.

CONFIG_FAIR_GROUP_SCHED permits to group CFS (i.e., SCHED_NORMAL and
SCHED_BATCH) tasks.

   These options need CONFIG_CGROUPS to be defined, and let the administrator
   create arbitrary groups of tasks, using the "cgroup" pseudo filesystem.  See
   Documentation/cgroups/cgroups.txt for more information about this filesystem.

When CONFIG_FAIR_GROUP_SCHED is defined, a "cpu.shares" file is created for each
group created using the pseudo filesystem.  See example steps below to create
task groups and modify their CPU share using the "cgroups" pseudo filesystem.

	# mount -t tmpfs cgroup_root /sys/fs/cgroup
	# mkdir /sys/fs/cgroup/cpu
	# mount -t cgroup -ocpu none /sys/fs/cgroup/cpu
	# cd /sys/fs/cgroup/cpu

	# mkdir multimedia	# create "multimedia" group of tasks
	# mkdir browser		# create "browser" group of tasks

	# #Configure the multimedia group to receive twice the CPU bandwidth
	# #that of browser group

	# echo 2048 > multimedia/cpu.shares
	# echo 1024 > browser/cpu.shares

	# firefox &	# Launch firefox and move it to "browser" group
	# echo <firefox_pid> > browser/tasks

	# #Launch gmplayer (or your favourite movie player)
	# echo <movie_player_pid> > multimedia/tasks
