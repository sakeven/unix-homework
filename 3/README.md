# 字符设备驱动分析

## 0. 模型

![字符设备驱动模型](/3/cdev.jpg)

## 1. 驱动初始化


### 1.1. 分配 cdev 
Linux 使用 cdev 结构体来描述字符设备，在驱动中分配 cdev ,主要是分配一个 cdev 结构体与申请设备号。

在 Linux 中以主设备号用来标识与设备文件相连的驱动程序。次编号被驱动程序用来辨别操作的是哪个设备。 cdev 结构体的 `dev_t` 成员定义了设备号，为 32 位，其中高 12 位为主设备号，低20 位为次设备号。

获得主设备号：MAJOR(dev_t dev);
获得次设备号：MINOR(dev_t dev);
生成：MKDEV(int major,int minor);

设备号申请的动静之分：

静态：
```c
/*申请使用从from开始的count 个设备号(主设备号不变，次设备号增加）*/
int register_chrdev_region(dev_t from, unsigned count, const char *name)；
```
静态申请相对较简单，但是一旦驱动被广泛使用,这个随机选定的主设备号可能会导致设备号冲突，而使驱动程序无法注册。

动态：
```c
/*请求内核动态分配count个设备号，且次设备号从baseminor开始。*/
int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,const char *name)；
```
动态申请简单，易于驱动推广，但是无法在安装驱动前创建设备文件（因为安装前还没有分配到主设备号）。


### 1.2. 初始化 cdev 
```c
void cdev _init(struct cdev *, struct file_operations *);
```
初始化 cdev 的成员，并建立 cdev 和 file_operations 之间的连接。


### 1.3. 注册 cdev 
```c
int cdev _add(struct cdev *, dev_t, unsigned);
```
向系统添加一个 cdev ，完成字符设备的注册。


### 1.4. 硬件初始化

硬件初始化主要是硬件资源的申请与配置。


## 2. 实现设备操作

用户空间的程序以访问文件的形式访问字符设备，通常进行open、read、write、close等系统调用。而这些系统调用的最终落实则是file_operations结构体中成员函数，它们是字符设备驱动与内核的接口。
```c
struct file_operations {

/*拥有该结构的模块计数，一般为THIS_MODULE*/
struct module *owner;

/*用于修改文件当前的读写位置*/
 loff_t (*llseek) (struct file *, loff_t, int);

 /*从设备中同步读取数据*/
ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);

/*向设备中写数据*/
ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);


ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
int (*readdir) (struct file *, void *, filldir_t);

/*轮询函数，判断目前是否可以进行非阻塞的读取或写入*/
unsigned int (*poll) (struct file *, struct poll_table_struct *);

/*执行设备的I/O命令*/
int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);

long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
long (*compat_ioctl) (struct file *, unsigned int, unsigned long);

/*用于请求将设备内存映射到进程地址空间*/
int (*mmap) (struct file *, struct vm_area_struct *);

/*打开设备文件*/
int (*open) (struct inode *, struct file *);
int (*flush) (struct file *, fl_owner_t id);

/*关闭设备文件*/
int (*release) (struct inode *, struct file *);

int (*fsync) (struct file *, struct dentry *, int datasync);
int (*aio_fsync) (struct kiocb *, int datasync);
int (*fasync) (int, struct file *, int);
int (*lock) (struct file *, int, struct file_lock *);
ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
int (*check_flags)(int);
int (*flock) (struct file *, int, struct file_lock *);
ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
int (*setlease)(struct file *, long, struct file_lock **);
};
```

## 3. 驱动注销

### 3.1. 删除 cdev 
在字符设备驱动模块卸载函数中通过 `cdev_del()` 函数向系统删除一个 cdev ，完成字符设备的注销。
```c
void cdev _del(struct cdev *);
```

### 3.2. 释放设备号
在调用 `cdev_del()` 函数从系统注销字符设备之后，`unregister_chrdev_region()` 应该被调用以释放原先申请的设备号。
```c
void unregister_chrdev_region(dev_t from, unsigned count);
```

## 4. 字符设备驱动程序基础:


## 4.1 cdev 结构体
```c
struct cdev {

struct kobject kobj;

struct module *owner; /*通常为THIS_MODULE*/

struct file_operations *ops; /*在 cdev _init()这个函数里面与 cdev 结构联系起来*/

struct list_head list;

dev_t dev; /*设备号*/

unsigned int count;

}；
```
