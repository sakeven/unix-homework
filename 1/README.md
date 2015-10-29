## unix 系统分析第一题

使用 strace 分析 code.c 中的系统调用，结果存放在 strace.out 中。

我们主要分析结果中的这些输出：

```
2139  write(1, "Hello world, this is Linux!>", 28) = 28  # write 系统调用，输出 "Hello world, this is Linux!>"，对应代码 10 和 13 行
2139  read(0, "date\n", 1024)           = 5             # read 系统调用，输入 "date\n"，对应代码 14 行
2139  clone(child_stack=0, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f6392412a10) = 2140 # clone 系统调用，fork 产生一个子进程，对应代码 17 行
2139  wait4(-1,  <unfinished ...>)                      # wait4 系统调用，等待子进程结束, 对应代码 22 行
2140  execve("/bin/date", ["date"], [/* 32 vars */]) = 0 # execve 系统调用，执行 "/bin/date" 字符串对应的文件路径，对应代码 18 行
.
.
.
2139  write(1, "child process return 0 \n", 24) = 24 # write 系统调用，输出 "child process return 0\n", 对应代码 23 行
2139  write(1, ">", 1)                  = 1         # write 系统调用，输出 ">", 对应代码 13 行
```

所以主要的系统调用有：write、read、clone、wait4、execve 等。
