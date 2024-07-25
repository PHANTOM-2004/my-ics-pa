
## 指令回顾- AUIPC
![](assets/Pasted%20image%2020240725105555.png)
![](assets/Pasted%20image%2020240725105625.png)

## 源码阅读

### MACRO

#### 位操纵
```cpp
#define BITMASK(bits) ((1ull << (bits)) - 1)
```

```cpp
#define BITS(x, hi, lo) (((x) >> (lo)) & BITMASK((hi) - (lo) + 1)) // x[hi:lo], verilog
```
这个宏就是取出`hi`到`lo`的`bit`. 

#### 符号位扩展
```cpp
#define SEXT(x, len) ({ struct { int64_t n : len; } __x = { .n = x }; (uint64_t)__x.n; })
```
这个宏其实顾名思义`sign extend`符号为扩展. 这里利用了结构体的位操作. 

可以思考如下片段: 
```c
#include <stdint.h>
#include <stdio.h>

struct T {
  int64_t n : 16;
} s;

int main() {
  struct T r;
  r.n = (unsigned short)0xffff;
  printf("%x\n", (unsigned)r.n);
}
```
输出
```
ffffffff
```
其实因为`int64_t`, 赋值的时候默认填充了高位. 也就实现了符号扩展

## 交叉编译库头文件的改动记录

```shell
sudo vim /usr/riscv64-linux-gnu/usr/include/gnu/stubs.h
```

```diff
@@ -5,5 +5,5 @@
 #include <bits/wordsize.h>

 #if __WORDSIZE == 32 && defined __riscv_float_abi_soft
-# include <gnu/stubs-ilp32.h>
+//# include <gnu/stubs-ilp32.h>
 #endif
```
这里注释掉一个头文件, 如果以后需要用到需要删除注释 

## 运行dummy

### 支持指令

```asm
Disassembly of section .text:

80000000 <_start>:
80000000:       00000413                li      s0,0
80000004:       00009117                auipc   sp,0x9
80000008:       ffc10113                addi    sp,sp,-4 # 80009000 <_end>
8000000c:       00c000ef                jal     80000018 <_trm_init>

80000010 <main>:
80000010:       00000513                li      a0,0
80000014:       00008067                ret

80000018 <_trm_init>:
80000018:       ff010113                addi    sp,sp,-16
8000001c:       00000517                auipc   a0,0x0
80000020:       01c50513                addi    a0,a0,28 # 80000038 <_etext>
80000024:       00112623                sw      ra,12(sp)
80000028:       fe9ff0ef                jal     80000010 <main>
8000002c:       00050513                mv      a0,a0
80000030:       00100073                ebreak
80000034:       0000006f                j       80000034 <_trm_init+0x1c>
```

> 这里读了很多手册, 一直在找低一个`li`指令, 首先我们发现这是一个伪指令, 然后那么就在想, 伪指令的编码是什么 ? 但是最后一直没有找到伪指令的编码, 因为伪指令是汇编器的内容, 然后汇编器来转换为真实指令. 
> 
> 其实到这里应该明白了, 但是我还是在糊涂, 总不可能我一个模拟`cpu`执行的人来做伪指令的任务吧 ? 所以说伪指令本来就没有编码, 汇编的时候其实已经直接转变为真实指令了. 所以这里`li`指令的实现

### Decode结构

```c
typedef struct Decode {
  vaddr_t pc;
  vaddr_t snpc; // static next pc
  vaddr_t dnpc; // dynamic next pc
  ISADecodeInfo isa;
  IFDEF(CONFIG_ITRACE, char logbuf[128]);
} Decode;
```

###  `exec_once` 剖析

- 首先送入当前`cpu.pc`, 以及一个未初始化的`Decode`结构体
- 设置该`Decode`的`pc`, `spc`为当前`cpu.pc`. 
- 进入`isa_exec_once()`
- 进入后内部有一个`inst_fetch`, 
```c
s->isa.inst.val = inst_fetch(&s->snpc, 4);
````
这个函数利用`s-snpc`进行取指令, 然后给`s->snpc`加上增量. 于是完成了静态`npc`的赋值. 
```c
static inline uint32_t inst_fetch(vaddr_t *pc, int len) {
  uint32_t inst = vaddr_ifetch(*pc, len);
  (*pc) += len;
  return inst;
}
```


### 指令模式匹配

这里首先需要分析一个宏`INSTPAT_START()`, 利用`clangd`我们得到展开后的结果
```c
#define INSTPAT_START(name)                                                    \
  {                                                                            \
    const void **__instpat_end = &&concat(__instpat_end_, name);

// Expands to
{
  const void **__instpat_end = &&__instpat_end_;
```

```c
#define INSTPAT_END(name)                                                      \
  concat(__instpat_end_, name) :;                                              \
  }

// Expands to
__instpat_end_ :;
}
```

因此这两个中间包裹了
```c
{
  const void **__instpat_end = &&__instpat_end_; //这里是标签地址扩展功能, 

// 中间是指令的模式匹配, 比如我们展开其中一个
// Expands to
do {
  uint64_t key, mask, shift;
  pattern_decode("??????? ????? ????? ??? ????? 00101 11",
                 (sizeof("??????? ????? ????? ??? ????? 00101 11") - 1), &key,
                 &mask, &shift);
  if ((((uint64_t)((s)->isa.inst.val) >> shift) & mask) == key) {
    {
      decode_operand(s, &rd, &src1, &src2, &imm, TYPE_U);
      (cpu.gpr[check_reg_idx(rd)]) = s->pc + imm;
    };
    goto *(__instpat_end);// goto直接跳转到最后
  }


  __instpat_end_ :;
}
```


接下来分析`patter_decode`

```c
__attribute__((always_inline)) static inline void
pattern_decode(const char *str, int len, uint64_t *key, uint64_t *mask,
               uint64_t *shift) {
  uint64_t __key = 0, __mask = 0, __shift = 0;
#define macro(i)                                                               \
  if ((i) >= len)                                                              \
    goto finish;                                                               \
  else {                                                                       \
    char c = str[i];                                                           \
    if (c != ' ') {                                                            \
      Assert(c == '0' || c == '1' || c == '?',                                 \
             "invalid character '%c' in pattern string", c);                   \
      __key = (__key << 1) | (c == '1' ? 1 : 0);                               \
      __mask = (__mask << 1) | (c == '?' ? 0 : 1);                             \
      __shift = (c == '?' ? __shift + 1 : 0);                                  \
    }                                                                          \
  }
  // 这个macro(i)做的事情, 就是把第i位抽取出来, 0/1放到 key中, 
  // 同时设置对应的mask, shift决定匹配的有效位置

#define macro2(i)                                                              \
  macro(i);                                                                    \
  macro((i) + 1)
#define macro4(i)                                                              \
  macro2(i);                                                                   \
  macro2((i) + 2)
#define macro8(i)                                                              \
  macro4(i);                                                                   \
  macro4((i) + 4)
#define macro16(i)                                                             \
  macro8(i);                                                                   \
  macro8((i) + 8)
#define macro32(i)                                                             \
  macro16(i);                                                                  \
  macro16((i) + 16)
#define macro64(i)                                                             \
  macro32(i);                                                                  \
  macro32((i) + 32)
  macro64(0);
  panic("pattern too long");
#undef macro
finish:
  *key = __key >> __shift;
  *mask = __mask >> __shift;
  *shift = __shift;
}
```

```c
macro64(0) ->
macro32(0), macro32(32) ->
macro16(0), macro16(16), macro16(32), macro16(48) ->
macro8(0), macro8(8), macro8(16), macro8(24), 
```
实际上画个图你就可以发现, 这是一颗二叉树, 这里的`if`判断就是保证之参与`len` 位的解码. 这个二叉树恰好包含了所有的位, 并且是按照顺序从0开始的.  

最后的三句话
```c
  *key = __key >> __shift;
  *mask = __mask >> __shift;
  *shift = __shift;
```
把对应的`key`移到最低位, 以及对应的`mask`移动到最低位. 然后再看译码做的事情: 
```c
  if ((((uint64_t)((s)->isa.inst.val) >> shift) & mask) == key) {
    {
      decode_operand(s, &rd, &src1, &src2, &imm, TYPE_U);
      (cpu.gpr[check_reg_idx(rd)]) = s->pc + imm;
    };
```
这其实非常自然了, 把`instruction`先移位得到目标, 然后与`mask`进行与来提取, 看得到是不是模式对应的`key`. 

前文我们的模式码为什么能写成U, I, S, N呢? 这里也是利用宏的简化, 利用`concat`进行拼接. 
```c
INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
```

```c
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}
```

