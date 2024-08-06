## 栈空间

我们需要关注`linkder.ld`
```ld
ENTRY(_start)
PHDRS { text PT_LOAD; data PT_LOAD; }

SECTIONS {
  /* _pmem_start and _entry_offset are defined in LDFLAGS */
  . = _pmem_start + _entry_offset;
  .text : {
    *(entry)
    *(.text*)
  } : text
  etext = .;
  _etext = .;
  .rodata : {
    *(.rodata*)
  }
  .data : {
    *(.data)
  } : data
  edata = .;
  _data = .;
  .bss : {
	_bss_start = .;
    *(.bss*)
    *(.sbss*)
    *(.scommon)
  }
  _stack_top = ALIGN(0x1000);
  . = _stack_top + 0x8000;
  _stack_pointer = .;
  end = .;
  _end = .;
  _heap_start = ALIGN(0x1000);
}
```
就可以看到栈空间大小`0x8000 bytes`.

## 支持异常处理

### `csrw`
```asm
80001384:       30571073                csrw    mtvec,a4
```
首先需要我们实现指令`csrw`. 

实际上这还是一个伪指令, 我们查阅手册就会发现对应的指令是`csrrw`
![](assets/Pasted%20image%2020240802111838.png)
 ![](assets/Pasted%20image%2020240802112853.png)

按照手册, 一共支持4096个CSR. 因为`12bit`的编码, 其中高`4bit`用来表示权限, 不过这里我们并不在乎权限. 

### `ecall`

![](assets/Pasted%20image%2020240802130605.png)

### 解析指令

实际上这里文档说的不是特别清晰, 硬件层面提供的
```c
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */
  /*set the cause*/
  csr(MCAUSE) = NO;
  /*set the epc*/
  csr(MEPC) = epc;

  /*intr vector is mtvec csr(0x305)*/
  return csr(MTVEC);
}
```
这是很容易实现的, 我们只需要支持`csr`的操作即可. 

实际上如果我们查看该函数的引用, 是没有的. 因为这个函数是希望我们在执行`ecall`指令的时候被调用的, 而不是在执行`ecall`的时候专门写个`helper`函数用于执行, 而是把这个过程抽离出来. 

## 保存上下文

- 实现这一过程中的新指令, 详情请RTFM.
- 理解上下文形成的过程并RTFSC, 然后重新组织`abstract-machine/am/include/arch/$ISA-nemu.h` 中定义的`Context`结构体的成员, 使得这些成员的定义顺序和 `abstract-machine/am/src/$ISA/nemu/trap.S`中构造的上下文保持一致.

```c
#define CONTEXT_SIZE  ((NR_REGS + 3) * XLEN)
#define OFFSET_SP     ( 2 * XLEN)
#define OFFSET_CAUSE  ((NR_REGS + 0) * XLEN)
#define OFFSET_STATUS ((NR_REGS + 1) * XLEN)
#define OFFSET_EPC    ((NR_REGS + 2) * XLEN)
```
我们首先关注一下这部分内容, 这个结构体的大小, 也就是寄存器+三个字长. 
那么按照偏移量来说, 结构体的顺序也就显然了. 首先放在最前面的是通用寄存器. 接下来`cause, status, epc`

> 地址空间. 这是为PA4准备的, 在x86中对应的是`CR3`寄存器, 代码通过一条`pushl $0`指令在堆栈上占位, mips32和riscv32则是将地址空间信息与0号寄存器共用存储空间, 反正0号寄存器的值总是0, 也不需要保存和恢复. 不过目前我们暂时不使用地址空间信息, 你目前可以忽略它们的含义.

实际上还有一个`void* pdir`. 这里其实有一个细节, 就是为什么用`uintptr_t`, 因为按照`C99`规定, 这个结构是一定可以放的下指针的, 所以我们在使用union的时候, 就能保证不破坏结构体的大小. 

在使用`union`的时候注意内存布局问题

> 你会在`__am_irq_handle()`中看到有一个上下文结构指针`c`, `c`指向的上下文结构究竟在哪里? 这个上下文结构又是怎么来的? 具体地, 这个上下文结构有很多成员, 每一个成员究竟在哪里赋值的? `$ISA-nemu.h`, `trap.S`, 上述讲义文字, 以及你刚刚在NEMU中实现的新指令, 这四部分内容又有什么联系?


这部分是要读汇编代码理解的, 实际上看到第一行就可以断定, 这个东西在函数
```c
extern void __am_asm_trap(void);
```
的栈上开辟的. 
```asm
  addi sp, sp, -CONTEXT_SIZE
```
首先就是可以观察到栈指针的变化.  他的赋值也是从栈上来的. 

我们把汇编宏展开一手
```shell
riscv64-linux-gnu-gcc -D__riscv_xlen=32 -E $AM_HOME/am/src/riscv/nemu/trap.S | less
```

首先一大坨存了通用寄存器
```asm
  sw x1, (1 * 4)(sp); sw x3, (3 * 4)(sp); sw x4, (4 * 4)(sp); sw x5, (5 * 4)(sp); sw x6, (6 * 4)(sp); sw x7, (7 * 4)(sp); sw x8, (8 * 4)(sp); sw x9, (9 * 4)(sp); sw x10, (10 * 4)(sp); sw x11, (11 * 4)(sp); sw x12, (12 * 4)(sp)
; sw x13, (13 * 4)(sp); sw x14, (14 * 4)(sp); sw x15, (15 * 4)(sp); sw x16, (16 * 4)(sp); sw x17, (17 * 4)(sp); sw x18, (18 * 4)(sp); sw x19, (19 * 4)(sp); sw x20, (20 * 4)(sp); sw x21, (21 * 4)(sp); sw x22, (22 * 4)(sp); sw x
23, (23 * 4)(sp); sw x24, (24 * 4)(sp); sw x25, (25 * 4)(sp); sw x26, (26 * 4)(sp); sw x27, (27 * 4)(sp); sw x28, (28 * 4)(sp); sw x29, (29 * 4)(sp); sw x30, (30 * 4)(sp); sw x31, (31 * 4)(sp); # sw x1, (1 * 4)(sp); sw x3, (3 
* 4)(sp); sw x4, (4 * 4)(sp); sw x5, (5 * 4)(sp); sw x6, (6 * 4)(sp); sw x7, (7 * 4)(sp); sw x8, (8 * 4)(sp); sw x9, (9 * 4)(sp); sw x10, (10 * 4)(sp); sw x11, (11 * 4)(sp); sw x12, (12 * 4)(sp); sw x13, (13 * 4)(sp); sw x14, 
(14 * 4)(sp); sw x15, (15 * 4)(sp); sw x16, (16 * 4)(sp); sw x17, (17 * 4)(sp); sw x18, (18 * 4)(sp); sw x19, (19 * 4)(sp); sw x20, (20 * 4)(sp); sw x21, (21 * 4)(sp); sw x22, (22 * 4)(sp); sw x23, (23 * 4)(sp); sw x24, (24 * 
4)(sp); sw x25, (25 * 4)(sp); sw x26, (26 * 4)(sp); sw x27, (27 * 4)(sp); sw x28, (28 * 4)(sp); sw x29, (29 * 4)(sp); sw x30, (30 * 4)(sp); sw x31, (31 * 4)(sp);
```

接下来把`csr`读出来
```asm
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
```

然后把`csr`存储到`context`里边
```asm
  sw t0, ((32 + 0) * 4)(sp)
  sw t1, ((32 + 1) * 4)(sp)
  sw t2, ((32 + 2) * 4)(sp)
```

调用函数. 注意这个函数接受一个`Context`的指针. 
```asm
# set mstatus.MPRV to pass difftest
  li a0, (1 << 17)
  or t1, t1, a0
  csrw mstatus, t1

  mv a0, sp
  jal __am_irq_handle
```


最后实际上呢, 栈上开辟了这个东西就是`*c`所指向的. 
```asm
mv a0, sp
```
这里传递的就是地址, `sp`, 因为这个结构体的空间是在栈上开辟的. `genius`. 


## 事件分发

![](assets/Pasted%20image%2020240802163001.png)

```c
Context *simple_trap(Event ev, Context *ctx) {
  switch(ev.event) {
    case EVENT_IRQ_TIMER:
      putch('t'); break;
    case EVENT_IRQ_IODEV:
      putch('d'); break;
    case EVENT_YIELD:
      putch('y'); break;
    default:
      panic("Unhandled event"); break;
  }
  return ctx;
}
```

## 恢复上下文

实现`mret`, 我们不需要考虑权限问题, 这里只用考虑`mepc`
![](assets/Pasted%20image%2020240802165712.png)
参考如下指令: 
![](assets/Pasted%20image%2020240802165823.png)

> 不过这里需要注意之前自陷指令保存的PC, 对于x86的`int`指令, 保存的是指向其下一条指令的PC, 这有点像函数调用; 而对于mips32的`syscall`和riscv32的`ecall`, 保存的是自陷指令的PC, 因此软件需要在适当的地方对保存的PC加上4, 使得将来返回到自陷指令的下一条指令.

不断输出的`y`. 看来是实现正确了 . 
![](assets/Pasted%20image%2020240802170138.png)

> 从`yield test`调用`yield()`开始, 到从`yield()`返回的期间, 这一趟旅程具体经历了什么? 软(AM, `yield test`)硬(NEMU)件是如何相互协助来完成这趟旅程的? 你需要解释这一过程中的每一处细节, 包括涉及的每一行汇编代码/C代码的行为, 尤其是一些比较关键的指令/变量. 事实上, 上文的必答题"理解上下文结构体的前世今生"已经涵盖了这趟旅程中的一部分, 你可以把它的回答包含进来.

- `call yield`
在这一步中, 我们首先在`a7`中保存了一个`-1`, 然后`ecall`. 

```c
void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}
```

- `nemu`执行`ecall`
```c
 INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall, N,
          s->dnpc = isa_raise_intr(-1, s->pc)); // pc of current instruction
```
这里也是有点意思, 我们设置`epc`, 返回中断向量. 不过这个时候, 我们的`NO`从哪里来呢? 
## 写了一个非常隐蔽的`bug`
在我添加`csr`支持之后, `difftest`无法通过了, 从第一条就开始无法通过. 然后便发现原因, 其实也很简单, 我们更改了`CPU_state`的结构. 但是`difftest`的时候使用的还是原来的`CPU_state`.因此拷贝内存的时候就拷贝错误了. 

解决方案也非常简单, 特殊寄存器和`cpu`分开就可以了. 不过需要记得初始化`mstatus`. 

## 前面的`NO`从哪里来? 

这个地方也是找了好久, 实际上我们不考虑权限问题. 假定就在`machine`这个层面上考虑. 最终在手册里边找到`exeception code` 这部分, 应该设置为`11`也就是我刚刚没有通过的`diff test`中的`0xb`


## `FTRACE`的`bug`
![](assets/Pasted%20image%2020240802195248.png)
我们观察一下, trap并没有被当作函数调用. 他是`no_type`的. 
我们希望这也是个函数, 从而能够被我们的`ftrace`追踪, 
```asm
.align 3
.section .text
.type __am_asm_trap, @function # 这里制定类型为函数
.globl __am_asm_trap
__am_asm_trap:
  addi sp, sp, -CONTEXT_SIZE

  MAP(REGS, PUSH)

  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc

  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  # set mstatus.MPRV to pass difftest
  li a0, (1 << 17)
  or t1, t1, a0
  csrw mstatus, t1

  mv a0, sp
  jal __am_irq_handle

  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  MAP(REGS, POP)

  addi sp, sp, CONTEXT_SIZE
  mret
  # 这里.是当前地址, 减去函数的开始, 得到函数的大小
  .size __am_asm_trap, .-__am_asm_trap 
```

![](assets/Pasted%20image%2020240802200530.png)
![](assets/Pasted%20image%2020240802200516.png)

`1508-13d0 = 138H = 312D`, 恰好就是这个函数的大小, 于是我们就可以追踪这个函数了. 
![](assets/Pasted%20image%2020240802201156.png)
## `ETRACE`

- 打开etrace不改变程序的行为(对程序来说是非侵入式的): 你将来可能会遇到一些bug, 当你尝试插入一些`printf()`之后, bug的行为就会发生变化. 对于这样的bug, etrace还是可以帮助你进行诊断, 因为它是在NEMU中输出的, 不会改变程序的行为.
- etrace也不受程序行为的影响: 如果程序包含一些致命的bug导致无法进入异常处理函数, 那就无法在CTE中调用`printf()`来输出; 在这种情况下, etrace仍然可以正常工作

这其实是平凡的, 我们只要在`nemu`中做就可以了, 这样就完全不会影响原程序. 

-- -- 
## `Nanos`
### `Newlib`

说实话这个地方编译`Newlib`的时候抱错了,  出现了很多隐式函数声明的错误. 我只能手动把这些`-Werror`关闭了. 
```makefile
CFLAGS+=-Wno-error=implicit-function-declaration
```

### 堆和栈 
这两个应当是在程序运行的时候动态分配的, 被放到内存的哪部分是由操作系统决定的. 程序只能决定栈的布局. 

### 识别不同格式
`ELF`格式有一个`magic number`就知道有没有`elf header`了. 

### `FileSize`与`MemSize`

```
Program Headers:  
 Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align  
 PHDR           0x000034 0x00000034 0x00000034 0x00180 0x00180 R   0x4  
 INTERP         0x0001b4 0x000001b4 0x000001b4 0x00013 0x00013 R   0x1  
     [Requesting program interpreter: /lib/ld-linux.so.2]  
 LOAD           0x000000 0x00000000 0x00000000 0x003f0 0x003f0 R   0x1000  
 LOAD           0x001000 0x00001000 0x00001000 0x001e0 0x001e0 R E 0x1000  
 LOAD           0x002000 0x00002000 0x00002000 0x000fc 0x000fc R   0x1000  
 LOAD           0x002ee8 0x00003ee8 0x00003ee8 0x00124 0x00128 RW  0x1000  
 DYNAMIC        0x002ef0 0x00003ef0 0x00003ef0 0x000f0 0x000f0 RW  0x4  
 NOTE           0x0001c8 0x000001c8 0x000001c8 0x00078 0x00078 R   0x4  
 GNU_PROPERTY   0x0001ec 0x000001ec 0x000001ec 0x00034 0x00034 R   0x4  
 GNU_EH_FRAME   0x002008 0x00002008 0x00002008 0x00034 0x00034 R   0x4  
 GNU_STACK      0x000000 0x00000000 0x00000000 0x00000 0x00000 RW  0x10  
 GNU_RELRO      0x002ee8 0x00003ee8 0x00003ee8 0x00118 0x00118 R   0x1
```

- 填充和内存堆砌
- 未初始化的变量`.bss`, 这部分在`filesize`应该是不占空间的. 

### `LOAD`

```
  +-------+---------------+-----------------------+
   |       |...............|                       |
   |       |...............|                       |  ELF file
   |       |...............|                       |
   +-------+---------------+-----------------------+
   0       ^               |              
           |<------+------>|       
           |       |       |             
           |       |                            
           |       +----------------------------+       
           |                                    |       
Type       |   Offset    VirtAddr    PhysAddr   |FileSiz  MemSiz   Flg  Align
LOAD       +-- 0x001000  0x03000000  0x03000000 +0x1d600  0x27240  RWE  0x1000
                            |                       |       |     
                            |   +-------------------+       |     
                            |   |                           |     
                            |   |     |           |         |       
                            |   |     |           |         |      
                            |   |     +-----------+ ---     |     
                            |   |     |00000000000|  ^      |   
                            |   | --- |00000000000|  |      |    
                            |   |  ^  |...........|  |      |  
                            |   |  |  |...........|  +------+
                            |   +--+  |...........|  |      
                            |      |  |...........|  |     
                            |      v  |...........|  v    
                            +-------> +-----------+ ---  
                                      |           |     
                                      |           |    
                                         Memory
```

注意剩余的那部分填充为`0`. [为什么填充为0](https://stackoverflow.com/questions/13437732/elf-program-headers-memsiz-vs-filesiz)

830003fc
### `.bss`与`.data`

```c
char a[0x77770];  
char b[0x66660] = "fuck you bitch";  
  
int main(){  
 return 0;  
}
```

![](assets/Pasted%20image%2020240804120716.png)

实际上这里应该是进行了内存对齐操作. 但是从数量就可以看出来, `.data`和`.bss`对应的就是初始化的部分和没有初始化的部分. `.bss`就是多出来的那部分内容. 

> 你需要找出每一个需要加载的segment的`Offset`, `VirtAddr`, `FileSiz`和`MemSiz`这些参数. 其中相对文件偏移`Offset`指出相应segment的内容从ELF文件的第`Offset`字节开始, 在文件中的大小为`FileSiz`, 它需要被分配到以`VirtAddr`为首地址的虚拟内存位置, 在内存中它占用大小为`MemSiz`. 也就是说, 这个segment使用的内存就是`[VirtAddr, VirtAddr + MemSiz)`这一连续区间, 然后将segment的内容从ELF文件中读入到这一内存区间, 并将`[VirtAddr + FileSiz, VirtAddr + MemSiz)`对应的物理区间清零.

### 再次`ELF Parser`

这次我们需要解析的是`program header`

这个就很好解析, 注意到`ELF`头的两条信息
```
 Size of program headers:           32 (bytes)  
 Number of program headers:         12
```

参照手册
```
e_phentsize: This member holds the size in bytes of one entry in the file's program header table; all entries are the same size.  

e_phnum: This member holds the number of entries in the program header table.  Thus the product of e_phentsize and e_phnum gives the table's size in bytes.  If a file has no program header, e_phnum holds the value zero.  
  
If  the  number of entries in the program header table is larger than or equal to PN_XNUM (0xffff), this member holds PN_XNUM (0xffff) and  the real number of entries in the program header table is held in the sh_info member of the initial entry in section header table.  Otherwise, the sh_info member of the initial entry contains the value zero.  
```

`Program Header`需要的结构体.
```c
          typedef struct {  
              uint32_t   p_type;  
              Elf32_Off  p_offset;  
              Elf32_Addr p_vaddr;  
              Elf32_Addr p_paddr;  
              uint32_t   p_filesz;  
              uint32_t   p_memsz;  
              uint32_t   p_flags;  
              uint32_t   p_align;  
          } Elf32_Phdr;
```

### 处理异常类型的时候发现前面的`bug`

```c
Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    switch (c->mcause) {
      case 0xb: ev.event = EVENT_YIELD; break;
      default: ev.event = EVENT_ERROR; break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }
  c->mepc += 4;//add 4 in software
  return c;
}
```
`riscv32`需要在适当的地方增加`epc`, 这是软件层面完成的. 这里如果不加, 那么会一直循环在同一个异常. 


### 一语惊醒梦中人

在设置异常号的时候, 根据当前情况, 我们的`m->status`只能是`11`, 因为我们不考虑权限这个问题. 但是这样的问题就是我们怎么识别系统调用? 这里困扰了我很久. 

实际上, 系统调用是软件层面的事情. 软件层面只需要根据`a7`识别就可以, 我们读代码发现, 其实早就发现, 异常的信息放在了`a7`里面. 

> 处理器通常只会提供一条自陷指令, 这时`EVENT_SYSCALL`和`EVENT_YIELD` 都通过相同的自陷指令来实现, 因此CTE需要额外的方式区分它们. 如果自陷指令本身可以携带参数, 就可以用不同的参数指示不同的事件, 例如x86和mips32都可以采用这种方式; 如果自陷指令本身不能携带参数, 那就需要通过其他状态来区分, 一种方式是通过某个寄存器的值来区分, riscv32采用这种方式.

```c
Context *__am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};

    switch (c->mcause) {
    // machine mode(highest)
    case 0xb: {
      // judege the system call
      // 这里面再套一层`case`, 根据a7识别
      switch (c->GPR1) {
      case (uint32_t)-1:
        ev.event = EVENT_YIELD;
        break;
      default: // unknown type
        ev.event = EVENT_ERROR;
      }
      c->mepc += 4; // add 4 in software
      break;
    }
    // unknown case
    default:
      ev.event = EVENT_ERROR;
      break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }
  return c;
}
```

![](assets/Pasted%20image%2020240804183805.png)

## 实现系统调用

先看一组宏
```c
// helper macros
#define _concat(x, y) x ## y
#define concat(x, y) _concat(x, y)
#define _args(n, list) concat(_arg, n) list
#define _arg0(a0, ...) a0
#define _arg1(a0, a1, ...) a1
#define _arg2(a0, a1, a2, ...) a2
#define _arg3(a0, a1, a2, a3, ...) a3
#define _arg4(a0, a1, a2, a3, a4, ...) a4
#define _arg5(a0, a1, a2, a3, a4, a5, ...) a5

// extract an argument from the macro array
#define SYSCALL  _args(0, ARGS_ARRAY)
#define GPR1 _args(1, ARGS_ARRAY)
#define GPR2 _args(2, ARGS_ARRAY)
#define GPR3 _args(3, ARGS_ARRAY)
#define GPR4 _args(4, ARGS_ARRAY)
#define GPRx _args(5, ARGS_ARRAY)
```
其实这里总结来, `GPRi`就是一组里面第`i`个寄存器. 

```c
# define ARGS_ARRAY ("ecall", "a7", "a0", "a1", "a2", "a0")
```

```c
intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {
  register intptr_t _gpr1 asm (GPR1) = type;
  register intptr_t _gpr2 asm (GPR2) = a0;
  register intptr_t _gpr3 asm (GPR3) = a1;
  register intptr_t _gpr4 asm (GPR4) = a2;
  register intptr_t ret asm (GPRx);
  asm volatile (SYSCALL : "=r" (ret) : "r"(_gpr1), "r"(_gpr2), "r"(_gpr3), "r"(_gpr4));
  return ret;
}
```

这里的赋值就是向特定的寄存器赋值
```
a7 <= type
a0 <= a0
a1 <= a1
a2 <= a2
return value <= a0
```
这里的`SYSCALL`展开后就是`ecall`. 


### 实现`SYS_yield`

这里的`libos`中的`makefile`留了个小坑
```makefile
# HAS_NAVY = 1
```
这里被注释掉了, 这就导致无法建立符号链接, 缺少头文件和镜像. 所以需要取消注释.
> 其实这里不是个坑, 不过似乎当时注释掉(在build navy的时候)就出现了链接问题. 后面发现应该是没影响的. 

> 添加一个系统调用比你想象中要简单, 所有信息都已经准备好了. 我们只需要在分发的过程中添加相应的系统调用号, 并编写相应的系统调用处理函数`sys_xxx()`, 然后调用它即可. 回过头来看`dummy`程序, 它触发了一个`SYS_yield`系统调用. 我们约定, 这个系统调用直接调用CTE的`yield()`即可, 然后返回`0`.


首先是`am`部分
```c
Context *__am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};

    switch (c->mcause) {
    // machine mode(highest)
    case 0xb: {
      // judege the system call
      switch (c->GPR1) {
      case (uint32_t)-1:
        ev.event = EVENT_YIELD;
        break;
      default: // unknown type
        ev.event = EVENT_SYSCALL;
        break;
      }
      // mepc += 4;
      c->mepc += 4; // add 4 in software
    } break;
    // unknown case
    default:
      ev.event = EVENT_ERROR;
      break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }
  return c;
}
```

接下来是`os`部分进行分发, 并对于`syscall`调用对应`do_syscall`
```c
static Context *do_event(Event e, Context *c) {
  switch (e.event) {
  case EVENT_YIELD:
    printf("Handle EVENT_YIELD\n");
    break;
  case EVENT_SYSCALL:
    printf("Handle EVENT_SYSCALL\n");
    do_syscall(c);
    break;
  default:
    panic("Unhandled event ID = %d", e.event);
  }

  return c;
}
```

最后添加具体`syscall`内容实现
```c
void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2; //it is  a0

  switch (a[0]) {
  case SYS_yield:
    sys_yield();
    break;
  case SYS_exit:
    sys_exit(a[1]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}
```


## `STRACE`

在`Nanos-lite`中实现并不困难, 我们只需要在`handler`的部分加上跟踪即可. 

> 在GNU/Linux中, 输出是通过`SYS_write`系统调用来实现的. 根据`write`的函数声明(参考`man 2 write`), 你需要在`do_syscall()`中识别出系统调用号是`SYS_write`之后, 检查`fd`的值, 如果`fd`是`1`或`2`(分别代表`stdout`和`stderr`), 则将`buf`为首地址的`len`字节输出到串口(使用`putch()`即可). 最后还要设置正确的返回值, 否则系统调用的调用者会认为`write`没有成功执行, 从而进行重试. 至于`write`系统调用的返回值是什么, 请查阅`man 2 write`. 另外不要忘记在`navy-apps/libs/libos/src/syscall.c`的`_write()`中调用系统调用接口函数.


## `write`

```c
int _write(int fd, void *buf, size_t count) {
 return _syscall_(SYS_write, fd, (intptr_t)buf, count);// 首先进行系统调用
}
```
`_syscall_`的返回值是`GPRx`, 这就是我们在对应系统调用中实现的返回值. 

```c
  switch (a[0]) {
  case SYS_yield:
    sys_yield();
    break;
  case SYS_exit:
    sys_exit(a[1]);
    break;
  case SYS_write:
    c->GPRx = sys_write(a[1], (void const *)a[2], a[3]); //设置返回值
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
```

## 堆的管理

> 1. program break一开始的位置位于`_end`
> 2. 被调用时, 根据记录的program break位置和参数`increment`, 计算出新program break
> 3. 通过`SYS_brk`系统调用来让操作系统设置新program break
 >4. 若`SYS_brk`系统调用成功, 该系统调用会返回`0`, 此时更新之前记录的program break的位置, 并将旧program break的位置作为`_sbrk()`的返回值返回
> 5. 若该系统调用失败, `_sbrk()`会返回`-1`

这里记得查询如何使用`end`.

> The program must explicitly declare these symbols; they are not defined in any header file.

```c
      #include <stdio.h>  
      #include <stdlib.h>  
  
      extern char etext, edata, end; /* The symbols must have some type,  
                                         or "gcc -Wall" complains */  
  
      int  
      main(void)  
      {  
          printf("First address past:\n");  
          printf("    program text (etext)      %10p\n", &etext);  
          printf("    initialized data (edata)  %10p\n", &edata);  
          printf("    uninitialized data (end)  %10p\n", &end);  
  
          exit(0);
       }
```

可以看到成功调用, 并且不是单个字符输出
![](assets/Pasted%20image%2020240805145601.png)

-- -- 

## 必答题

> 我们知道`navy-apps/tests/hello/hello.c`只是一个C源文件, 它会被编译链接成一个ELF文件. 那么, hello程序一开始在哪里? 它是怎么出现内存中的? 为什么会出现在目前的内存位置? 它的第一条指令在哪里? 究竟是怎么执行到它的第一条指令的? hello程序在不断地打印字符串, 每一个字符又是经历了什么才会最终出现在终端上?

首先这个源文件会被编译, 然后按照一定的方式与提供的库`libos`, `libc`链接成为一个`ELF`. 

如何读入数据? 原本数据还在`elf`文件中, 但是通过`resourse.S`
```asm
.section .data
.global ramdisk_start, ramdisk_end
ramdisk_start:
.incbin "build/ramdisk.img"
ramdisk_end:

.section .rodata
.globl logo
logo:
.incbin "resources/logo.txt"
.byte 0
```
这里而把`ramdisk.img`读入到内存中. 

第一条指令在哪里? 在我们`init_proc`的过程中调用了`naive_load`, 其中就是我们实现的`loader`, 解析`elf`文件, 加载`Program Header`中`LOAD`类型的段. 找到`entry poinrt`, 然后通过
```c
void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = 0x%p", entry);
  ((void (*)())entry)();
}
```
这里而这个看似很奇怪的东西其实很显然(只要SJ课学的好的话). 把一个`entry`转换为一个函数指针, 然后调用这个函数指针. 也就实现了向这个地方的跳转. 就到了第一条指令.

字符经过了什么到终端上面? 这里便会调用提供的`libc`, 其中有`stdio.h`的各种函数, 比如这里对于`printf`, 其中再往下划分, 会调用一个`write`函数. `write`函数会调用`libos`中的`_write`, `_write`调用`syscall`, 然后产生异常, 由操作系统处理`syscall`. 由下面的我们实现的处理进行输出. 
```c
static int sys_write(int fd, void const *buf, size_t count) {
  // sys write need to ret correct value;
  // On success, the number of bytes written is returned.
  // On error, -1 is returned, and errno is set to indicate the error.
  int ret = 0;
  assert(fd == 1 || fd == 2);
  for (size_t i = 0; i < count; i++) {
    putch(((char const *)buf)[i]);
    ret++;
  }
  return ret;
}
```

再往下一层抽象就是`putch`, 这是`am`中的接口. `am`中负责嗲用`outb`, 这个是一个内联汇编, 用于和`nemu`的`IO`设备进行交互. 最后一个字符就这样打印出来了. 

-- -- 
## `TODO`:  `FTRACE`支持多个文件

-- -- 

## 简单的文件系统

但在真实的操作系统中, 这种直接用文件名来作为读写操作参数的做法却有所缺陷. 例如, 我们在用`less`工具浏览文件的时候:

```
cat file | less
```

`cat`工具希望把文件内容写到`less`工具的标准输入中, 但我们却无法用文件名来标识`less`工具的标准输入! 实际上, 操作系统中确实存在不少"没有名字"的文件. 为了统一管理它们, 我们希望通过一个编号来表示文件, 这个编号就是文件描述符(file descriptor).

> 在Nanos-lite中, 由于sfs的文件数目是固定的, 我们可以简单地把文件记录表的下标作为相应文件的文件描述符返回给用户程序. 在这以后, 所有文件操作都通过文件描述符来标识文件:


实现文件操作
```c
int fs_open(const char *pathname, int flags, int mode);
size_t fs_read(int fd, void *buf, size_t len);
size_t fs_write(int fd, const void *buf, size_t len);
size_t fs_lseek(int fd, size_t offset, int whence);
int fs_close(int fd);
```

测试时不要忘记: 
![](assets/Pasted%20image%2020240805163841.png)


![](assets/Pasted%20image%2020240805183045.png)
其实如果`update`之后就会发现这个问题, 这是显然的. 因为这个时候`ramdisk`根本就不是一个`elf`文件. 他是前面一部分放的文件, 后面一部分是程序. 


![](assets/Pasted%20image%2020240805183218.png)
这就是为什么需要让`loader`使用文件. 

-- -- 
## 虚拟文件系统 

> 不过在Nanos-lite中, 由于特殊文件的数量很少, 我们约定, 当上述的函数指针为`NULL`时, 表示相应文件是一个普通文件, 通过ramdisk的API来进行文件的读写, 这样我们就不需要为大多数的普通文件显式指定ramdisk的读写函数了.

> 我们把文件看成字节序列, 大部分字节序列都是"静止"的, 例如对于ramdisk和磁盘上的文件, 如果我们不对它们进行修改, 它们就会一直位于同一个地方, 这样的字节序列具有"位置"的概念; 但有一些特殊的字节序列并不是这样, 例如键入按键的字节序列是"流动"的, 被读出之后就不存在了, 这样的字节序列中的字节之间只有顺序关系, 但无法编号, 因此它们没有"位置"的概念. 属于前者的文件支持`lseek`操作, 存储这些文件的设备称为"块设备"; 而属于后者的文件则不支持`lseek`操作, 相应的设备称为"字符设备". 真实的操作系统还会对`lseek`操作进行抽象, 我们在Nanos-lite中进行了简化, 就不实现这一抽象了.

-- --

## `OS`中的`IOE`

### 输出

> 首先当然是来看最简单的输出设备: 串口. 在Nanos-lite中, `stdout`和`stderr`都会输出到串口. 之前你可能会通过判断`fd`是否为`1`或`2`, 来决定`sys_write()`是否写入到串口. 现在有了VFS, 我们就不需要让系统调用处理函数关心这些特殊文件的情况了: 我们只需要在`nanos-lite/src/device.c`中实现`serial_write()`, 然后在文件记录表中设置相应的写函数, 就可以实现上述功能了. 由于串口是一个字符设备, 对应的字节序列没有"位置"的概念, 因此`serial_write()`中的`offset`参数可以忽略. 另外Nanos-lite也不打算支持`stdin`的读入, 因此在文件记录表中设置相应的报错函数即可.

### `timeofday`

> 实现`gettimeofday`系统调用, 这一系统调用的参数含义请RTFM. 实现后, 在`navy-apps/tests/`中新增一个`timer-test`测试, 在测试中通过`gettimeofday()`获取当前时间, 并每过0.5秒输出一句话.

```
DESCRIPTION  
      The functions gettimeofday() and settimeofday() can get and set the time as well as a timezone.  
  
      The tv argument is a struct timeval (as specified in <sys/time.h>):  
  
          struct timeval {  
              time_t      tv_sec;     /* seconds */  
              suseconds_t tv_usec;    /* microseconds */  
          };  
  
      and gives the number of seconds and microseconds since the Epoch (see time(2)).  
  
      The tz argument is a struct timezone:  
  
          struct timezone {  
              int tz_minuteswest;     /* minutes west of Greenwich */  
              int tz_dsttime;         /* type of DST correction */  
          };
```

我们可以看一个简单例子
```c
#include <sys/time.h>  
#include <stdio.h>  
  
int main() {  
   struct timeval tv;  
   struct timezone tz;  
  
   // 调用 gettimeofday() 函数获取当前时间e  
   if (gettimeofday(&tv, &tz) != 0) {  
       perror("gettimeofday failed");  
       return 1;  
   }  
  
   // 输出当前时间t  
   printf("Current time: %ld seconds, %ld microseconds\n",  
          tv.tv_sec, tv.tv_usec);  
  
   // 输出时区信息e  
   printf("Timezone: %d minutes west of Greenwich, DST correction: %d\n",  
          tz.tz_minuteswest, tz.tz_dsttime);  
  
   return 0;  
}
```

有如下输出:
```
Current time: 1722860988 seconds, 116871 microseconds  
Timezone: 0 minutes west of Greenwich, DST correction: 0
```

```c
int main() {
  struct timeval tv[1];
  uint64_t last = 0;
  uint64_t current = 0;
  int t = 100;
  while (t) {
    gettimeofday(tv, NULL);
    current = (tv->tv_sec * 1000000 + tv->tv_usec) / 1000; // get ms
    if (current - last >= 500) {
      t--;
      printf("TIME: %d\n", (int)current);
      last = current;
    }
  }
  return 0;
}
```

不过其实这里如果按照从`epoch`的时间其实也可以, 不过会有一点麻烦, 需要我们在`am`之中添加一个接口. 以及`nemu`中添加一个接口, 获得从`unix Epoch`的时间. 


![](assets/Pasted%20image%2020240806141043.png)
这里我尝试添加了一下, 并不复杂, `nemu -> am -> os`这三个层面增加即可. 实际上在输出的过程中还发现了, `libc`提供的输出并不支持`64`位输出. 

