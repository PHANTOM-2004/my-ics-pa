<u>不要忘记在前面悬而未决的`union`关于`gpr(0)`的问题</u>

## 多道程序

 > 内存加载到不同位置? 

实际上有一个比较明显的地方, 就是对于在目前编译条件编译出的固定加载位置的程序来说, 我们显然不能把这个东西交给编译器来做. 


> 关于`union`的内存内布局

```c
#include <stdio.h>

union{
  int val_int;
  char buffer[128];
}a;

int main(){
  printf("int: %x buffer: %x\n", &a.val_int, &a.buffer);
  return 0;
}
```

我们可以看到在`int`和`char[0]`这部分共用地址. 
```
int: 75c3a040 buffer: 75c3a040
```

具体可以参考[回答](https://stackoverflow.com/questions/6352199/memory-layout-of-union-of-different-sized-member)

![](assets/Pasted%20image%2020240811160604.png)

```c
  pcb[0].cp = kcontext((Area) { pcb[0].stack, &pcb[0] + 1 }, f, (void *)1L);
  pcb[1].cp = kcontext((Area) { pcb[1].stack, &pcb[1] + 1 }, f, (void *)2L);
```

### 实现`kcontext()`以及上下文切换

> - 修改CTE中`__am_asm_trap()`的实现, 使得从`__am_irq_handle()`返回后, 先将栈顶指针切换到新进程的上下文结构, 然后才恢复上下文, 从而完成上下文切换的本质操作

> 至于"先将栈顶指针切换到新进程的上下文结构", 很自然的问题就是, 新进程的上下文结构在哪里? 怎么找到它? 又应该怎么样把栈顶指针切换过去? 如果你发现代码跑飞了, 不要忘记, 程序是个状态机.

我们知道在`irq_handler`这部分, 对于`Context`的处理:
```c
    c = user_handler(ev, c);
```

而在我们刚才的`yield-os`之中, 能够发现
```c
cte_init(schedule);

// and user_handler function
static Context *schedule(Event ev, Context *prev) {
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  return current->cp;
}
```
那么这个`schedule`就是决定下一个上下文`Context`在哪里的`handler`. 这就回答了新进程的上下文结构在哪里的问题. 至于怎么找到他, 他就是`__am_irq_handler`的返回值. 

```asm
  mv a0, sp
  jal __am_irq_handle
```
我们看到在`trap.S`中调用了这一函数, `riscv32`返回值的约定是
![](assets/Pasted%20image%2020240811163330.png)

![](assets/Pasted%20image%2020240811163355.png)

于是我们设置`sp`为`a0`, 这样就可以恢复`a0`中存储的`c`的上下文
```asm
  mv a0, sp
  jal __am_irq_handle

  # the return value Context* c is stored in a0
  # set sp = c, so that restore the context of c
  mv sp, a0

  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2
```

> 我们希望代码将来从`__am_asm_trap()`返回之后, 就会开始执行`f()`. 换句话说, 我们需要在`kcontext()`中构造一个上下文, 它指示了一个状态, 从这个状态开始, 可以正确地开始执行`f()`. 所以你需要思考的是, 为了可以正确地开始执行`f()`, 这个状态究竟需要满足什么样的条件?

注意最后的
```asm
  mret
```
我们回顾`mret`做了什么? 
```c
  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret, N, s->dnpc = csr(MEPC));
```
因此我们的`entry`这个函数指针正是`mepc`应当存储的值

```c
  Context *const ret =   Context *const ret = (Context*)kstack.end - 1;
  ret->mepc = (uintptr_t)entry; // set the entry point
  // in order to pass difftest
  ret->mstatus = 0x1800;
  return ret;
```

### 内核线程的参数

这里同样是参数传递的问题, 显然我们上面说过了, 这里`void*`是通过`a0`进行传递的.
![](assets/Pasted%20image%2020240811165547.png)

-- -- 
### `OS`中的上下文切换

![](assets/Pasted%20image%2020240811170152.png)

> `schedule`函数

```c
Context *schedule(Context *prev) {
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  return current->cp;
}
```

假如当前正在运行的`PCB`是`1`号, 那么设置`1`号的`cp`为当前的`cp`. 同时返回`2`号的上下文`cp`.
假如当前正在运行的`PCB`是`2`号, 那么设置`2`号的`cp`为当前的`cp`. 同时返回`1`号的上下文`cp`.

这里的设置其实就是保存的意思. 注意存储在`current`, 如果不保存, 那么`current`之中就不可能返回正确的上下文.


![](assets/Pasted%20image%2020240811180356.png)

> 这里`schedule`的理解很重要

```c
  context_kload(&pcb[0], hello_fun, (void *)0xcc);
  context_kload(&pcb[1], hello_fun, (void *)0xff);
  switch_boot_pcb();
  yield(); // add yield here
```

首先第一次`yield`的时候, 我们也不知道`Context* c`是哪里, 因为是保存的当时的现场. 于是第一次`current`的等号必然不成立, 因此`current`被设置为`0`号. 恢复`0`号的现场. 之后就是正常交错了.

![](assets/Pasted%20image%2020240811181206.png)
如上图, 就是这样, 首先输出`cc`.

-- --  
### 用户进程

#### 正确设置栈指针

> 用户程序的入口位于`navy-apps/libs/libos/src/crt0/start.S`中的`_start()`函数, 这里的`crt`是`C RunTime`的缩写, `0`的含义表示最开始. `_start()`函数会调用`navy-apps/libs/libos/src/crt0/crt0.c`中的`call_main()`函数, 然后调用用户程序的`main()`函数, 从`main()`函数返回后会调用`exit()`结束运行.

> 于是Nanos-lite和Navy作了一项约定: Nanos-lite把栈顶位置设置到GPRx中, 然后由Navy里面的`_start`来把栈顶位置真正设置到栈指针寄存器中.

我们约定的GPRx就是`a0`
```asm
  mv s0, zero
  mv sp, a0 
  jal call_main
```

但是这还没有结束, `heap.end`具体是什么? 我们`log`一下就可以发现, 实际上这在前面也写过了
```
88000000
```
这正是我们需要放到`a0`中的数字. 但是这个并不能在`NANOS`中的因为这是`ISA`独立的. 因此这个设置只能是在上下文恢复的过程中设置的. 

虽然说这样, 但是思考过后, 唯一的方法只能是在`nanos-lite`这里就直接设置栈顶指针. 但是是通过上下文的方式



#### 实现`ucontext`
这个几乎和`kcontext`相同, 但是这里不需要传递参数. 

#### 实现`context_uload`

> 不过你还是需要思考, 对于用户进程来说, 它需要一个什么样的状态来开始执行呢?

我们可以想一想之前的`kload`做了什么. `yield-os`之中, 我们首先通过`kcontext`设置了对应的栈. 
我们在栈里面设置的是一个`context`, 包含了寄存器信息, 尤其是`epc`信息, `epc`之中存放的是`entry`.

但是如果按照这样, 我们把用户程序加载进来, 很显然能够发现一个问题, 总不能每次用户程序都从`elf`之中的`entry`从头开始吧? 

再思考一下. 我们上来`load`了一个用户程序, 然后等待输入的时候`yield`, 此时保存的是用户程序的现场, 包括`epc`等等. 这个时候我们是没有使用`entry`的, 这个`entry`实际上只有我们第一次的时候用到了. 

```c
void context_uload(PCB *const pcb, char const *const fname) {
  // NOTE:heap.end is the stack top for user program

  Area const kstack_area = {
      .start = (void *)pcb,
      .end = (void *)((uintptr_t)pcb + (uintptr_t)sizeof(pcb->stack))};

  extern uintptr_t loader(PCB *pcb, const char *filename) ;
  uintptr_t const entry = loader(pcb, fname);//get_elf_entry(fname);

  pcb->cp = ucontext(NULL, kstack_area, (void *)entry);
  pcb->cp->GPRx = (uintptr_t)heap.end;
}
```


![](assets/Pasted%20image%2020240811215342.png)


这里我尝试使用`native`但是错误了, 排查了原因可以发现
```c
  pcb->cp = ucontext(NULL, kstack_area, (void *)entry);
```
我这里使用了空指针, 但是实际上不应该是空指针. 现在是空指针只是因为我没有处理, 但是`native`的`am`是处理的. 因此会发生段错误.


