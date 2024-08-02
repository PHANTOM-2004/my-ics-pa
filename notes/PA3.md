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
## 
