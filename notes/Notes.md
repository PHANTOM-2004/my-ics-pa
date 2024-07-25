
## binutils/coreutils
- objdump
- objcopy
- nm
- size
- gprof (timing function)
- readelf
- addr2line

### static链接
通过elf中的重定位表, 确定应该给`main.o`填充什么. 
### 程序的第一条指令

可以从`readelf`中读出, 同时可以使用`gdb`中的`starti`进行验证. 

### strace`

### man gcc does not work
need gcc-doc from apt

### end

after `extern char end`, `&end`标志最后一个位置. 

### ldd
用于查看程序的动态链接, 这里能找到一个
```shell
a.out: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=c8a327fed3d6749b644fe920ed5bb37772c3e80a, for GNU/Linux 3.2.0, with debug_info, not stripped
```
实际上这里的`*so.2`做的事情和shebang做的事情是一样的. 