
> 之前一直对`makefile`的语法不够熟悉, 只知道最基本的简单构建, 但是在`pa2`中需要阅读大量的`makefile`, 因此我对细节的理解不能太大缺失. 这里对其进行补充学习. 


## prerequests

- 如何识别文件被修改? 
```makefile
blah:
	cc blah.c -o blah
```

把文件作为依赖即可
```
blah: blah.c 
	cc blah.c -o blah
```

## strings

single or double quotes have no meaning to Make. They are simply characters that are assigned to the variable. Quotes _are_ useful to shell/bash, though, and you need them in commands like `printf`. In this example, the two commands behave the same:

```makefile
a := one two# a is set to the string "one two"
b := 'one two' # Not recommended. b is set to the string "'one two'"
all:
	printf '$a'
	printf $b
```

Reference variables using either `${}` or `$()`
```
x := dude

all:
	echo $(x)
	echo ${x}

	# Bad practice, but works
	echo $x 
```
说实话没想到这是个`Bad practice`, 实际上这种写法我也见到很多, 这里是引用变量. 

## echo on screen
```makefile
x := This is a makefile trial

target_a: target_b
	@echo "target a depends on target_b"
	touch target_a

target_b:
	@echo $(x)
	@echo "create target_b"
	touch target_b

clean:
	rm -f target_a target_b
```

这里`@`号作用在`echo`这里体现的就很明显, 我们既然已经输出了, 那么就不希望这里再输出一次了. 因此我们使用`@`关闭其输出

## multiple targets


```makefile
$@
```

用一个例子
```makefile
all: a.o b.o

a.o b.o:
	@echo $@
```

会输出
```
a.o
b.o
```

## wildcard pattern

这个`wildcard` 适用于匹配文件的. 我们通过语法
```makefile
$(wildcard pattern)
```
比如
```makefile
$(wildcard *.cpp)
```
- `*` 任意个字符
- `?`单个字符
- `[abc]`同正则表达式
- `[!abc]`除了这里面的其他的字符

比如这样的例子, 其中特殊字符后面会解释
```makefile
print: $(wildcard *.c *.cpp)
	ls -ll $?
```

***避坑: 通配符并非任意时刻都展开***, 我们必须在解引用内部进行统配, 否则会被当成字符. 
```makefile
thing_wrong := *.o # Don't do this! '*' will not get expanded 
thing_right := $(wildcard *.o) 

all: one two three four # Fails, because $(thing_wrong) is the string "*.o" 

one: $(thing_wrong) # Stays as *.o if there are no files that match this pattern:( 

two: *.o 

# Works as you would expect! In this case, it does nothing. 
three: $(thing_right) # Same as rule three four: $(wildcard *.o)

```

百分号的用途就很多了, 这里只列出一个目标的统配: 
> 这里利用了string substitution: 
> `$(patsubst pattern,replacement,text)` does the following:
> "Finds ***whitespace-separated words*** in text that match pattern and replaces them with replacement. 
> Here pattern may contain a ‘%’ which acts as a wildcard, matching ***any number of any characters*** within a word. If replacement also contains a ‘%’, the ‘%’ is replaced by the text that matched the ‘%’ in pattern. Only the first ‘%’ in the pattern and replacement is treated this way; any subsequent ‘%’ is unchanged."

这里面有几种`shorthand`: 
```makefile
foo := a.o b.o l.a c.o 

one := $(patsubst %.o,%.c,$(foo)) 

# This is a shorthand for the above 
two := $(foo:%.o=%.c) 

# This is the suffix-only shorthand, and is also equivalent to the above. 
three := $(foo:.o=.c) 

all: 
	echo $(one) 
	echo $(two) 
	echo $(three)
```
<u>Note: don't add extra spaces for this shorthand. It will be seen as a search or replacement term.</u>

pattern rules: Let's start with an example first:
```makefile
# Define a pattern rule that compiles every .c file into a .o file
%.o : %.c
		$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@ 
		# 这里表示第一个依赖, 但是这里就只有一个依赖, 所以就是依赖本身. 
```

## 自动变量
There are many [automatic variables](https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html), but often only a few show up:
```makefile
hey: one two
	# Outputs "hey", since this is the target name
	echo $@

	# Outputs all prerequisites newer than the target
	echo $?

	# Outputs all prerequisites
	echo $^

	# Outputs the first prerequisite
	echo $<

	touch hey

one:
	touch one

two:
	touch two

clean:
	rm -f hey one two
```

- Compiling a C program: `n.o` is made automatically from `n.c` with a command of the form `$(CC) -c $(CPPFLAGS) $(CFLAGS) $^ -o $@`
- Compiling a C++ program: `n.o` is made automatically from `n.cc` or `n.cpp` with a command of the form `$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $^ -o $@`
- Linking a single object file: `n` is made automatically from `n.o` by running the command `$(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@`

## 赋值

There are two flavors of variables:

- recursive (use `=`) - only looks for the variables when the command is _used_, not when it's _defined_.
- simply expanded (use `:=`) - like normal imperative programming -- only those defined so far get expanded


当然规则还有很多, 这么多对于阅读应该没什么问题了. 


-- -- 

## `MAKECMDGOALS`
```makefile
ifeq ($(MAKECMDGOALS),)
  MAKECMDGOALS  = image
  .DEFAULT_GOAL = image
endif
```
这个就是命令行接收到的目标, `.DEFAULT_GOAL`也是特殊变量. 顾名思义. 

-- --
## 项目的一些构建技巧

```makefile
SRCS-y += src/nemu-main.c
DIRS-y += src/cpu src/monitor src/utils
DIRS-$(CONFIG_MODE_SYSTEM) += src/memory
DIRS-$(CONFIG_TRACE) += src/trace/
DIRS-BLACKLIST-$(CONFIG_TARGET_AM) += src/monitor/sdb
```
实际上这里的`DIRS-$(...)`容易让人混淆, 其实这里在`menuconfig`部分已经有所定义, 如果存在的话就是`-y`也就加到源文件里边, 这些宏定义的话值就是`-y`. 
