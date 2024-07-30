
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

 8. `REM/MULT/DIV`
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

-- --
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

-- -- 
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

-- --
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

-- --
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

-- --
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

#### 参数解析+传递`ELF`文件
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

如何区分`call`和`ret`, 其实这个比较容易, 我们只需要知道, 这两个都是伪指令. 
[参考](https://projectf.io/posts/riscv-jump-function/)
![](assets/Pasted%20image%2020240728110859.png)
![](assets/Pasted%20image%2020240728102854.png)

对于`call`来说, 前面一定会有一个`auipc`, 那么就显然了.  这不对,  因为这个是大跳, 一个function还可以通过小跳来`call`. 因此这里需要读`RISCV32-ABI`.

```asm
jal  rd, imm       # rd = pc+4; pc += imm
jalr rd, rs1, imm  # rd = pc+4; pc = rs1+imm
```

> In the above example we used **jal** to call our function, but it’s limited to `±1MiB` relative to the PC. For far calls we can combine **jalr** with [auipc](https://projectf.io/posts/riscv-branch-set#auipc) to reach anywhere in 32-bit memory space. Use the **call** pseudoinstruction and the assembler will choose the correct instruction(s) for you.

> 这里的`x1`就是1号寄存器,根据`abi`规定, 就是函数调用的约定. 因此但凡`jal`放到`ra`里面的就是调用. 因此`call`的时候我们区分大小跳. 

> 对于`ret`来说就是考虑`zero`和`ra`. 那么`imm=0, rs1 = ra, rd = zero`

![](assets/Pasted%20image%2020240728141531.png)
这个问题其实也很显然, 考察返回的时候的地址, 假如我在`main`里边调用了`add`, 那么返回的时候这个地址实际上就是调用时候的地址, 也就是说这个地址是在`main`里边的, 所以说`ret main`而不是`ret add`. 

#### `menuconfig`
```konfig
config FTRACE
  depends on TRACE && TARGET_NATIVE_ELF && ENGINE_INTERPRETER
  bool "Enable function call tracer"
  default y

config FTRACE_FUNC_NAME_LIMIT
  depends on FTRACE
  int "Support function name max length"
  default 64

config FTRACE_FUNC_MAX_NUM
  depends on FTRACE
  int "Support how many functions"
  default 128
```

这个感觉还是非常`nice`的
```
[ITRACE]: recent instruction below
         0x80000230: 00 00 07 97 auipc  a5, 0x0
         0x80000234: 07 47 a7 83 lw     a5, 0x74(a5)
         0x80000238: 40 f5 05 33 sub    a0, a0, a5
         0x8000023c: 00 15 35 13 sltiu  a0, a0, 0x1
         0x80000240: f8 1f f0 ef jal    ra, 0x800001c0
         0x800001c0: 00 05 04 63 beq    a0, zero, 0x800001c8
         0x800001c4: 00 00 80 67 jalr   zero, 0x0(ra)
         0x80000244: 00 c1 20 83 lw     ra, 0xc(sp)
         0x80000248: 00 81 24 03 lw     s0, 0x8(sp)
         0x8000024c: 00 00 05 13 addi   a0, zero, 0x0
         0x80000250: 01 01 01 13 addi   sp, sp, 0x10
         0x80000254: 00 00 80 67 jalr   zero, 0x0(ra)
         0x80000278: 00 05 05 13 addi   a0, a0, 0x0
  -->    0x8000027c: 00 10 00 73 ebreak
         0x800001c4: 00 00 80 67 jalr   zero, 0x0(ra)
         0x8000022c: 00 84 25 03 lw     a0, 0x8(s0)
[FTRACE]: recent instruction below8000000c  call [_trm_init@80000264]
80000274   call [main@800001d8]
800001f8    call [f0@80000010]
80000178     call [f2@800000b0]
800000fc      call [f1@80000068]
80000178       call [f2@800000b0]
800000fc        call [f1@80000068]
80000178         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
80000190         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
800001b8         ret [f2]
8000010c        ret [f3]
80000190       call [f2@800000b0]
800000fc        call [f1@80000068]
80000178         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
80000190         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
800001b8         ret [f2]
8000010c        ret [f3]
800001b8       ret [f2]
8000010c      ret [f3]
80000190     call [f2@800000b0]
800000fc      call [f1@80000068]
80000178       call [f2@800000b0]
800000fc        call [f1@80000068]
80000178         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
80000190         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
800001b8         ret [f2]
8000010c        ret [f3]
80000190       call [f2@800000b0]
800000fc        call [f1@80000068]
80000178         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
80000190         call [f2@800000b0]
800000fc          call [f1@80000068]
80000178           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
80000190           call [f2@800000b0]
800000fc            call [f1@80000068]
80000178             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
80000190             call [f2@800000b0]
800000fc              call [f1@80000068]
80000064               ret [f2]
8000010c              ret [f3]
800001b8             ret [f2]
8000010c            ret [f3]
800001b8           ret [f2]
8000010c          ret [f3]
800001b8         ret [f2]
8000010c        ret [f3]
800001b8       ret [f2]
8000010c      ret [f3]
800001b8     ret [main]
80000210    call [check@800001c0]
800001c4     ret [main]
80000228    call [check@800001c0]
800001c4     ret [main]
80000240    call [check@800001c0]
800001c4     ret [main]
80000254    ret [_trm_init]
test list [1 item(s)]: recursion
[     recursion] PASS
```

### 符号表

![](assets/Pasted%20image%2020240728142500.png)
可执行文件中删除符号表, 不影响运行

如果编译过程中文件
```shell
gcc -c hello.c
strip -s hello.o

gcc -o hello hello.c
```

```
/usr/bin/ld: error in hello.o(.eh_frame); no .eh_frame_hdr table will be created  
/usr/bin/ld: /usr/lib/gcc/x86_64-pc-linux-gnu/14.1.1/../../../../lib/Scrt1.o: in function `_start':  
(.text+0x1b): undefined reference to `main'  
collect2: error: ld returned 1 exit status
```
这是因为, 符号表在链接过程中是给`ld`用的, 没有符号表, `ld`就不知道如何链接. 前者是`gcc`已经完成了编译链接的过程, 因此不需要符号表. 


## `trace`的一些优点

还可以指导开发者进行程序和系统的优化, 例如:
- 可以基于`ftrace`进一步分析出调用`memcpy()`时的参数情况, 比如`dest`和`src`是否对齐, 拷贝的内存长度是长还是短, 然后根据频繁出现的组合对`memcpy()`的算法实现进行优化
- 可以基于`ftrace`统计函数调用的次数, 对访问次数较多的函数进行优化, 可以显著提升程序的性能
- 可以基于`itrace`过滤出分支跳转指令的执行情况, 作为分支预测器(现代处理器中的一个提升性能的部件)的输入, 来调整分支预测器的实现, 从而提升处理器的性能
- 可以基于`mtrace`得到程序的访存序列, 作为缓存(现代处理器中的另一个提升性能的部件)模型的输入, 对预取算法和替换算法的优化进行指导(你将会在`Lab4`中体会这一点)

-- -- 

## `klib`

![](assets/Pasted%20image%2020240728155210.png)
这个宏决定了`klib`库的行为. 

> 为什么定义宏`__NATIVE_USE_KLIB__`之后就可以把`native`上的这些库函数链接到klib? 这具体是如何发生的? 尝试根据你在课堂上学习的链接相关的知识解释这一现象.

```makefile
echo "g++ -pie -o $(IMAGE) -Wl,--whole-archive $(LINKAGE) -Wl,-no-whole-archive $(LDFLAGS_CXX) -lSDL2 -ldl"
```

我们观测一下`LDFLAGS_CXX`

```shell
g++ 
-pie 
-o /home/scarlet-arch/Projects/cplusplus/NJU/PA/ics2023/am-kernels/tests/cpu-tests/build/string-native 
-Wl,cat--whole-archive /home/scarlet-arch/Projects/cplusplus/NJU/PA/ics2023/am-kernels/tests/cpu-tests/build/native/tests/string.o /home/scarletarch/Projects/cplusplus/NJU/PA/ics2023/abstract-machine/am/build/am-native.a 
/home/scarlet-arch/Projects/cplusplus/NJU/PA/ics2023/abstract-machine/klib/build/klib-native.a 
-Wl,-no-whole-archive 
-Wl,-z 
-Wl,noexecstack -lSDL2 -ldl
```

```
Wl,option
Pass  option  as  an  option to the linker.  If option contains commas, it is split into multiple options at the commas.  You can use this syntax to pass an argument to the option.  

For example, -Wl,-Map,output.map passes -Map output.map to the linker.  When using the GNU linker, you can also get the same effect with -Wl,-Map=output.map.
```

至于链接到标准库, 我想是`linkder`的原因. 我们可以用下面的程序验证: 
```c
#include <stdio.h>

char *strcpy (char *__restrict __dest, const char *__restrict __src){
  printf("T you\n");
  return NULL;
}

int main(){
  strcpy(NULL, NULL);
  //printf("Hello world\n");
}
```

> 在`gcc`下很奇怪的把`strcpy`的调用给优化掉了, 哪怕`O0`也不行. `clang`没有问题, 

 
这里优先使用程序内部的, 如果这里没有, 才去链接标准库. 

## Difftest

```c
// 在DUT host memory的`buf`和REF guest memory的`addr`之间拷贝`n`字节,
// `direction`指定拷贝的方向, `DIFFTEST_TO_DUT`表示往DUT拷贝, `DIFFTEST_TO_REF`表示往REF拷贝
void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction);

// `direction`为`DIFFTEST_TO_DUT`时, 获取REF的寄存器状态到`dut`;
// `direction`为`DIFFTEST_TO_REF`时, 设置REF的寄存器状态为`dut`;
void difftest_regcpy(void *dut, bool direction);

// 让REF执行`n`条指令
void difftest_exec(uint64_t n);

// 初始化REF的DiffTest功能
void difftest_init();
```

定位`isa_difftest_checkregs`

![](assets/Pasted%20image%2020240728191457.png)

![](assets/Pasted%20image%2020240728191551.png)

实际上上面那个调用那是个函数指针, 看对应文件就可以发现. 

![](assets/Pasted%20image%2020240728191759.png)

> 有报错是因为我没有更新`compile_commands.json`. 更新之后就好了
![](assets/Pasted%20image%2020240728191942.png)

实际上这应该就是我们的原本的顺序. 

![](assets/Pasted%20image%2020240728192721.png)
我们比较的是这两个东西. 

-- -- 
## TODO: 捕捉死循环

> 初步想法, 就是如果在某一个地方jump来jump去, 也就是对`jump`相关命令进行跟踪, 如果发现有大量重复的地址, 那么就是死循环. 当然要设置一个界限. 


-- --

## IO

### 编址

给设备也像寄存器一样编号, `CPU`访问他们. 

### 端口IO

> 一种I/O编址方式是端口映射I/O(port-mapped I/O), CPU使用专门的I/O指令对设备进行访问, 并把设备的地址称作端口号.

这种情况使用专门的IO召集令. 

x86提供了`in`和`out`指令用于访问设备, 其中`in`指令用于将设备寄存器中的数据传输到CPU寄存器中, `out`指令用于将CPU寄存器中的数据传送到设备寄存器中. 一个例子是使用`out`指令给串口发送命令字:

```asm
movl $0x41, %al
movl $0x3f8, %edx
outb %al, (%dx)
```


> 但是这种方法不现实, 因为一旦设计了IO指令, 那么访问的设备的地址就固定了下来. 

### 内存映射IO

> 随着设备越来越多, 功能也越来越复杂, I/O地址空间有限的端口映射I/O已经逐渐不能满足需求了. 有的设备需要让CPU访问一段较大的连续存储空间, 如VGA的显存, 24色加上Alpha通道的1024x768分辨率的显存就需要3MB的编址范围. 于是内存映射I/O(memory-mapped I/O, MMIO)应运而生.

内存映射I/O成为了现代计算机主流的I/O编址方式: RISC架构只提供内存映射I/O的编址方式, 而PCI-e, 网卡, x86的APIC等主流设备, 都支持通过内存映射I/O来访问.

> volatile关键字

```c
void fun() {  
 extern unsigned char _end;  // _end是什么?  
 volatile unsigned char *p = &_end;  
 *p = 0;  
 while(*p != 0xff);  
 *p = 0x33;  
 *p = 0x34;  
 *p = 0x86;  
}
```

```asm
0000000000001130 <fun>:  
   1130:       c6 05 e1 2e 00 00 00    movb   $0x0,0x2ee1(%rip)  # 4018 <_end>  
   1137:       eb fe                   jmp    1137 <fun+0x7>
```

```asm
0000000000001150 <fun>:  
   1150:       c6 05 c1 2e 00 00 00    movb   $0x0,0x2ec1(%rip)  # 4018 <_end>  
   1157:       48 8d 15 ba 2e 00 00    lea    0x2eba(%rip),%rdx  # 4018 <_end>  
   115e:       66 90                   xchg   %ax,%ax  
   1160:       0f b6 02                movzbl (%rdx),%eax  
   1163:       3c ff                   cmp    $0xff,%al  
   1165:       75 f9                   jne    1160 <fun+0x10>  
   1167:       c6 05 aa 2e 00 00 33    movb   $0x33,0x2eaa(%rip) # 4018 <_end>  
   116e:       c6 05 a3 2e 00 00 34    movb   $0x34,0x2ea3(%rip) # 4018 <_end>  
   1175:       c6 05 9c 2e 00 00 86    movb   $0x86,0x2e9c(%rip) # 4018 <_end>  
   117c:       c3                      ret
```


### `map_read/write`

```c
word_t map_read(paddr_t addr, int len, IOMap *map) {
  assert(len >= 1 && len <= 8);
  check_bound(map, addr);
  paddr_t offset = addr - map->low;
  invoke_callback(map->callback, offset, len, false); // prepare data to read
  word_t ret = host_read(map->space + offset, len);
  return ret;
}
```

其中`map_read()`和`map_write()`用于将地址`addr`映射到`map`所指示的目标空间, 并进行访问. 访问时, 可能会触发相应的回调函数, 对设备和目标空间的状态进行更新. 由于NEMU是单线程程序, 因此只能串行模拟整个计算机系统的工作, 每次进行I/O读写的时候, 才会调用设备提供的回调函数(callback). 基于这两个API, 我们就可以很容易实现端口映射I/O和内存映射I/O的模拟了.

```c
void paddr_write(paddr_t addr, int len, word_t data) {
  if (likely(in_pmem(addr))) {
#ifdef CONFIG_MTRACE_WRITE
    Log("\twrite paddr:\t%x[%d]\twrite data:\t" FMT_WORD, (uint32_t)addr, len, data);
#endif
    pmem_write(addr, len, data);
    return;
  }
  IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); return);
  out_of_bound(addr);
}
```

```c
void mmio_write(paddr_t addr, int len, word_t data) {
  map_write(addr, len, data, fetch_mmio_map(addr));
}
```
这里`fetch_...`找不到的时候返回`NULL`. 

```c
static void check_bound(IOMap *map, paddr_t addr) {
  if (map == NULL) {
    Assert(map != NULL, "address (" FMT_PADDR ") is out of bound at pc = " FMT_WORD, addr, cpu.pc);
  } else {
    Assert(addr <= map->high && addr >= map->low,
        "address (" FMT_PADDR ") is out of bound {%s} [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
        addr, map->name, map->low, map->high, cpu.pc);
  }
}
```
下面`check_bound`会进行检测, 保证在范围内. 

注意一点, `nemu`和`ref`在面对IO的时候行为不一定一样. 
```c
static inline int find_mapid_by_addr(IOMap *maps, int size, paddr_t addr) {
  int i;
  for (i = 0; i < size; i ++) {
    if (map_inside(maps + i, addr)) {
      difftest_skip_ref();
      return i;
    }
  }
  return -1;
}
```

> 传递参数: `mainargs`. 这个问题在`AM`的platform相关的部分, 给`make`传递参数, 制定`ARS`. `nemu`中的`makefile`会利用这个参数运行. 
```
run: image
	$(MAKE) -C $(NEMU_HOME) ISA=$(ISA) run ARGS="$(NEMUFLAGS) --batch -e $(IMAGE).elf" IMG=$(IMAGE).bin
```
如果是`native`, 那么如何输出的`mainargs`呢?

这里我们发现`hello`的`main`比较奇怪, 并不是真正的`main`, 他只有`mainargs`而没有`argc`参数. 我们找到被调用的地方. 

```c
  const char *args = getenv("mainargs");
  halt(main(args ? args : "")); // call main here!
```
其中`getenv()`是一个库函数. 
>   getenv, secure_getenv - get an environment variable

那这样其实就有说法了. 我们可以
```shell
export mainargs='son of bitch'
make ARCH=native run
```
得到输出
```
# Building hello-run [native]
# Building am-archive [native]
# Building klib-archive [native]
# Creating image [native]
+ LD -> build/hello-native
/home/scarlet-arch/Projects/cplusplus/NJU/PA/ics2023/am-kernels/kernels/hello/build/hello-native
Hello, AbstractMachine!
mainargs = 'son of bitch'.
Exit code = 00h
```

-- --
## 实现`printf`

> In C++, overflow of signed integers results in undefined behaviour (UB), whereas **overflow of unsigned integers is defined**

我们设计一个`printf_base`, 然后使用一个函数指针`string_handler_t`处理输出. 

```c
typedef void (*string_handler_t)(char *, char const *, size_t);

static inline void str_write_to_buffer(char *out, char const *in, size_t size) {
  assert(out);
  assert(in);
  memcpy(out, in, size);
}

static inline void str_write_to_stdout(char *out, char const *in, size_t size) {
  assert(out == NULL); // out is stdout
  assert(in);
  size_t cnt = 0;
  while (*in && cnt++ < size)
    putch(*in++);
}

```

为基类设计如下签名: 
```c
int printf_base(char *out, char const *fmt, size_t const n,
                string_handler_t shandler, va_list ap); 
```

-- --
## 实现`__am_timer_update`

我们参照`native`的实现

```c
//native 
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  struct timeval now;
  gettimeofday(&now, NULL);
  long seconds = now.tv_sec - boot_time.tv_sec;
  long useconds = now.tv_usec - boot_time.tv_usec;
  uptime->us = seconds * 1000000 + (useconds + 500);
}

void __am_timer_init() {
  gettimeofday(&boot_time, NULL);
}
```

但是我们获得系统时钟的方式不能是`gettimeofday`, 而是来源于我们的`nemu`. 

```c
static uint32_t *rtc_port_base = NULL;

static void rtc_io_handler(uint32_t offset, int len, bool is_write) {
  assert(offset == 0 || offset == 4);
  if (!is_write && offset == 4) {
    uint64_t us = get_time();
    rtc_port_base[0] = (uint32_t)us;
    rtc_port_base[1] = us >> 32;
  }
}
```
上文是`nemu`写入时间的端口.

```c
void init_timer() {
  rtc_port_base = (uint32_t *)new_space(8);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("rtc", CONFIG_RTC_PORT, rtc_port_base, 8, rtc_io_handler);
#else
  add_mmio_map("rtc", CONFIG_RTC_MMIO, rtc_port_base, 8, rtc_io_handler);
#endif
  IFNDEF(CONFIG_TARGET_AM, add_alarm_handle(timer_intr));
}
```
接下来看`nemu`初始化时钟的方式. 
```c
void add_mmio_map(const char *name, paddr_t addr, void *space, uint32_t len, io_callback_t callback) {
  assert(nr_map < NR_MAP);
  paddr_t left = addr, right = addr + len - 1;
  if (in_pmem(left) || in_pmem(right)) {
    report_mmio_overlap(name, left, right, "pmem", PMEM_LEFT, PMEM_RIGHT);
  }// 这里是区间检测
  for (int i = 0; i < nr_map; i++) {
    if (left <= maps[i].high && right >= maps[i].low) {
      report_mmio_overlap(name, left, right, maps[i].name, maps[i].low, maps[i].high);//这里是其他外设检测
    }
  }

  maps[nr_map] = (IOMap){ .name = name, .low = addr, .high = addr + len - 1,
    .space = space, .callback = callback };//加入map
  Log("Add mmio map '%s' at [" FMT_PADDR ", " FMT_PADDR "]",
      maps[nr_map].name, maps[nr_map].low, maps[nr_map].high);

  nr_map ++;
}
```

所以我们要想办法从`nemu`中读上述的时间. 
这个时候再关注`nemu.h`, 提供了相对的地址. 这里有一点很有意思, 有一个
```c
extern char _pmem_start
```
但是我们找不到这个符号. 因为这个符号是传递给`ld`的, 给`ld`传的时候指定了这个符号的地址. 
```makefile
LDFLAGS   += -T $(AM_HOME)/scripts/linker.ld \
             --defsym=_pmem_start=0x80000000 --defsym=_entry_offset=0x0
```

当然我们具体实现呢, 是不依赖这个`_pmem_start`, 因为我们这里找的是裸机的地址. <u>注意此时的抽象层次</u>. 
```c
static AM_TIMER_UPTIME_T boot_time = {};

void __am_timer_init() {
  uint64_t const _read_time = *(uint64_t*)RTC_ADDR;
  boot_time.us = _read_time;
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uint64_t const _read_time = *(uint64_t*)RTC_ADDR;
  uptime->us = _read_time - boot_time.us; 
}
```

-- -- 
## 测测你的🐎

第一次出现了一些问题, 忘记把`device`打开了, 因此出现地址出错. 

> 然后就遇到留得坑了, 这个跑分显示不正常. 
![](assets/Pasted%20image%2020240729195907.png)

> 关于`AM_TIMER_UPTIME`的实现, 我们在框架代码中埋了一些小坑, 如果你没有修复相关的问题, 你可能会在运行benchmark的时候出现跑分不正确的现象. 这是为了强迫大家认真RTFSC了解程序运行过程中的一切细节: benchmark读取时钟信息的时候, 整个计算机系统究竟发生了什么? 只有这样你才能把时钟相关的bug调试正确.


我们首先理一下思路, 在`AM`层面编写的程序, 这实际上编译出来就是访存裸指令.
```c
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uint64_t const _read_time = *(uint64_t*)RTC_ADDR;
  uptime->us = _read_time - boot_time.us; 
}
```
这部分的机器指令在`nemu`中执行. 然后每次在`nemu`中访问外设会呼叫`map_read`(访存失败的`fallback`), 在这里边会调用回调函数`callback`, `RTC`的回调就是更新时间. 所以说当我们读的时候, 就更新时间. 看起来没毛病. 

回调内部, 如果偏移量是`4`, 也就是访问高字节(`uint64_t`分两部分访存)的时候, 我们才更新时间. 
```c
  if (!is_write && offset == 4) { // any time here when read update
    uint64_t us = get_time();
    rtc_port_base[0] = (uint32_t)us;
    rtc_port_base[1] = us >> 32;
    //Log("[UPDATE] update RTC %s %lu", is_write ? "W" : "R", us);
  }
```

前边的都没什么问题. 最后把`uptime`改为绝对时间, 才可以`bench`. 也就是我们并不需要记录一个`boot_time`. 

我们回顾`rtc_io_handler`的实现, 其中有一个`get_time()`然后设置为当前时间: 
```c
    uint64_t us = get_time();
```

这里的`get_time()`并不是我想当然的某个库函数. 
```c
uint64_t get_time() {
  if (boot_time == 0) boot_time = get_time_internal();
  uint64_t now = get_time_internal();
  return now - boot_time;
}
```
内部的`get_time_internal()`如下. 
```c
static uint64_t get_time_internal() {
#if defined(CONFIG_TARGET_AM)
  uint64_t us = io_read(AM_TIMER_UPTIME).us;
#elif defined(CONFIG_TIMER_GETTIMEOFDAY)
  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t us = now.tv_sec * 1000000 + now.tv_usec;
#else
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
  uint64_t us = now.tv_sec * 1000000 + now.tv_nsec / 1000;
#endif
  return us;
}
```
所以第一次调用`get_time()`开始(这个时候第一次调用就是`boot_time`)
实际上这个第一次调用出现在`cpu_exec`函数中. 也就是`cpu`启动的时候. 

之后`get_time()`返回的就是距离`cpu`启动发生了多长时间, 也就是说这个已经是一个时间段而不是时间点了, 所以我们在`am`里边使用`boot_time`就是错误的, 所以导致跑分异常高. 

> 我想这就是这个坑吧. 的确要读很多很多`Fucking source code`. 

最后测完记录一下:
![](assets/Pasted%20image%2020240730000137.png)

## Run something cool
![](assets/Pasted%20image%2020240730112656.png)

![](assets/Pasted%20image%2020240730112850.png)
`nemu`没有实现声卡, 所以没有声音
![](assets/Pasted%20image%2020240730113034.png)

-- -- 
## `DTRACE`
这是显然的, 我们只需要在`map_read/write`中记录即可. 

-- -- 
## `keyboard`
这里阅读源代码时有些宏过于复杂, 我们对其展开.
```shell
gcc -E keyboard.c -I $NEMU_HOME/include -I $NEMU_HOME/src/device/ | less
```
右边的是枚举类型, 左边是把左边数组填充对应的枚举值. 这样的简化的确非常有效. 
```c
static void init_keymap() {
  keymap[SDL_SCANCODE_ESCAPE] = NEMU_KEY_ESCAPE; 
  keymap[SDL_SCANCODE_F1] = NEMU_KEY_F1; 
  keymap[SDL_SCANCODE_F2] = NEMU_KEY_F2; 
  keymap[SDL_SCANCODE_F3] = NEMU_KEY_F3; 
  keymap[SDL_SCANCODE_F4] = NEMU_KEY_F4; 
  keymap[SDL_SCANCODE_F5] = NEMU_KEY_F5; 
  keymap[SDL_SCANCODE_F6] = NEMU_KEY_F6; 
  keymap[SDL_SCANCODE_F7] = NEMU_KEY_F7; 
  ...
  keymap[SDL_SCANCODE_DELETE] = NEMU_KEY_DELETE; 
  keymap[SDL_SCANCODE_HOME] = NEMU_KEY_HOME; 
  keymap[SDL_SCANCODE_END] = NEMU_KEY_END; 
  keymap[SDL_SCANCODE_PAGEUP] = NEMU_KEY_PAGEUP; 
  keymap[SDL_SCANCODE_PAGEDOWN] = NEMU_KEY_PAGEDOWN;
}

enum {
  NEMU_KEY_NONE = 0,
  NEMU_KEY_ESCAPE, NEMU_KEY_F1, NEMU_KEY_F2, NEMU_KEY_F3, NEMU_KEY_F4, 
  ... 
  NEMU_KEY_PAGEUP, NEMU_KEY_PAGEDOWN,
};
```

展开的是这部分
```c
enum {
  NEMU_KEY_NONE = 0,
  MAP(NEMU_KEYS, NEMU_KEY_NAME)%%  %%
};

#define SDL_KEYMAP(k) keymap[SDL_SCANCODE_ ## k] = NEMU_KEY_ ## k;
static uint32_t keymap[256] = {};

static void init_keymap() {
  MAP(NEMU_KEYS, SDL_KEYMAP)
}
```
同时注意, 发送`keycode`的时候: 
```c
void send_key(uint8_t scancode, bool is_keydown) {
  if (nemu_state.state == NEMU_RUNNING && keymap[scancode] != NEMU_KEY_NONE) {
    uint32_t am_scancode = keymap[scancode] | (is_keydown ? KEYDOWN_MASK : 0);
    key_enqueue(am_scancode);
  }// 如果是down, 那么要或上一个mask, 所以我们只需要与这个mask, 看看有没有这
  // bit = 1, 就可以判断是松开还是按下. 
}
```
于是我们容易得到: 
```c
void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  // keydown = true: it is pressed
  uint32_t* const key_reg_addr = (uint32_t*)KBD_ADDR;// 4 bytes
  uint32_t const keycode = *key_reg_addr;
  kbd->keydown = !!(keycode & KEYDOWN_MASK);
  kbd->keycode = keycode & ~KEYDOWN_MASK; // set KEYDOWN_MASK bit zero
}
```

-- -- 
## `VGA`

这里提到了调色板, 这其实就是当年SJ的`OOP`的`bitmap`的某一次作业...

> 具体地, 屏幕大小寄存器的硬件(NEMU)功能已经实现, 但软件(AM)还没有去使用它; 而对于同步寄存器则相反, 软件(AM)已经实现了同步屏幕的功能, 但硬件(NEMU)尚未添加相应的支持.

### 屏幕大小寄存器

这部分关注`nemu`的代码
```c
void init_vga() {
  vgactl_port_base = (uint32_t *)new_space(8);
  vgactl_port_base[0] = (screen_width() << 16) | screen_height();
 ... 
}

static uint32_t screen_size() {
  return screen_width() * screen_height() * sizeof(uint32_t);
}
```
在`init_vga`中, 这里的`vgactl_port_base`是一个`8 byte`的空间, 其中`0`号写入的是长宽, 分别占两个字节. 

这里对应硬件其实还没设那么多门槛, 直接补充函数就行, 我还以为要自己写函数. 

### 同步寄存器

先看一眼`am`的实现
```c
#define SYNC_ADDR (VGACTL_ADDR + 4)

void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }// 一眼丁真, 这里显然使用了之前没有使用过的高4 byte
}
```

> 补充: `uintptr_t` might be the same size as a `void*`. It might be larger. It could conceivably be smaller, although such a C++ implementation approaches perverse. For example on some hypothetical platform where `void*` is 32 bits, but only 24 bits of virtual address space are used, you could have a 24-bit `uintptr_t` which satisfies the requirement. I don't know why an implementation would do that, but the standard permits it. \

下面是一个正确的`native`
![](assets/Pasted%20image%2020240730160913.png)


```c
void vga_update_screen() {
  // TODO: call `update_screen()` when the sync register is non-zero,
  // then zero out the sync register
  if(vgactl_port_base[1]){
    vgactl_port_base[1] = 0; // clear the sync register
  }
}

void init_vga() {

  vmem = new_space(screen_size());
  add_mmio_map("vmem", CONFIG_FB_ADDR, vmem, screen_size(), NULL);
  //这部分回调函数应该更新屏幕, 但是这里没有更新. 
  IFDEF(CONFIG_VGA_SHOW_SCREEN, init_screen());
  IFDEF(CONFIG_VGA_SHOW_SCREEN, memset(vmem, 0, screen_size()));
}
```

注意看`test`是因为, 这里边蕴含了我们需要的长宽信息.
```c
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  uint32_t *const fb = (uint32_t *)(uintptr_t)FB_ADDR; // locate buffer
  if (ctl->sync)
    outl(SYNC_ADDR, 1);
  // write to buffer, line first
  for (int i = ctl->y; i < ctl->y + ctl->h; i++)
    for (int j = ctl->x; j < ctl->x + ctl->w; j++)
      fb[i * vga_width + j] =
          ((uint32_t *)ctl->pixels)[(i - ctl->y) * ctl->w + (j - ctl->x)];
  //      io_write(AM_GPU_FBDRAW, x * w, y * h, color_buf, w, h, false);
}
```
我们实现的时候如何知道`x,y`和`w,h`的对饮关系? 
```c
io_write(AM_GPU_FBDRAW, x * w, y * h, color_buf, w, h, false);
```
这里就有了, 每次画一个色块. 
`x`是宽方向的内容, `y`是高方向的内容; 因此首先行计数, 然后列计数. 

> 上面的实现是有`bug`的, 我们应该等待绘制结束之后, 再去更新同步状态. 因为更新同步状态是面向硬件的, 他不会等待你里边指令执行结束之后才进行. 下面的实现才是正确的. 
```c
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  uint32_t *const fb = (uint32_t *)(uintptr_t)FB_ADDR; // locate buffer
  // write to buffer, line first
  for (int i = ctl->y; i < ctl->y + ctl->h; i++)
    for (int j = ctl->x; j < ctl->x + ctl->w; j++)
      fb[i * vga_width + j] =
          ((uint32_t *)ctl->pixels)[(i - ctl->y) * ctl->w + (j - ctl->x)];
  //      ref: io_write(AM_GPU_FBDRAW, x * w, y * h, color_buf, w, h, false);

  if (ctl->sync)
    outl(SYNC_ADDR, 1);
}
```


![](assets/Pasted%20image%2020240730165506.png)


实现之后, 就可以放一下有意思的了

![](assets/Pasted%20image%2020240730172115.png)

-- -- 
## TODO: 声卡

-- -- 

## 打字小游戏

> 请你以打字小游戏为例, 结合"程序在计算机上运行"的两个视角, 来剖析打字小游戏究竟是如何在计算机上运行的. 具体地, 当你按下一个字母并命中的时候, 整个计算机系统(NEMU, ISA, AM, 运行时环境, 程序) 是如何协同工作, 从而让打字小游戏实现出"命中"的游戏效果?

### `font.c`与`game.c`
这两个会被`cross compiler`编译为`riscv32`的程序, 并且按照框架中给定的`*.ld`链接为符合`nemu`可以处理的指令. 

### `main`
```c
ioe_init();
video_init();

io_read(...)
```
这部分是`am`的内容, `am`提供与硬件`nemu`交互的接口. 比如我们刚刚所做的`video_init`部分, 这里其实都是已经编译好的指令, 我们可以理解为一个和硬件交互的库, 这里的交互就是和`IO`外设交互的库. 他们并不是在`nemu`中实现的, 而是同样运行在`nemu`中的

### `Klib`
```c
printf("Type 'ESC' to exit\n");
```
这里的`printf`可并不是`glibc`或者`libc`中链接过去的`printf`. 这是运行在`nemu`中的`printf`. 实际上是运行在裸机的`printf`. 

### 其他函数

至于游戏中的其他函数, 也就是正常的函数调用. 但注意, 这些都是运行在`nemu`中的, 也就是说我们可以通过`ftrace`追踪到的. 

非常奇妙, 我们的抽象层级也一步一步的变化. 

-- -- 
## `nemu`中的`nemu`

这里的抽象层级又变化了. `nemu`相当于一个`TRM + IOE`, 我们在这上面运行一个`nemu`模拟器, 然后在`nemu`模拟器里边还可以继续运行程序. Crazy. 

> 这就类似于在`qemu`运行一个`debian12`, 再在这个`debian12`的`qemu`中运行一个`archlinux`.

-- -- 
## `Klib`- 编写可移植的程序

> 为了不损害程序的可移植性, 你编写程序的时候不能再做一些架构相关的假设了, 比如"指针的长度是4字节"将不再成立, 因为在`native`上指针长度是8字节, 按照这个假设编写的程序, 在`native`上运行很有可能会触发段错误.
> 
> 当然, 解决问题的方法还是有的, 至于要怎么做, 老规矩, STFW吧.


-- -- 

## 必答题(选)

> 有些太简单了我就懒得写了

### `inline` and `static`
编译与链接 在`nemu/include/cpu/ifetch.h`中, 你会看到由`static inline`开头定义的`inst_fetch()`函数. 分别尝试去掉`static`, 去掉`inline`或去掉两者, 然后重新进行编译, 你可能会看到发生错误. 请分别解释为什么这些错误会发生/不发生? 你有办法证明你的想法吗?

这个有点意思. 实际上`C`和`CPP`中的`inline`还不是一回事. 

[参考cppreference](https://en.cppreference.com/w/c/language/inline)

`inline`蕴含了静态属性, 也就是链接时外部不可见. 
> If a non-static function is declared `inline`, then it must be defined in the same translation unit. The inline definition that does not use `extern` is not externally visible and does not prevent other translation units from defining the same function. <u>This makes the `**inline**` keyword an alternative to `static` for defining functions inside header files, which may be included in multiple translation units of the same program</u>.

> Each translation unit may have zero or one external definition of every identifier with [internal linkage](https://en.cppreference.com/w/c/language/storage_duration "c/language/storage duration") (a `static` global).
> 
> If an identifier with internal linkage is used in any expression other than a non-VLA,(since C99) [`sizeof`](https://en.cppreference.com/w/c/language/sizeof "c/language/sizeof"), [`_Alignof`](https://en.cppreference.com/w/c/language/_Alignof "c/language/ Alignof")(since C11), or [`typeof`](https://en.cppreference.com/w/c/language/typeof "c/language/typeof")(since C23), <u>there must be one and only one external definition for that identifier in the translation unit</u>.
> 
> The entire program may have zero or one external definition of every identifier with [external linkage](https://en.cppreference.com/w/c/language/storage_duration "c/language/storage duration").
> 
> If an identifier with external linkage is used in any expression other than a non-VLA,(since C99) [`sizeof`](https://en.cppreference.com/w/c/language/sizeof "c/language/sizeof"), [`_Alignof`](https://en.cppreference.com/w/c/language/_Alignof "c/language/ Alignof")(since C11), or [`typeof`](https://en.cppreference.com/w/c/language/typeof "c/language/typeof")(since C23), there must be one and only one external definition for that identifier somewhere in the entire program.
>
> inline definitions in different translation units are not constrained by one definition rule. See [`inline`](https://en.cppreference.com/w/c/language/inline "c/language/inline") for the details on the inline function definitions.


The `inline` keyword was adopted from C++, but in C++, if a function is declared `inline`, it must be declared `inline` in every translation unit, and also every definition of an inline function must be exactly the same 

(in C, the definitions may be different, and depending on the differences only results in unspecified behavior). On the other hand, C++ allows non-const function-local statics and all function-local statics from different definitions of an inline function are the same in C++ but distinct in C.



在这里的话, 我们可以去掉`static'`, 但是不能去掉`inline`. 

### `volatile`
编译与链接
1. 在`nemu/include/common.h`中添加一行`volatile static int dummy;` 然后重新编译NEMU. 请问重新编译后的NEMU含有多少个`dummy`变量的实体? 你是如何得到这个结果的?
2. 添加上题中的代码后, 再在`nemu/include/debug.h`中添加一行`volatile static int dummy;` 然后重新编译NEMU. 请问此时的NEMU含有多少个`dummy`变量的实体? 与上题中`dummy`变量实体数目进行比较, 并解释本题的结果.
3. 修改添加的代码, 为两处`dummy`变量进行初始化:`volatile static int dummy = 0;` 然后重新编译NEMU. 你发现了什么问题? 为什么之前没有出现这样的问题? (回答完本题后可以删除添加的代码.)
```c
//sv.h
static  volatile int a = 0;
```

```c
//test1.c

#include "sv.h"
#include <stdio.h>

int func(){
  printf("func: [%p] %d\n", &a, a);
  return 0;
}

```

```c
//test.c

#include<stdio.h>
#include "sv.h"
extern int func(); 

int main(){
  printf("Hello world\n");
  printf("[%p] %d\n", &a, a);
  func();
}
```

![](assets/Pasted%20image%2020240730183534.png)
这个地方实际上`volatile`应该没啥卵用, 可能是担心编译器优化掉这个没有使用的变量? 主要是`static`. 实际上我们只要知道预处理之后的文件, 有一个`static int a`, 那么只在当前编译单元可见. 因此这个头文件被包含几次, 那么就有几个. 

```shell
grep  -E '\s*#include\s*["<]debug.h[">]\s*' ./ -r | count

# output 2
```

```shell
grep  -E '\s*#include\s*["<]common.h[">]\s*' ./ -r | count

# output 44
```

当然如果定义就有问题了, 因为`debug.h`与`common.h`互相包含.  然后就多重定义了. 如果不赋值, 可以当做声明, 所以没问题. 

```c
#include "sv.h"
#include <stdio.h>
int func();
int func();
int func();
int func();
int func();
int func();
int func();
int func();

static volatile int a = 0;
int main() {
  printf("Hello world\n");
  printf("[%p] %d\n", &a, a);
  func();
}
```
类似于我们可以这样声明多次. 


### `Makefile`
了解Makefile 请描述你在`am-kernels/kernels/hello/`目录下敲入`make ARCH=$ISA-nemu` 后, `make`程序如何组织.c和.h文件, 最终生成可执行文件`am-kernels/kernels/hello/build/hello-$ISA-nemu.elf`. (这个问题包括两个方面:`Makefile`的工作方式和编译链接的过程.)

### ARCHITECTURE

```makefile
ARCH_SPLIT = $(subst -, ,$(ARCH)) # 替换-为空格
ISA        = $(word 1,$(ARCH_SPLIT)) # 分词 1
PLATFORM   = $(word 2,$(ARCH_SPLIT)) # 分词 2
```

```makefile
# OBJS就是所有源文件的.o形式, 这里通过加前缀拼接形成路径. 
OBJS      = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(SRCS))))

# lazy evaluation ("=") causes infinite recursions
# 这里在原有LIBS基础上加上am klib这两个lib
LIBS     := $(sort $(LIBS) am klib) 

LINKAGE   = $(OBJS) \
  $(  addsuffix -$(ARCH).a, \
	$(join \
    $(addsuffix /build/, $(addprefix $(AM_HOME)/, $(LIBS))), \
    $(LIBS) )  )
```

```makefile
# 这里通过后续制定交叉编译对应的编译器和binutils
AS        = $(CROSS_COMPILE)gcc
CC        = $(CROSS_COMPILE)gcc
CXX       = $(CROSS_COMPILE)g++
LD        = $(CROSS_COMPILE)ld
AR        = $(CROSS_COMPILE)ar
OBJDUMP   = $(CROSS_COMPILE)objdump
OBJCOPY   = $(CROSS_COMPILE)objcopy
READELF   = $(CROSS_COMPILE)readelf
```

```makefile
# 这部分就是为不同架构制定不同规则
-include $(AM_HOME)/scripts/$(ARCH).mk
```

`$(dir names…)`[](https://www.gnu.org/software/make/manual/html_node/File-Name-Functions.html#index-dir)

Extracts the directory-part of each file name in names. The directory-part of the file name is everything up through (and including) the last slash in it. If the file name contains no slash, the directory part is the string ‘./’. For example,
```makefile
$(dir src/foo.c hacks)
```

produces the result `src/ ./`

```makefile
### Rule (compile): a single `.c` -> `.o` (gcc)
$(DST_DIR)/%.o: %.c
	@mkdir -p $(dir $@) && echo + CC $<
	@$(CC) -std=gnu11 $(CFLAGS) -c -o $@ $(realpath $<)

### Rule (compile): a single `.cc` -> `.o` (g++)
$(DST_DIR)/%.o: %.cc
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) -std=c++17 $(CXXFLAGS) -c -o $@ $(realpath $<)

### Rule (compile): a single `.cpp` -> `.o` (g++)
$(DST_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) -std=c++17 $(CXXFLAGS) -c -o $@ $(realpath $<)

### Rule (compile): a single `.S` -> `.o` (gcc, which preprocesses and calls as)
$(DST_DIR)/%.o: %.S
	@mkdir -p $(dir $@) && echo + AS $<
	@$(AS) $(ASFLAGS) -c -o $@ $(realpath $<)

### Rule (recursive make): build a dependent library (am, klib, ...)
$(LIBS): %:
	@$(MAKE) -s -C $(AM_HOME)/$* archive

### Rule (link): objects (`*.o`) and libraries (`*.a`) -> `IMAGE.elf`, the final ELF binary to be packed into image (ld)
$(IMAGE).elf: $(OBJS) $(LIBS)
	@echo + LD "->" $(IMAGE_REL).elf
	@$(LD) $(LDFLAGS) -o $(IMAGE).elf --start-group $(LINKAGE) --end-group

### Rule (archive): objects (`*.o`) -> `ARCHIVE.a` (ar)
$(ARCHIVE): $(OBJS)
	@echo + AR "->" $(shell realpath $@ --relative-to .)
	@$(AR) rcs $(ARCHIVE) $(OBJS)

### Rule (`#include` dependencies): paste in `.d` files generated by gcc on `-MMD`
-include $(addprefix $(DST_DIR)/, $(addsuffix .d, $(basename $(SRCS))))
```

这一步会编译每个文件, 然后build静态库, 最后`link`, 以及生成镜像. 最后根据目标, 调用`nemu`的时候传递不同的参数, 实际上这个`makefile`被多次复用. 


