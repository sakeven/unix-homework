#                      CFS Scheduler

## 1.  概览

CFS 意为 “完全公平调度器”，是一种新的“桌面”进程调度器，由 Ingo Molnar 实现，在 linux 2.6.23 中并入内核。
它是对之前的 vanilla 调度器的分时调度策略的替代。

80% 的 CFS 设计可以被总结为一句话：CFS 从根本上模仿一种在真实硬件上的“理想的精确的多任务 CPU ”。

“理想多任务 CPU ”是一种拥有 100% 物理能力的（不存在的 :-) ）的 CPU，在它上面运行的每个任务都有着精确相同的速度，并且是并行的。
例如：如果这个 CPU 上运行着 2 个任务，那么每个任务都使用 50% 的物理能力等...确实并行地运行。

在真实的硬件上，我们仅仅只能一次运行一个任务，所以我们需要介绍下“虚拟运行时”的概念。
在上述的理想多任务CPU上，一个任务的虚拟运行时说明了该任务的下一个时间片将要开始执行的时间。
在实践中，一个任务的虚拟运行时等于它的真实运行时均分到所有的正在运行的任务。

## 2. 少数实现细节 

在 CFS 中虚拟运行时由每个任务的 `p->se.vruntime`（单位：纳秒）表述和记录。
这样，它就有可能精确的标记和衡量一个任务应该要得到的“期望 CPU 时间”。

[ 小细节：在“理想”硬件上，在任何时间所有任务都应当拥有相同的 `p->se.vruntime` 值－－例如，所有任务都同时地执行并且没有任务会从 CPU 时间的理想分享中得到“超出均衡”这一信息。]

CFS 的任务挑选逻辑是基于 `p->se.vruntime`值的，因此这个逻辑很简单：它总是尝试运行具有最小的 `p->se.vruntime` 的任务（例如：到目前为止最少执行的任务）。
CFS 总是试着在可运行任务间分割 CPU 时间，使之尽可能接近“理想多任务硬件”。

其余的 CFS 设计大部分仅仅脱离这个非常简单的概念，加入了一些附加的修饰如优先级，多进程和用于识别休眠任务的不同算法变体。

## 3. 红黑树 

CFS's design is quite radical: it does not use the old data structures for the
runqueues, but it uses a time-ordered rbtree to build a "timeline" of future
task execution, and thus has no "array switch" artifacts (by which both the
previous vanilla scheduler and RSDL/SD are affected).

CFS also maintains the rq->cfs.min_vruntime value, which is a monotonic
increasing value tracking the smallest vruntime among all tasks in the
runqueue.  The total amount of work done by the system is tracked using
min_vruntime; that value is used to place newly activated entities on the left
side of the tree as much as possible.

The total number of running tasks in the runqueue is accounted through the
rq->cfs.load value, which is the sum of the weights of the tasks queued on the
runqueue.

CFS maintains a time-ordered rbtree, where all runnable tasks are sorted by the
p->se.vruntime key. CFS picks the "leftmost" task from this tree and sticks to it.
As the system progresses forwards, the executed tasks are put into the tree
more and more to the right --- slowly but surely giving a chance for every task
to become the "leftmost task" and thus get on the CPU within a deterministic
amount of time.

Summing up, CFS works like this: it runs a task a bit, and when the task
schedules (or a scheduler tick happens) the task's CPU usage is "accounted
for": the (small) time it just spent using the physical CPU is added to
p->se.vruntime.  Once p->se.vruntime gets high enough so that another task
becomes the "leftmost task" of the time-ordered rbtree it maintains (plus a
small amount of "granularity" distance relative to the leftmost task so that we
do not over-schedule tasks and trash the cache), then the new leftmost task is
picked and the current task is preempted.



## 4. CFS 的一些特性 

CFS uses nanosecond granularity accounting and does not rely on any jiffies or
other HZ detail.  Thus the CFS scheduler has no notion of "timeslices" in the
way the previous scheduler had, and has no heuristics whatsoever.  There is
only one central tunable (you have to switch on CONFIG_SCHED_DEBUG):

   /proc/sys/kernel/sched_min_granularity_ns

which can be used to tune the scheduler from "desktop" (i.e., low latencies) to
"server" (i.e., good batching) workloads.  It defaults to a setting suitable
for desktop workloads.  SCHED_BATCH is handled by the CFS scheduler module too.

Due to its design, the CFS scheduler is not prone to any of the "attacks" that
exist today against the heuristics of the stock scheduler: fiftyp.c, thud.c,
chew.c, ring-test.c, massive_intr.c all work fine and do not impact
interactivity and produce the expected behavior.

The CFS scheduler has a much stronger handling of nice levels and SCHED_BATCH
than the previous vanilla scheduler: both types of workloads are isolated much
more aggressively.

SMP load-balancing has been reworked/sanitized: the runqueue-walking
assumptions are gone from the load-balancing code now, and iterators of the
scheduling modules are used.  The balancing code got quite a bit simpler as a
result.



## 5. 调度策略

CFS implements three scheduling policies:

  - SCHED_NORMAL (traditionally called SCHED_OTHER): The scheduling
    policy that is used for regular tasks.

  - SCHED_BATCH: Does not preempt nearly as often as regular tasks
    would, thereby allowing tasks to run longer and make better use of
    caches but at the cost of interactivity. This is well suited for
    batch jobs.

  - SCHED_IDLE: This is even weaker than nice 19, but its not a true
    idle timer scheduler in order to avoid to get into priority
    inversion problems which would deadlock the machine.

SCHED_FIFO/_RR are implemented in sched/rt.c and are as specified by
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
