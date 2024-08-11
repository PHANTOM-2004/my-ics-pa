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
  Context *const ret =
      (Context *)((intptr_t)kstack.end - (intptr_t)sizeof(Context));
  ret->mepc = (uintptr_t)entry; // set the entry point
  // in order to pass difftest
  ret->mstatus = 0x1800;
  return ret;
```

### 内核线程的参数

这里同样是参数传递的问题, 显然我们上面说过了, 这里`void*`是通过`a0`进行传递的.
![](assets/Pasted%20image%2020240811165547.png)


