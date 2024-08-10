# My implementation for ICS2023-pa

项目说明以及概要请参照`2023`分支. (For more information, please refer to `branch 2023`)

本项目由我个人独立完成, 如果正在写PA的NJU的同学恰好看到这个仓库, 请注意学术诚信(Do your homework by yourself, TOO).
-- --

## What is it?

### `nemu`

1. 实现了`nemu`, 这是一个`emulator`. 我选择了`riscv32`作为`ISA`.
`nemu`(类似于`qemu`)模拟`cpu`取指, 译码, 执行的过程.

2. 其中包含一个简单的`monitor`, `monitor`中实现`debug`与`trace`系统. 支持简单的表达式求值, 监视点等功能. 

- `ITRACE`: 对指令调用的跟踪, 采用环形缓冲区
- `FTRACE`: 对函数调用的跟踪. 这里实现一个简单的`elf_parser`解析`elf`文件, 读取符号表中的函数信息
- `MTRACE`: 对指令访存的跟踪.
- `DTRACE`: 访问外设的跟踪.
- `ETRACE`: 对异常的跟踪.
- `DEBUG`: 实现单步执行, 监视点, 表达式求值等. 同时表达式求值部分进行了充分测试.

3. 支持`IO`类型: 
* 5 devices
  * serial, timer, keyboard, VGA, audio
  * most of them are simplified and unprogrammable
* 2 types of I/O
  * port-mapped I/O and memory-mapped I/O

### `Abstract Machine`

实现了`abstract-machine`
对于完整的`computer`而言, 只有`cpu`是不够的. 因此`AM`中包含了程序运行需要的`runtime`(在这里也就是`klib`).
同时还需要包含与外界的`IOE(Input Output Extension`). 这里通过静态链接, 把交叉编译的程序链接到
`klib`. 同时在`AM`中提供内部程序与键盘, VGA等固件交互的接口.

- `klib`: 简单的标准库, 包括`printf`系列以及`string`系列的常用函数.
- `IOE`: 实现对`nemu`的扩展, 与外设的交互. 通过抽象寄存器实现, 将接口抽象为寄存器, 通过`io_read/write`进行读写. 
- `CTE`: 实现`CTE`(Context Transfer Extension). 设置中断向量, 实现上下文的保存和恢复. `trap`通过汇编实现, 保存上下文, 调用`__am_irq_handler`根据约定寄存器的值识别异常类型, 最后恢复上下文. 

-- --


### `navy`
提供了`nanos-lite`所需要的`runtime`.
我在其中完善了:
- `libndl`: 对系统调用进行封装, 实现`VGA`现存以及时钟的交互
- `libminiSDL`: 仿照`SDL`库, 在`NDL`上进一步封装, 实现时钟, 事件等待(阻塞与非阻塞), 画布相关操作(画布填充, 画布复制)等`api`.
- `libos`: 这部分主要是为操作系统的`syscall`提供进一步的封装. 比如
```c
int _open(const char *path, int flags, mode_t mode) {
  return _syscall_(SYS_open, (intptr_t)path, flags, mode);
}
```
系统调用的实现在`nanos-lite`中实现, 在`am`中实现`cte_init`传入`do_syscall`这一回调函数, 具体的处理通过`do_syscall`这一回调函数进行处理. 封装了输入输出, 时钟, 文件系统, 堆栈管理的相关接口. 



### `nanos-lite`
一个非常简易的操作系统. 
- `elf loader`: 负责解析`elf`文件, 将文件加载进入内存, 并且跳入程序执行入口.
- `STRACE`: 实现对系统调用的跟踪.
- 简单的文件系统: 文件大小固定, 文件数量固定. 实现虚拟文件系统(一切皆文件的思想), 将键盘输入, 标准输出, `VGA`显存等与外设交互的接口抽象为文件的读写.
- 系统调用的实现: 这部分与`am`, `libos`紧密联系, 实现的是`do_syscall`这一回调函数. 支持如下系统调用:
```c
void do_syscall(Context *c) {
  uintptr_t a[4] = {c->GPR1, c->GPR2, c->GPR3, c->GPR4};

#ifdef CONFIG_STRACE
  strace(a[0], a[1], a[2], a[3], c->mepc);
#endif

  switch (a[0]) {
  case SYS_yield:
    sys_yield();
    break;
  case SYS_exit:
    sys_exit(a[1]);
    break;
  case SYS_write:
    c->GPRx = (uintptr_t)sys_write(a[1], (void const *)a[2], a[3]);
    break;
  case SYS_brk:
    c->GPRx = (uintptr_t)sys_brk(a[1]);
    break;
  case SYS_read:
    c->GPRx = (uintptr_t)sys_read(a[1], (void *)a[2], a[3]);
    break;
  case SYS_open:
    c->GPRx = (uintptr_t)sys_open((char const *)a[1], a[2], a[3]);
    break;
  case SYS_close:
    c->GPRx = (uintptr_t)sys_close(a[1]);
    break;
  case SYS_lseek:
    c->GPRx = (uintptr_t)sys_lseek(a[1], a[2], a[3]);
    break;
  case SYS_gettimeofday:
    c->GPRx = (uintptr_t)sys_gettimeofday((struct timeval *)a[1],
                                          (struct timezone *)a[2]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}
```
 -- -- 
## Summary

### 抽象层次
`nemu`平台支持裸指令的运行, `abstract machine`提供与硬件交互的抽象`api`,`klib`中的简单库为裸机上的程序提供简单的`runtime`. 

当在`nemu`中运行`nanos-lite`时, `am`与`klib`为`OS`提供与硬件交互的接口, `OS`取裸机上的一段内存作为`ramdisk`.

`navy`中的`libc`(来自`Newlib`)提供了操作系统中运行程序的`runtime`, `navy`中的`libos`为操作系统提供系统调用的接口, 作为`libc`的底层支持. 在运行复杂程序时, 使用`navy`中的`libminiSDL`等库提供更复杂的`IO`交互. 

### 收获
实际上这次的`PA`让我对`OS`与体系结构有了更加深入的思考. 把指令集, 汇编, 以及抽象机相关的知识串联. 为什么`trap.S`需要使用汇编而不是纯`C`, 为什么恢复上下文需要使用`asm`而不是`C`, 为什么需要`klib`, 又为什么需要`libos`, 以及为什么需要中断, 为什么需要`syscall`.

代码能力和对体系结构的理解都有深化. 如何利用日志, 以及`assert`提高编码质量, 方便调试, 提早定位错误. 

### <s>一些遗憾</s>
在`PA3`的最后, 之前为了保证正确性一直打开了`difftest`. 可是当关闭的时候, 指令执行确出现了错误, 这是一个`nemu`层面的`bug`, 但仍然没有定位出来. 根据指令执行日志定位到了一个函数调用. 但原因尚未查明. 不过也刚好`PA`的主要内容也都认真走了一遍流程. 在这个`bug`暴露出来的时候, 主要影响就是无法关闭`difftest`, 对性能造成了损失. 

<s>实际上这样也是一个教训, 编码需要更多的`assert`, 以及不同环境的测试. `difftest`带有的版本相当于一个`DEBUG`版本, 关闭`difftest`相当于`RELEASE`. 实际上我们需要更充分的测试, 关闭所有调试信息, 以及打开所有调试信息分别测试. 因为很可能在哪里, 就写出来一个`UB`. 而在`-O2`甚至`-O3`优化之下, 这样的问题就可能暴露出来. </s>

在起始阶段需要更充分的测试, 或者至少需要通过`Assert`把问题尽可能早的暴露出来. 

以上是之前的想法, 当我换用`gcc`和`clang`均发现同样的错误的时候, 我实际上不应该怀疑`UB`. 因为不同编译器对于`UB`的处理大概率不一样. 我从寄存器角度排查, 发现使用了没有初始化的内存. 当打开`difftest`的时候内存是默认全部初始化为`0`的. 但是用脚趾头想一想都知道真实的计算机不可能上来内存全部初始化为`0`. 那么问题出现在哪里? 

正是`ELF Loader`, 程序中没有清零, 而是由`OS`清零的`.bss`段. 我采用的申请`buffer`清零,在把清零过得`buffer`拷贝过去的方法. 但是拷贝的时候拷贝的大小拷贝错了, 导致需要清零的一段没有清零. 

测试是极为重要的, `ELF Loader`完成后不能只看跑不跑, 而要检查对应的区间是否设置为`0`. 而且在`difftest`的影响下的确是`0`, 如果我真的去检查还真不一定检查出来. 但是如果我在这一段设置一部分`assert`, 这样就能在我关闭`difftest`的时候发现. 这也启示着要在不同环境进行测试, 并且使用`assert`使错误尽可能早的暴露出来. 

这正应验了, <mark>机器永远是对的, 没有调试不出来的错误</mark>

 -- -- 
## Toolchain && Environment

使用正确的工具

- `gcc`: `gcc (GCC) 14.1.1 20240720`
- `clang`: `clang version 18.1.8`
- `llvm toolchain`, `clangd` + `bear`实现精准的补全与跳转
- `Editor`: `Neovim`
- `OS && DE`: `archlinux` + `KDE(X11)`.

-- --
