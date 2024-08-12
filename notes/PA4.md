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

> 一山不容二虎? 

这是因为两者加载的时候用的是同样的栈, 会把对方覆盖掉. 

### 用户进程的参数

> 既然用户进程是操作系统来创建的, 很自然参数和环境变量的传递就需要由操作系统来负责. 最适合存放参数和环境变量的地方就是用户栈了, 因为在首次切换到用户进程的时候, 用户栈上的内容就已经可以被用户进程访问. 于是操作系统在加载用户进程的时候, 还需要负责把`argc/argv/envp`以及相应的字符串放在用户栈中, 并把它们的存放方式和位置作为和用户进程的约定之一, 这样用户进程在`_start`中就可以根据约定访问它们了.

> 这项约定其实属于ABI的内容, ABI手册有一节Process Initialization的内容, 里面详细约定了操作系统需要为用户进程的初始化提供哪些信息. 不过在我们的Project-N系统里面, 我们只需要一个简化版的Process Initialization就够了: 操作系统将`argc/argv/envp`及其相关内容放置到用户栈上, 然后将GPRx设置为`argc`所在的地址.

```
|               |
+---------------+ <---- ustack.end (0x88000000)
|  Unspecified  |
+---------------+
|               | <----------+
|    string     | <--------+ |
|     area      | <------+ | |
|               | <----+ | | |
|               | <--+ | | | |
+---------------+    | | | | |
|  Unspecified  |    | | | | |
+---------------+    | | | | |
|     NULL      |    | | | | |
+---------------+    | | | | |
|    ......     |    | | | | |
+---------------+    | | | | |
|    envp[1]    | ---+ | | | |
+---------------+      | | | |
|    envp[0]    | -----+ | | |
+---------------+        | | |
|     NULL      |        | | |
+---------------+        | | |
| argv[argc-1]  | -------+ | |
+---------------+          | |
|    ......     |          | |
+---------------+          | |
|    argv[1]    | ---------+ |
+---------------+            |
|    argv[0]    | -----------+
+---------------+
|      argc     |
+---------------+ <---- cp->GPRx 
|               |
```

> 此外, 上图中的`Unspecified`表示一段任意长度(也可为0)的间隔, 字符串区域中各个字符串的顺序也不作要求, 只要用户进程可以通过`argv/envp`访问到正确的字符串即可.

> 根据这一约定, 你还需要修改Navy中`_start`的代码, 把`argc`的地址作为参数传递给`call_main()`. 然后修改`call_main()`的代码, 让它解析出真正的`argc/argv/envp`, 并调用`main()`

实际上`argc`的地址正是`sp`. 也就是我们的`GPRx`.

我们可以看一下`heap`的空间. 
```
heap.start: [0x80359000]      heap.end(GPRx->sp): [0x88000000]
```
我们把堆的最后作为用户栈, 这是基于用户栈是从高往低增长的. 

至于这些字符串放在哪里, 正是我需要做的事情. 但是用户栈的大小, 现在似乎还没提到具体的事情. 


```c
void call_main(uintptr_t *args) {
  // set argc/argv/envp
  int argc = *(int*) args;
  char ** argv = (char**) ((int*)args + 1);
  char **envp = (argv + argc + 1);
  
  environ = envp;

  exit(main(argc, argv, envp));
  assert(0);
}
```

更改函数签名
```c
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]);
```

这里我有个疑惑的地方, 这几个东西从哪里来? 我从哪里把这几个量拷贝到用户栈上面? 

![](assets/Pasted%20image%2020240812005053.png)
看到这里我就明白了, 还是要多看两眼, 有些答案可能就在后面

并且当时在想, 用户栈的栈顶应当和参数有关. 因此我们设置
```c
pcb->cp->GPRx = //?
```
并不总是`heap.end`, 这取决于我们的栈使用情况. 

```c
void context_uload(PCB *const pcb, char const *const fname, char const *argv[],
                   char *const envp[]) {
  // NOTE:heap.end is the stack top for user program

  Area const kstack_area = {
      .start = (void *)pcb,
      .end = (void *)((uintptr_t)pcb + (uintptr_t)sizeof(pcb->stack))};

  extern uintptr_t loader(PCB * pcb, const char *filename);
  uintptr_t const entry = loader(pcb, fname); // get_elf_entry(fname);

  Log("register sp: %x", (uintptr_t)heap.end);
  pcb->cp = ucontext(NULL, kstack_area, (void *)entry);

  // then we need to load the strings to user stack
  uintptr_t const string_area_start = ((uintptr_t)kstack_area.end - 64);
  uintptr_t sptr = string_area_start;

  int argc = 0, envpc = 0;

  // first copy argv string
  for (char const **p = argv; *p != NULL; p++) {
    size_t const len = strlen(*p) + 1;
    sptr -= len;
    memcpy((void *)sptr, *p, len);
    argc++;
  }

  // then copy envp string
  for (char *const *p = envp; *p != NULL; p++) {
    size_t const len = strlen(*p) + 1;
    sptr -= len;
    memcpy((void *)sptr, *p, len);
    envpc++;
  }

  sptr -= 64;                             // let the unspecified = 64;
  sptr = sptr & (~sizeof(uintptr_t) + 1); // align with pointer

  // copy envp pointer, first is NULL
  assert(envp[envpc] == NULL);
  for (int i = envpc; i >= 0; i--) {
    sptr -= sizeof(char*);
    char **pos = (void *)sptr;
    *pos = envp[i];
  }

  //copy argv pointer, first is NULL
  assert(argv[argc] == NULL);
  for (int i = argc; i >= 0; i--) {
    sptr -= sizeof(char const*);
    char const **pos = (void *)sptr;
    *pos = argv[i];
  }

  // finally copy argc
  sptr -= sizeof(int);
  *(int*)sptr = argc;

  // set the top of user stack
  pcb->cp->GPRx = (uintptr_t)sptr;
}
```

> 在`main()`函数中, `argv`和`envp`的类型是`char * []`, 而在`execve()`函数中, 它们的类型则是`char *const []`. 从这一差异来看, `main()`函数中`argv`和`envp`所指向的字符串是可写的, 你知道为什么会这样吗?

这部分内容已经被加载到用户栈之中了, 所以我们可以随便写这些东西. 

### 实现`execve`

为了探究这个问题, 我们需要了解当Nanos-lite尝试通过`SYS_execve`加载B时, A的用户栈里面已经有什么内容. 我们可以从栈底(`heap.end`)到栈顶(栈指针`sp`当前的位置)列出用户栈中的内容:

- Nanos-lite之前为A传递的用户进程参数(`argc/argv/envp`)
- A从`_start`开始进行函数调用的栈帧, 这个栈帧会一直生长, 直到调用了libos中的`execve()`
- CTE保存的上下文结构, 这是由于A在`execve()`中执行了系统调用自陷指令导致的
- Nanos-lite从`__am_irq_handle()`开始进行函数调用的栈帧, 这个栈帧会一直生长, 直到调用了`SYS_execve`的系统调用处理函数

通过上述分析, 我们得出一个重要的结论: 在加载B时, Nanos-lite使用的是A的用户栈! 这意味着在A的执行流结束之前, A的用户栈是不能被破坏的. 因此`heap.end`附近的用户栈是不能被B复用的, 

<u>我们应该申请一段新的内存作为B的用户栈, 来让Nanos-lite把B的参数放置到这个新分配的用户栈里面.</u>

> 为了实现这一点, 我们可以让`context_uload()`统一通过调用`new_page()`函数来获得用户栈的内存空间. `new_page()`函数在`nanos-lite/src/mm.c`中定义, 它会通过一个`pf`指针来管理堆区, 用于分配一段大小为`nr_page * 4KB`的连续内存区域, 并返回这段区域的首地址. 我们让`context_uload()`通过`new_page()`来分配32KB的内存作为用户栈, 这对PA中的用户程序来说已经足够使用了. 此外为了简化, 我们在PA中无需实现`free_page()`.

> 有一些细节我们并没有完全给出, 例如调用`context_uload()`的`pcb`参数应该传入什么内容, 这个问题就交给你来思考吧!


> 最后, 为了结束A的执行流, 我们可以在创建B的上下文之后, 通过`switch_boot_pcb()`修改当前的`current`指针, 然后调用`yield()`来强制触发进程调度. 这样以后, A的执行流就不会再被调度, 等到下一次调度的时候, 就可以恢复并执行B了.


<u> 我们应当传入一个PCB, 内核栈我们无所谓谁在使用, 因为他的作用只是作为上下文恢复的. 但是用户栈我们需要考虑是谁在使用, 因此 传递的时候只需要传递`current`即可</u>



我们梳理一下`sys_execve`的流程

1. 启动时`kload hello, uload app`.
2. 第一次`yield`切换到`hello`, `hello`之中继续`yield`切换到`app`
3. `app`之中调用`sys_execve`, 此时`current`指向了`app`的`PCB`, 我们把`current`传递给`uload`.
4. 调用 `switch_boot_pcb`, `yield`.此时回到了情况`1`但是加载的是新程序

- 测试`exec-test`
![](assets/Pasted%20image%2020240812153423.png)

- 测试`menu`
![](assets/Pasted%20image%2020240812153601.png)

- 完善`Nterm`的参数解析
![](assets/Pasted%20image%2020240812163230.png)


### busybox

这里编译的时候加上一些选项
```makefile
CFLAGS += -Wno-error=implicit-function-declaration
CFLAGS += -Wno-error=int-conversion
```

![](assets/Pasted%20image%2020240812171055.png)
我们可以看到这里的顺序, 首先加载程序会把前文的数据段覆盖掉. 因此我们需要先加载参数再加载程序.


```
read paddr:     83014ae0[4]     read data:      0x698c42f0
read paddr:     808a3eac[4]     read data:      0x698c42f0
```

这里发现一个严重的问题, 就是如果我先拷贝参数再加载发现参数没了. 也就是在`loader`的过程中我们的用户`sp`被`corrupt`了.  回顾我们之前使用的`loader`之中的`malloc`, 我们发现这个东西与`new_page()`提供的空间冲突了. 那么这样的结果就不难解释了. 

```c
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
  static uint8_t *addr = NULL;
  if (addr == NULL)
    addr = heap.start;
  size = ROUNDUP(size, 8);
  void *const res = (void *)addr;
  addr += size;
  assert((uint32_t)addr >= (uint32_t)heap.start && (uint32_t)addr < (uint32_t)heap.end);
  for (uint32_t *p = res; p != (uint32_t*)addr; p++)
    *p = 0; // actually no need to set zero

  return res;
#endif
```

因此我们操作系统申请的内存统一使用`new_page`进行管理

> 不过为了遍历`PATH`中的路径, `execvp()`可能会尝试执行一个不存在的用户程序, 例如`/bin/wc`. 因此Nanos-lite在处理`SYS_execve`系统调用的时候就需要检查将要执行的程序是否存在, 如果不存在, 就需要返回一个错误码. 我们可以通过`fs_open()`来进行检查, 如果需要打开的文件不存在, 就返回一个错误的值, 此时`SYS_execve`返回`-2`. 另一方面, libos中的`execve()`还需要检查系统调用的返回值: 如果系统调用的返回值小于0, 则通常表示系统调用失败, 此时需要将系统调用返回值取负, 作为失败原因设置到一个全局的外部变量`errno`中, 然后返回`-1`.

```shell
~/P/c/N/P/i/nanos-lite (pa4|✚5) $ errno -l  
EPERM 1 Operation not permitted  
ENOENT 2 No such file or directory
```
