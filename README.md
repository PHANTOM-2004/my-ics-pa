# My implementation for ICS2023-pa

项目说明以及概要请参照`2023`分支. (For more information, please refer to `branch 2023`)

本项目由我个人独立完成, 如果正在写PA的NJU的同学恰好看到这个仓库, 请自觉忽略(Do your homework by yourself, TOO).
-- --

## What is it?

### `nemu`

1. 实现了`nemu`, 这是一个`emulator`. 我选择了`riscv32`作为`ISA`.
`nemu`(类似于`qemu`)模拟`cpu`取指, 译码, 执行的过程.

2. 其中包含一个简单的`monitor`, `monitor`中实现`debug`与`trace`系统.

- `ITRACE`: 对指令调用的跟踪, 采用环形缓冲区
- `FTRACE`: 对函数调用的跟踪. 这里实现一个简单的`elf_parser`解析`elf`文件, 读取符号表中的函数信息
- `MTRACE`: 对指令访存的跟踪.
- `DTRACE`: 访问外设的跟踪.
- `DEBUG`: 实现单步执行, 监视点, 表达式求值等. 同时表达式求值部分进行了充分测试.

### `Abstract Machine`

实现了`abstract-machine`
对于完整的`computer`而言, 只有`cpu`是不够的. 因此`AM`中包含了程序运行需要的`runtime`(在这里也就是`klib`).
同时还需要包含与外界的`IOE(Input Output Extension`). 这里通过静态链接, 把交叉编译的程序链接到
`klib`. 同时在`AM`中提供内部程序与键盘, VGA等固件交互的接口.

- `klib`: 简单的标准库, 包括`printf`系列以及`string`系列的常用函数.
- `IOE`: 实现对`nemu`的扩展, 与外设的交互.

-- --
可以在`nemu`中执行依赖简单的程序(简单指的是不需要复杂`runtime`的程序). 同时实现键盘输入与视频输出.

## Toolchain && Environment

使用正确的工具

- `gcc`: `gcc (GCC) 14.1.1 20240720`
- `llvm toolchain`, `clangd` + `bear`实现精准的补全与跳转
- `Editor`: `Neovim`
- `OS && DE`: `archlinux` + `KDE(X11)`.

-- --

## Update Logs

to be continued
