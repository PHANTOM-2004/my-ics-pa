
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

