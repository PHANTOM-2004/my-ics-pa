
## 指令回顾- AUIPC
![](assets/Pasted%20image%2020240725105555.png)
![](assets/Pasted%20image%2020240725105625.png)

## 源码阅读

### MACRO

#### 位操纵
```c
#define BITMASK(bits) ((1ull << (bits)) - 1)
```

```c
#define BITS(x, hi, lo) (((x) >> (lo)) & BITMASK((hi) - (lo) + 1)) // x[hi:lo], verilog
```
这个宏就是取出`hi`到`lo`的`bit`. 

#### 几种立即数类型
> 这部分是框架已经提供的, 作为参考

- `I-imm`
```c
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
```
`I`这一种比较显然, 把原来的`12bit`进行扩展
- `U-imm`
```c
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
```
`U`这种也比较显然, 把原来的`20bit`进行扩展. 
- `S-imm`
```
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
```
`S`这种也先相对容易理解, 就是把立即数拼接起来. 立即数被编码放进了两部分. 

> 框架的立即数是不够的, 需要我进行补充

- `B-imm`
> 值得注意的是`S/B`两个`type`的复杂性, 尤其是对于`S`. 具体可以看这一篇文章的[解释](https://stackoverflow.com/questions/58414772/why-are-risc-v-s-b-and-u-j-instruction-types-encoded-in-this-way#:~:text=The%20only%20difference%20between%20the,2%20in%20the%20B%20format.)
> 总结下来有两点: (1)指令格式一致性方便译码 (2)默认2的倍数, 因此直接不存最低一位, 就可以多存一位, 最高位还是放在原来的地方, 因为那一位用于符号扩展. 
```c
#define immB()                                                  \
  do {                                                          \
    *imm = (BITS(i, 31, 31) << 12) | (BITS(i, 7, 7) << 11) |    \
           (BITS(i, 30, 25) << 5) | (BITS(i, 11, 8) << 1);      \
  }while(0)
```

- `J-imm`
这个与`B-imm`是同理的.
> The offset is sign-extended and added to the **address of the jump instruction** to form the jump target address. Jumps can therefore target a ±1 MiB range. JAL stores the address of the instruction following the jump ('pc'+4) into register rd

```c
#define immJ()                                                  \
do {                                                            \
*imm = SEXT((BITS(i, 31, 31) << 20) | (BITS(i, 19, 12) << 12) | \
	(BITS(i, 20, 20) << 11) | (BITS(i, 30, 21) << 1),21);       \
} while (0)
```



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


1. `addi/slti` etc
`li`, 通常这个指令会被转换为`addi`以及`auipc`, 因此我们只需要实现这两个. 
我们验证一下: `0x0000_0413`恰好就是`addi`的编码, 对应寄存器`s0`就是`8`号寄存器. 因此我们先实现`addi` 即可. 

`addi`属于`I-type`. 
执行: `rs1 + (sext)imm12 -> rd`
> ADDI adds the sign-extended 12-bit immediate to register rs1. Arithmetic overflow is ignored and the result is simply the low XLEN bits of the result. 
> 
> ADDI rd, rs1, 0 is used to implement the MV rd, rs1 assembler pseudoinstruction. 
> 
> SLTI (set less than immediate) places the value 1 in register rd if register rs1 is less than the signextended immediate when both are treated as signed numbers, else 0 is written to rd.
> 
   SLTIU is similar but compares the values as unsigned numbers (i.e., the immediate is first sign-extended to XLEN bits then treated as an unsigned number). Note, SLTIU rd, rs1, 1 sets rd to 1 if rs1 equals zero, otherwise sets rd to 0 (assembler pseudoinstruction SEQZ rd, rs). 
>  
   ANDI, ORI, XORI are logical operations that perform bitwise AND, OR, and XOR on register rs1 and the sign-extended 12-bit immediate and place the result in rd. Note, XORI rd, rs1, -1 performs a bitwise logical inversion of %%  %%register rs1 (assembler pseudoinstruction NOT rd, rs).
> 
> Shifts by a constant are encoded as a specialization of the I-type format. The operand to be shifted is in rs1, and the shift amount is encoded in the lower 5 bits of the I-immediate field. The right shift type is encoded in bit 30. SLLI is a logical left shift (zeros are shifted into the lower bits); SRLI is a logical right shift (zeros are shifted into the upper bits); and SRAI is an arithmetic right shift (the original sign bit is copied into the vacated upper bits).
> 对于移位直接当作`I-type`就可以, 因为我们只关注低5位. 

> LUI (load upper immediate) is used to build 32-bit constants and uses the U-type format. LUI places the 32-bit U-immediate value into the destination register rd, filling in the lowest 12 bits with zeros.

低12位置0. 

2. `mv`这同样是一个伪指令
> ADDI rd, rs1, 0 is used to implement the MV rd, rs1 assembler pseudoinstruction.

3. `ret`是一个伪指令, 应该实现`jalr`
显然属于`J`type
执行: `rs1 + (sext)imm`

> The indirect jump instruction JALR (jump and link register) uses the I-type encoding. The target address is obtained by adding the sign-extended 12-bit I-immediate to the register rs1, then setting the least-significant bit of the result to zero. The address of the instruction following the jump (pc+4) is written to register rd. Register x0 can be used as the destination if the result is not required.

5. `lw`/`sw` etc
> Load and store instructions transfer a value between the registers and memory. 
> 
> Loads are encoded in the I-type format and stores are S-type. The effective address is obtained by adding register rs1 to the sign-extended 12-bit offset. 
> 
> Loads copy a value from memory to register rd. Stores copy the value in register rs2 to memory. 
> 
> The LW instruction loads a 32-bit value from memory into rd. LH loads a 16-bit value from memory, then sign-extends to 32-bits before storing in rd. LHU loads a 16-bit value from memory but then zero extends to 32-bits before storing in rd. LB and LBU are defined analogously for 8-bit values. The SW, SH, and SB instructions store 32-bit, 16-bit, and 8-bit values from the low bits of register rs2 to memory.

6. `beq/bne/blt` etc这其中还有伪指令`beqz`等等
>Branch instructions compare two registers. BEQ and BNE take the branch if registers rs1 and rs2 are equal or unequal respectively. BLT and BLTU take the branch if rs1 is less than rs2, using signed and unsigned comparison respectively. BGE and BGEU take the branch if rs1 is greater than or equal to rs2, using signed and unsigned comparison respectively. Note, BGT, BGTU, BLE, and BLEU can be synthesized by reversing the operands to BLT, BLTU, BGE, and BGEU, respectively

>The 12-bit B-immediate encodes signed offsets in multiples of 2 bytes. The offset is sign-extended and added to the **address of the branch instruction** to give the target address. The conditional branch range is ±4 KiB.

7. `add/sub` etc
>ADD performs the addition of rs1 and rs2. SUB performs the subtraction of rs2 from rs1. Overflows are ignored and the low XLEN bits of results are written to the destination rd. 
>
>SLT and SLTU perform signed and unsigned compares respectively, writing 1 to rd if rs1 < rs2, 0 otherwise. Note, SLTU rd, x0, rs2 sets rd to 1 if rs2 is not equal to zero, otherwise sets rd to zero (assembler pseudoinstruction SNEZ rd, rs). 
>
>AND, OR, and XOR perform bitwise logical operations. 
>
>SLL, SRL, and SRA perform logical left, logical right, and arithmetic right shifts on the value in register rs1 by the shift amount held in the lower 5 bits of register rs2

 7. `REM/MULT/DIV`
>  MUL performs an XLEN-bit×XLEN-bit multiplication of rs1 by rs2 and places the lower XLEN bits in the destination register. MULH, MULHU, and MULHSU perform the same multiplication but return the upper XLEN bits of the full 2×XLEN-bit product, for signed×signed, unsigned×unsigned, and rs1×unsigned rs2 multiplication, respectively.
> 
> If both the high and low bits of the same product are required, then the recommended code sequence is: MULH[[S]U] rdh, rs1, rs2; MUL rdl, rs1, rs2 (source register specifiers must be in same order and rdh cannot be the same as rs1 or rs2). Microarchitectures can then fuse these into a single multiply operation instead of performing two separate multiplies.
> 
> DIV and DIVU perform an XLEN bits by XLEN bits signed and unsigned integer division of rs1 by rs2, rounding towards zero. REM and REMU provide the remainder of the corresponding division operation. For REM, the sign of a nonzero result equals the sign of the dividend.


***这里也是有一个大坑呀, 只能说还好俺的基础知识比较牢固***
假如想做`64bit`乘法, 下面这么写是错的, 仔细想想
```c
  INSTPAT("0000001 ????? ????? 001 ????? 0110011", mulh, R,
R(rd) = (word_t)BITS((int64_t)src1 * (int64_t)src2, 63, 32));
```
这样写才是对的, 思考如何完成符号扩展. 
```c
  INSTPAT("0000001 ????? ????? 001 ????? 0110011", mulh, R,
R(rd) = (word_t)BITS((int64_t)(sword_t)src1 * (int64_t)(sword_t)src2, 63, 32));
```
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


-- --



## Abstract Machine

```
AM = TRM + IOE + CTE + VME + MPE
```

- TRM(Turing Machine) - 图灵机, 最简单的运行时环境, 为程序提供基本的计算能力
- IOE(I/O Extension) - 输入输出扩展, 为程序提供输出输入的能力
- CTE(Context Extension) - 上下文扩展, 为程序提供上下文管理的能力
- VME(Virtual Memory Extension) - 虚存扩展, 为程序提供虚存管理的能力
- MPE(Multi-Processor Extension) - 多处理器扩展, 为程序提供多处理器通信的能力 (MPE超出了ICS课程的范围, 在PA中不会涉及)

> `volatile` 关键字:  用于声明变量的值可能会在没有明显原因的情况下发生变化。这告诉编译器不要对这样的变量进行优化，即使编译器无法检测到该变量是如何改变的。实际上可能这个变量被硬件改变, 或者多线程中, 或者被其他某种方式改变, 不希望他被优化掉. 


### Makfile的阅读

####  `.DEFAULTGOAL/MAKECOMMANDGOALS`
这两个是`make`预定义的变量, 在没有目标的时候会默认创建前者, 当然我们可以自己指定前者, 这样就直接使用了我们制定的值. 后者就是命令行传递过来的目标. 

```makefile
ifeq ($(MAKECMDGOALS),)
  MAKECMDGOALS  = image
  .DEFAULT_GOAL = image
endif

### Override checks when `make clean/clean-all/html`
ifeq ($(findstring $(MAKECMDGOALS),clean|clean-all|html),)

```

#### `findstring`
> Searches _in_ for an occurrence of _find_. If it occurs, the value is _find_; otherwise, the value is empty. You can use this function in a conditional to test for the presence of a specific substring in a given string. Thus, the two examples,

#### `addprefix`

```makefile
OBJS      = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(SRCS))))

这里首先是每个文件的`basename + .o`, 然后再在这些文件前面加上目标地址, 这样就得到了`OBJS`的全部地址. 
```


#### 组织目标位置
这里主要是字符串处理的方法
```makefile
### Collect the files to be linked: object files (`.o`) and libraries (`.a`)
OBJS      = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(SRCS))))

LIBS     := $(sort $(LIBS) am klib) 
# lazy evaluation ("=") causes infinite recursions

LINKAGE   = $(OBJS) \
  $(addsuffix -$(ARCH).a, $(join \
    $(addsuffix /build/, $(addprefix $(AM_HOME)/, $(LIBS))), \
    $(LIBS) ))
```

### AM中调用NEMU可以直接执行而不`sdb`
我们关注这部分即可, 取决于宏`CONFIG_TARGET_AM`, 也就是说`make`的时候定义这个宏. 实际上在`menuconfig`里面也是有的, 但是上面写了`don't select`, 可能是正常`make nemu`还是不用这个宏的. 
```c
void engine_start() {
#ifdef CONFIG_TARGET_AM
  cpu_exec(-1);
#else
  /* Receive commands from user. */
  sdb_mainloop();
#endif
}
```

```makefile
### Paste in arch-specific configurations (e.g., from `scripts/x86_64-qemu.mk`)

-include $(AM_HOME)/scripts/$(ARCH).mk
```
我们看到这里包含了`ISA-PLATFORM`的配置. 

```makefile
include $(AM_HOME)/scripts/isa/riscv.mk
include $(AM_HOME)/scripts/platform/nemu.mk
CFLAGS  += -DISA_H=\"riscv/riscv.h\"
COMMON_CFLAGS += -march=rv32im_zicsr -mabi=ilp32   # overwrite
LDFLAGS       += -melf32lriscv                     # overwrite

AM_SRCS += riscv/nemu/start.S \
           riscv/nemu/cte.c \
           riscv/nemu/trap.S \
           riscv/nemu/vme.c
```
这其中还包含了`nemu`的配置, 因此如果我们希望定义上文的宏, 需要在`nemu.mk`中做文章. 

```makefile
run: image
$(MAKE) -C $(NEMU_HOME) ISA=$(ISA) run ARGS="$(NEMUFLAGS)" IMG=$(IMAGE).bin
```

***非常可惜, 我的想法是错误的***.  
> 我忽略了讲义中的<u>批处理模式</u>. 实际上经过寻找, 只需改变运行时参数即可, 并不需要改变编译参数, 并不是上面所说的宏. 而是直接传一个参数`--batch` , 下面的方式才是<u>正解</u>. 

> 至于我是怎么发现的呢? 因为我突然想到`batch`了, 在项目中我记得我见到过. 因此我在项目目录下面`grep`了一下. 
```makefile
# $(AM_HOME)/scripts/platform/nemu.mk:27

run: image
	$(MAKE) -C $(NEMU_HOME) ISA=$(ISA) run ARGS="$(NEMUFLAGS) --batch" IMG=$(IMAGE).bin
```


### 一些的其他问题
使用`bear`的时候给我报了依托`warning`. 这个问题在上游还没有解决. 我去`google`找了一下, 具体可以看: [grpc-cross-language-bug](https://github.com/grpc/grpc/issues/37178)

### 如何`make klib`
这次文档不教了, 看看`makefile`.
```makefile
 make archive ARCH=riscv32-nemu
```

### 实现`string.c`
> 这部分就当学习了, 再次像高程那样瞎几把循环是没有意义的. 因此看一看
> [glibc](https://codebrowser.dev/glibc/glibc/string/memcpy.c.html)的实现方式. 


> 我们这里约定, 实际上`glibc`也是一样的, 不能接受空指针. 

- `strcpy` 
>  `stpcpy()`: This function returns a pointer to the terminating null byte of the copied string. 这个东西会把最后的`'\0'` 拷贝过去. 

- `strncpy`
> DESCRIPTION
       These functions copy non-null bytes from the string pointed to by src into the array pointed to by dst.  
       If the source has too few non-null bytes to fill the destination, the functions pad the destination with trailing null bytes.  
       If the destination buffer, limited by its size, isn't large enough to hold the copy, the resulting character sequence is truncated.  

> RETURN VALUE
       strncpy()
              returns dst.
              
- `strcat`
 > `strcat()` These functions return dst.
 
- memcpy
> DESCRIPTION
       The memcpy() function copies n bytes from memory area src to memory area dest.  The memory areas must not overlap.  Use memmove(3) if the memory areas do overlap.
 > The memcpy() function returns a pointer to dest.
 
> 根据`glibc`, 这里的主要思想还是首先拷贝`byte`对齐, 然后按照`page`拷贝, 最后拷贝`word`. 

- `memset`
>DESCRIPTION
       The memset() function fills the first n bytes of the memory area pointed to by s with the constant byte c.
> 
> RETURN VALUE
       The memset() function returns a pointer to the memory area s.

- `memmove`
> ESCRIPTION
       The  memmove() function copies n bytes from memory area src to memory area dest.  The memory areas may overlap: copying takes place as though the bytes in src are first copied into a temporary array that does not over‐
       lap src or dest, and the bytes are then copied from the temporary array to dest.
> %%  %%
> RETURN VALUE
       The memmove() function returns a pointer to dest.

- `memcmp`
> DESCRIPTION
       The memcmp() function compares the first n bytes (each interpreted as unsigned char) of the memory areas s1 and s2.
> 
> RETURN VALUE
       The memcmp() function returns an integer less than, equal to, or greater than zero if the first n bytes of s1 is found, respectively, to be less than, to match, or be greater than the first n bytes of s2.
> 
>     For a nonzero return value, the sign is determined by the sign of the difference between the first pair of bytes (interpreted as unsigned char) that differ in s1 and s2.
> 
       If n is zero, the return value is zero.

### 实现`stdio.h`

参考这篇[文章](https://www.cprogramming.com/tutorial/c/lesson17.html)

#### `va_list`
这是一颗可以存放任意一个变量的类型. 

#### `va_start`
配合`va_list`食用. 
```c
int a_function ( int x, ... )
{
    va_list a_list;
    va_start( a_list, x );
}
```
> va_start is a macro which accepts two arguments, a va_list and the name of the variable that directly precedes the ellipsis ("...")

因此这里面使用的是`x`作为参数. 

#### `va_arg`
`double` `average (` `int` `num, ... )`
```c

double average(int const num , ...){
	va_list arguments;
	double sum = 0;

	va_start(arguments, num);
	for(int i = 0; i < num ;i++){
		sum += va_arg(arguments, double); // treat as double
	}
	va_end(arguments);//clear list
}
```

## `itrace`

### `iringbuf`

首先是实现环型缓冲区, 这里我们阅读源代码发现: 
```c
//src/cpu/cpu-exec.c

void assert_fail_msg() {
  isa_reg_display();
  statistic();
}
```

这部分会在执行失败的时候调用, 实际上也就是`Assert`失败的时候调用, 因此这也就是我们打印环形缓冲区的时机. 

然后便是`trace`的时机, 这个也比较明显, 我们找到`trace_and_difftest`. 每条指令结束之后都会执行这一句,  实际上调用这个指令的时候, 上一条指令已经执行结束并且`cpu.pc=s->dnpc`. 最新指向的是还没有执行的指令. 但这是没关系的, 因为下一步就是执行这个指令, 在这个指令执行的过程中`Assert`失败的话, 那么显然就是该指令. 因此我们把定位箭头指向缓冲区中最新的指令即可. 

> 再仔细观察, 可以发现文档中给的参考案例是16条. 一条`pc+instr`一共占用`8`字节, 然后`Decode`中的`log_buf`刚好就是`128`字节. 然而我们并不能直接用这个`log_buf`. 我们需要单独的区域来存储所有信息. 我想错了. 

这里学会善用`menuconfig`, 我们加一个选项制定`iringbuf`相关信息
```konfig
config ITRACE_RINGBUF_ON
  depends on ITRACE
  bool "Enable ringbuffer trace"
  default y
 
config ITRACE_RINGBUF_SIZE
  depends on ITRACE_RINGBUF_ON
  int "Valid when trace enabled; set the number of instruction in ringbuffer"
  default 16
```

## `mtrace`

`mtrace`的实现就更显然了, 我们只需要在访存的部分实现记录即可. 
```konfig
config MTRACE
  depends on TRACE && TARGET_NATIVE_ELF && ENGINE_INTERPRETER
  bool "Enable memory tracer"
  default n

config MTRACE_READ
  depends on MTRACE
  bool "Enable memory read tracer"
  default y

config MTRACE_WRITE
  depends on MTRACE
  bool "Enable memory write tracer"
  default y
```

## `ftrace`

> 消失的符号, 说实话这个太明显了. 只需知道预处理已经预处理掉了即可. 

### 前奏: `readelf`

`elf`符号表中的信息很有意思. 我们可以对比着来看, 既然追踪函数调用, 那么只需关注<u>符号表中</u>类型为`FUNC`的部分: 
```shell
riscv64-linux-gnu-readelf -a add-riscv32-nemu.elf | grep FUNC
```
我们得到
```
Num:    Value  Size Type    Bind   Vis      Ndx Name
 16: 80000100    32 FUNC    GLOBAL HIDDEN     1 _trm_init
 25: 80000010    24 FUNC    GLOBAL HIDDEN     1 check
 27: 80000000     0 FUNC    GLOBAL DEFAULT    1 _start
 29: 80000028   204 FUNC    GLOBAL HIDDEN     1 main
 33: 800000f4    12 FUNC    GLOBAL HIDDEN     1 halt
```
然后我们对照着反汇编得到结果, 先过滤一下得到: 
```shell
cat ./add-riscv32-nemu.txt | grep -E "^[0-9a-f]{8} <.+>:"
```
得到: 
```
80000000 <_start>:  
80000010 <check>:  
80000028 <main>:  
800000f4 <halt>:  
80000100 <_trm_init>:
```
我们发现地址都是对应的. 


但`readelf`输出的信息是已经经过解析的, 实际上符号表中`Name` 属性存放的是字符串在字符串表(`string table`)中的偏移量. 为了查看字符串表, 我们先查看`readelf`输出中`Section Headers`的信息:

```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        80000000 001000 000120 00  AX  0   0  4
  [ 2] .srodata.mainargs PROGBITS        80000120 001120 000001 00   A  0   0  4
  [ 3] .data.ans         PROGBITS        80000124 001124 000100 00  WA  0   0  4
  [ 4] .data.test_data   PROGBITS        80000224 001224 000020 00  WA  0   0  4
  [ 5] .comment          PROGBITS        00000000 001244 000012 01  MS  0   0  1
  [ 6] .riscv.attributes RISCV_ATTRIBUTE 00000000 001256 000033 00      0   0  1
  [ 7] .symtab           SYMTAB          00000000 00128c 000230 10      8  16  4
  [ 8] .strtab           STRTAB          00000000 0014bc 0000c5 00      0   0  1
  [ 9] .shstrtab         STRTAB          00000000 001581 000068 00      0   0  1
Key to Flags:
  W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
  L (link order), O (extra OS processing required), G (group), T (TLS),
  C (compressed), x (unknown), o (OS specific), E (exclude),
  D (mbind), p (processor specific)
```
我们希望找的是字符串表`strtab`. 

我们使用`xxd`查看对应的`elf`文件. 
![](assets/Pasted%20image%2020240727150942.png)
观察到`0x14bc`开始对应的就是字符串.  

> 符号表中的偏移量, 记录的是

> 有一个篮框的`hello world`, 这里竟然也踩到一个坑. `readelf`自动截断, 需要加上`-W`参数.[参考](https://stackoverflow.com/questions/15362297/why-i-am-getting-this-output-when-run-readelf-s)
> 其实已经猜到了, 在`readelf`读符号表头的时候, 得到偏移量`0x3038`, 然后进去找, 找到的都是函数相关的符号字符串. 并没有找到`hello world`. 我想应该是放在了静态存储区中
```
00002000: 0100 0200 4865 6c6c 6f20 576f 726c 6400  ....Hello World.
```
实际上只需要关注符号表: 
```
[16] .rodata        PROGBITS        0000000000002000 002000 000010 00   A  0   0  4
```
这是`readonly data`. 恰好就是`Hello World`放的地方. 


### 实现`ftrace`

#### 参数解析
```
getopt_long() and getopt_long_only()
       The getopt_long() function works like getopt() except that it also accepts long options, started with two dashes.  (If the program accepts only long options, then optstring should be specified as an empty string  (""), not NULL.)  Long option naimes may be abbreviated if the abbreviation is unique or is an exact match for some defined option.  A long option may take a parameter, of the form --arg=param or --arg param.

optstring is a string containing the legitimate option characters.  A legitimate option character is any visible one byte ascii(7) character (for which isgraph(3) would return nonzero) that is not '-', ':', or ';'.  

If such a character is followed by a colon, the option requires an argument(注意这里), so getopt() places a pointer to the following text in the same argv-element, or the text of the following argv-element, in optarg. 
```

我们可以参照`makefile`中的用法
```makefile
IMG ?=
NEMU_EXEC := $(BINARY) $(ARGS) $(IMG)
```
以及
```makefile
NEMUFLAGS += -l $(shell dirname $(IMAGE).elf)/nemu-log.txt 
```
说实话找了很多文档我没找到那个`case 1`是怎么来的, 什么时候会返回`1`.  

但是应该不影响我们实现`--elf`选项. 这个同理于前面几个选项. 我们需要配合更改`makefile`传递这个参数. 

```makefile
# $AM_HOME/scripts/platform/nemu.mk

run: image
	$(MAKE) -C $(NEMU_HOME) ISA=$(ISA) run ARGS="$(NEMUFLAGS) --batch 
	-e $(IMAGE).elf" IMG=$(IMAGE).bin
```

剩下的工作就是读这个文件了, 参数解析完毕. 


#### 解析`elf`
首先观察`readelf`读的符号表
```
Symbol table '.symtab' contains 24 entries:  
  Num:    Value          Size Type    Bind   Vis      Ndx Name  
	...
    8: 0000000000004008     0 NOTYPE  WEAK   DEFAULT   24 data_start  
    9: 0000000000000000     0 FUNC    GLOBAL DEFAULT  UND puts@GLIBC_2.2.5  
   10: 0000000000004018     0 NOTYPE  GLOBAL DEFAULT   24 _edata  
   11: 0000000000001154     0 FUNC    GLOBAL HIDDEN    15 _fini  
   12: 0000000000004008     0 NOTYPE  GLOBAL DEFAULT   24 __data_start  
   13: 0000000000000000     0 NOTYPE  WEAK   DEFAULT  UND __gmon_start__  
   14: 0000000000004010     0 OBJECT  GLOBAL HIDDEN    24 __dso_handle  
   15: 0000000000002000     4 OBJECT  GLOBAL DEFAULT   16 _IO_stdin_used  
   16: 0000000000004020     0 NOTYPE  GLOBAL DEFAULT   25 _end  
   17: 0000000000001040    38 FUNC    GLOBAL DEFAULT   14 _start  
   18: 0000000000004018     0 NOTYPE  GLOBAL DEFAULT   25 __bss_start  
   19: 0000000000001139    26 FUNC    GLOBAL DEFAULT   14 main  
   20: 0000000000004018     0 OBJECT  GLOBAL HIDDEN    24 __TMC_END__  
```

我们参考[解析elf](https://stackoverflow.com/questions/23809102/print-the-symbol-table-of-an-elf-file), 但是这个使用了`libelf`, 我还是自己写吧. 

```c
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>

int main(int argc, char **argv)
{
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    Elf_Data    *data;
    int         fd, ii, count;

    elf_version(EV_CURRENT);

    fd = open(argv[1], O_RDONLY);
    elf = elf_begin(fd, ELF_C_READ, NULL);

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        gelf_getshdr(scn, &shdr);
        if (shdr.sh_type == SHT_SYMTAB) {
            /* found a symbol table, go print it. */
            break;
        }
    }

    data = elf_getdata(scn, NULL);
    count = shdr.sh_size / shdr.sh_entsize;

    /* print the symbol names */
    for (ii = 0; ii < count; ++ii) {
        GElf_Sym sym;
        gelf_getsym(data, ii, &sym);
        printf("%s\n", elf_strptr(elf, shdr.sh_link, sym.st_name));
    }
    elf_end(elf);
    close(fd);
}
```

首先关注一个标准的`elf`头:
![](assets/Pasted%20image%2020240727173729.png)
我们希望得到`section`的信息, 这里可以直接从`elf`头读出. 根据文档, <u>如果没有section header, 那么偏移量就是0</u>. 
因此第一步就是解析elf头

解析来解析`section header`. 
- `number of section headers`: 这个说的就是`section header`一共有几条
- `section header string table index`: 这个有点意思, 意思就是`string table`在`section header`中的索引
- `size of section header`, 这个容易引起歧义:
```
 e_shentsize: This member holds a sections header's size in bytes.  A section header is one entry in the section header table; all entries are the same size.
```
> 实际上一个`section header` 大小是图片的`64`, 而一个`section header table`是由多个`header`构成的. 
![](assets/Pasted%20image%2020240727181254.png)
比如这个图是`32-bit`的, 那么刚好这里`40`就是我们结构体的大小. 

<u>这里也是吃了个大亏,  没有好好看手册, 这里得到`info`是不能直接得到的, 需要宏来解绑. </u>
```c
//There are macros for packing and unpacking the binding and type fields:

              ELF32_ST_BIND(info)
              ELF64_ST_BIND(info)
                     Extract a binding from an st_info value.

              ELF32_ST_TYPE(info)
              ELF64_ST_TYPE(info)
                     Extract a type from an st_info value.

              ELF32_ST_INFO(bind, type)
              ELF64_ST_INFO(bind, type)
                     Convert a binding and a type into an st_info value.

```


### `function`对应`name`
> 思路如下: 
- 如果给定一个地址, 那么我们就扫描整个符号表中所有`Type=FUNC`的条目.
- 检查这个地址是否处于`[Value, Value + Size)`这个区间里面, 如果是的, 说明就是这个函数. 



#### `menuconfig`
```konfig
config FTRACE
  depends on TRACE && TARGET_NATIVE_ELF && ENGINE_INTERPRETER
  bool "Enable function call tracer"
  default y
```
#### 传递给`nemu`一个`elf`文件
```
```

