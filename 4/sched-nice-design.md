这份文档解释了有关了在新的 Linux 调度器中更新和改善的优先级实现思想。

优先级曾经在 Linux 上一直非常薄弱，人们一直地纠缠我们，让我们实现优先级 +19 的任务消耗更少的 CPU 时间。

不幸地，这在旧的调度器上并不容易实现，（否则很早以前我们就完成了）因为优先级支持在历史上是与时间片长度耦合的，并且时间片单位是由 HZ 嘀嗒驱动的，所以最小的时间片曾经是 1/HZ 。

在 O(1) 调度器（在 2003 年）我们改变了负优先级使之比之前在 2.4 中更健壮（并且大家都乐于这个改变），并且我们有意地标准化线性时间片规则，因此 +19 优先级将真正是 1 时钟周期。
为了更好的理解这个，时间片图像这样（丑陋的 ASCII 艺术预警！）：

```
                   A
             \     | [timeslice length]
              \    |
               \   |
                \  |
                 \ |
                  \|___100msecs
                   |^ . _
                   |      ^ . _
                   |            ^ . _
 -*----------------------------------*-----> [nice level]
 -20               |                +19
                   |
                   |
```

所以如果有人真地想重新安排任务的优先级，+19 将会给它一个比普通线性规则所给的还大很多的标记。
（改变 ABI 来扩展优先级的解决方案很早被否决了。）

这个方法有时候有点用，但是之后在 HZ=1000 的时候，它引起 1 时钟周期变成 1 毫秒，这意味着 我们感受到的 0.1% CPU 使用量有点过多。
过多不是因为 CPU 利用率太低，而是因为它引起了太频繁的（一次一毫秒的）重新调记住。
（并且例如，这会引起损坏缓存。记住，这是很久以前，硬件非常落后，缓存非常小，并且人们运行数学运算在优先级 +19。）

所以对于 HZ=1000 我们把优先级 +19 提升到 5 毫米，因为这让人感觉是正确的最小尺度－并且这转化到 5% 的 CPU 利用率。
但是优先级 +19 的根本的 HZ 敏感的属性继续保留着，并且我们从未得到一个关于优先级 +19 在 CPU 利用率上太差的单独的抱怨，我们只得到关于这个（仍然）太强大的怨念:-)

综上所述：我们一直希望让优先级更一致的，但是在 HZ 和时钟周期，及它们令人讨厌的与时间片和粒度耦合的设计等级约束下，这不是真正切实可行的。

第二个（ 低频但是仍然周期性地发生）关于 Linux 的优先级支持的抱怨是它围绕（你可以看到上图的证据）起点的不对称，或者更如实地：优先级行为依据绝对的优先级的事实也是如此，然而优先级 API 是基本上“相关的”：

```c
int nice(int inc);

asmlinkage long sys_nice(int increment)
```

（第一个是 glibc API，第二个是 syscall API。）
注意 `inc` 是与此时的优先级有关的。像 bash 的 `nice` 命令这种工具正是借鉴这个相关 API。

With the old scheduler, if you for example started a niced task with +1
and another task with +2, the CPU split between the two tasks would
depend on the nice level of the parent shell - if it was at nice -10 the
CPU split was different than if it was at +5 or +10.

A third complaint against Linux's nice level support was that negative
nice levels were not 'punchy enough', so lots of people had to resort to
run audio (and other multimedia) apps under RT priorities such as
SCHED_FIFO. But this caused other problems: SCHED_FIFO is not starvation
proof, and a buggy SCHED_FIFO app can also lock up the system for good.

The new scheduler in v2.6.23 addresses all three types of complaints:

To address the first complaint (of nice levels being not "punchy"
enough), the scheduler was decoupled from 'time slice' and HZ concepts
(and granularity was made a separate concept from nice levels) and thus
it was possible to implement better and more consistent nice +19
support: with the new scheduler nice +19 tasks get a HZ-independent
1.5%, instead of the variable 3%-5%-9% range they got in the old
scheduler.

To address the second complaint (of nice levels not being consistent),
the new scheduler makes nice(1) have the same CPU utilization effect on
tasks, regardless of their absolute nice levels. So on the new
scheduler, running a nice +10 and a nice 11 task has the same CPU
utilization "split" between them as running a nice -5 and a nice -4
task. (one will get 55% of the CPU, the other 45%.) That is why nice
levels were changed to be "multiplicative" (or exponential) - that way
it does not matter which nice level you start out from, the 'relative
result' will always be the same.

The third complaint (of negative nice levels not being "punchy" enough
and forcing audio apps to run under the more dangerous SCHED_FIFO
scheduling policy) is addressed by the new scheduler almost
automatically: stronger negative nice levels are an automatic
side-effect of the recalibrated dynamic range of nice levels.
