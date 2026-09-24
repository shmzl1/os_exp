# Lab1 设计笔记

## 1. 启动时序图

```text
QEMU virt 复位
  PC = 0x00001000，M 态执行 QEMU 复位 ROM
    ↓
复位代码把控制权交给 -kernel 加载的 _entry
  PC = 0x80000000，M 态
    ↓
mstatus.MIE = 0；mie = 0
    ↓
mtvec = machine_trap（Direct 模式，入口 4 字节对齐）
    ↓
读取 mhartid
  ├─ 非 0 号 hart：进入 park 循环
  └─ hart 0：继续启动
    ↓
从 bss_start 到 kernel_end 逐字节清零
  此时未启用栈，只使用寄存器
    ↓
sp = boot_stack_top
  初始栈为 4 KB，栈顶 16 字节对齐
    ↓
call start()，仍为 M 态
    ↓
mstatus.MIE = 0；mstatus.MPP = S
    ↓
mepc = main
    ↓
medeleg = 0xffff；mideleg = 0xffff
    ↓
satp = 0（Bare 模式）
    ↓
pmpaddr0 = 0x3fffffffffffff；pmpcfg0 = 0xf
    ↓
mret：M 态 → S 态，PC = mepc
    ↓
main() 的第一条可执行语句：print_banner()
```

当前初始栈本身位于 `.bss`，BSS 清零范围会覆盖这块栈。因此必须先在不使用栈的汇编
代码中完成清零，再设置 `sp`。如果先使用栈，清零操作可能覆盖已经保存的返回地址或
局部变量。

## 2. 内核内存空间布局图

启动 ROM、UART MMIO 和内核 RAM 是物理地址空间中的独立区域，不属于一块连续内存：

```text
0x00001000  ┌──────────────────────────┐
            │ QEMU 复位 ROM            │  CPU 复位入口，不属于内核 RAM
            └──────────────────────────┘

0x10000000  ┌──────────────────────────┐
            │ UART0 MMIO               │  设备寄存器，不是普通 RAM
            └──────────────────────────┘

0x80000000  ┌──────────────────────────┐
            │ QEMU virt RAM            │
            ├──────────────────────────┤
            │ .text                    │  _entry 和其余代码
            ├──────────────────────────┤
            │ .rodata                  │  字符串及只读数据
            ├──────────────────────────┤
            │ .data                    │  已初始化的可写数据
            ├──────────────────────────┤
            │ .bss                     │  启动时清零的未初始化数据
            │   boot_stack（4 KB）     │  栈顶 16 字节对齐，向低地址增长
            │   其他 BSS 对象          │
            ├──────────────────────────┤
            │ kernel_end               │  已链接内核结束位置
            └──────────────────────────┘
```

`kernel.ld` 设置 `BASE_ADDRESS = 0x80000000` 和 `ENTRY(_entry)`，并按 `.text`、
`.rodata`、`.data`、`.bss` 的顺序布置内核。`entry.S` 在 `.bss` 中用
`.skip LAB1_STACK_KB * 1024` 分配初始栈，并在 `boot_stack_top` 前用 `.balign 16`
保证 ABI 所需的栈顶对齐。`kernel_end` 位于 `.bss` 之后，当前启动代码把它用作 BSS
清零区间的上界。

## 3. 五道核心思考题

### 3.1 为什么 PC 从 `0x1000` 开始，`-bios none` 表示什么，内核为何链接到 `0x80000000`

QEMU `virt` 机器的复位向量位于物理地址 `0x1000`，CPU 复位后先执行这里的 QEMU
复位 ROM。`-bios none` 表示不加载 OpenSBI 等外部固件，不表示复位 ROM 消失，也
不表示 CPU 直接从内核入口复位。QEMU 的复位代码仍会把控制权交给 `-kernel` 加载的
内核入口。

QEMU `virt` 的普通 RAM 从 `0x80000000` 开始，链接脚本也从该地址布置内核。这样
链接地址、加载地址和运行时地址一致。地址 `0x0` 不是该机器的内核 RAM 起点，把内核
链接到 `0x0` 会使程序地址与真实物理内存布局不符。

### 3.2 `entry.S` 中各项启动操作为什么存在

启动首先清除 `mstatus.MIE` 和 `mie`，避免尚未建立完整处理环境时进入中断。随后把
4 字节对齐的 `machine_trap` 地址写入 `mtvec`；地址低两位为 0，表示 Direct 模式。

代码读取 `mhartid`，只允许 hart 0 清零共享 BSS、使用唯一的初始栈并进入 C 代码。
其他 hart 进入 `park`，避免多个 hart 同时破坏共享初始化状态。hart 0 在不使用栈的
情况下先清零 BSS，再把 `sp` 设置为 `boot_stack_top`，然后调用 `start()`。栈位于
`.bss`，容量为 `LAB1_STACK_KB * 1024`，所以不能在清零前使用。异常入口、非 0 号
hart 和 `start()` 意外返回路径最终都进入不会继续执行初始化的停机循环。

### 3.3 从 M 态进入 S 态前必须配置什么

`start()` 读取 `mstatus`，清除 `MSTATUS_MPP_MASK` 和 `MSTATUS_MIE`，再设置
`MSTATUS_MPP_S`，使 `mret` 的目标特权级为 S。`mepc` 被设置为 `main`，决定
`mret` 后的 PC。`medeleg = 0xffff` 和 `mideleg = 0xffff` 请求把硬件允许委托的
低 16 位异常和中断交给 S 态；不可写或不可委托的位由硬件按其规则处理。

课程指定的 PMP 配置保持为：

```c
w_pmpaddr0(0x3fffffffffffffull);
w_pmpcfg0(0xf);
```

`pmpcfg0` 的低字节 `0xf` 中，bit 0、1、2 分别令 `R=W=X=1`，bits 4:3 为
`A=01`，bit 7 为 `L=0`。因此当前地址匹配模式实际是 **TOR**，不是说明书代码旁注
所写的 NAPOT；NAPOT 的 `A` 应为 `11`。对第 0 个 TOR 项，下界隐含为 0，上界为
`pmpaddr0 << 2` 且不包含上界。该范围覆盖本实验所需的 RAM 和 MMIO 物理地址，并向
S 态授予读、写、执行权限。缺少 PMP 授权时，`mret` 后在 S 态取第一条指令就可能
产生 instruction access fault。

最后执行 `mret`，处理器按 `mstatus.MPP` 切换到 S 态，并从 `mepc` 指定的
`main` 开始执行。

### 3.4 为什么选择 `satp = 0`，而不建立恒等映射

`satp = 0` 选择 Bare 模式，关闭 S 态地址翻译。S 态直接使用物理地址，正好与内核
链接和加载到 `0x80000000` 的方式一致，UART 的 `0x10000000` 也能直接作为 MMIO
地址访问。

Lab1 没有页表分配、页表填写和地址空间切换逻辑。恒等映射仍然需要正确建立多级页表，
并覆盖代码、数据、栈及 MMIO；它不会改善本实验的启动和输出目标，反而增加启动依赖。
因此当前阶段选择 Bare 模式。

### 3.5 UART 16550 轮询发送协议是什么

UART0 基地址是 `0x10000000`。THR 的偏移为 0，用于写入发送字节；LSR 的偏移为 5，
其中 bit 5 是 THRE。代码必须反复读取 LSR，只有 `LSR & (1 << 5)` 非零，即 THR
可以接收新字节时，才能向 THR 写入字符。MMIO 指针使用 `volatile`，保证每次状态读取
和数据写入都实际访问设备。

## 4. `entry.S` 逐行注解

### 4.1 指令

| 位置 | 指令 | 实际作用 |
| --- | --- | --- |
| `_entry` | `csrci mstatus, 8` | 清除 `mstatus.MIE`，关闭 M 态全局中断。 |
| `_entry` | `csrw mie, zero` | 清空 M 态各中断源的使能位。 |
| `_entry` | `la t0, machine_trap` | 把 M 态最小陷阱入口地址装入 `t0`。 |
| `_entry` | `csrw mtvec, t0` | 设置 M 态陷阱向量；对齐使其采用 Direct 模式。 |
| `_entry` | `csrr t0, mhartid` | 读取当前 hart 编号。 |
| `_entry` | `bnez t0, park` | 非 0 号 hart 跳到停放循环。 |
| `_entry` | `la t0, bss_start` | `t0` 指向需要清零的 BSS 首字节。 |
| `_entry` | `la t1, kernel_end` | `t1` 指向 BSS 清零区间的结束地址。 |
| `clear_bss` | `bgeu t0, t1, bss_cleared` | 用无符号地址比较；到达末尾时退出清零循环。 |
| `clear_bss` | `sb zero, 0(t0)` | 把当前 BSS 字节写成 0。 |
| `clear_bss` | `addi t0, t0, 1` | 指针前移一个字节。 |
| `clear_bss` | `j clear_bss` | 返回循环开头，继续清零。 |
| `bss_cleared` | `la sp, boot_stack_top` | 清零完成后才设置初始栈顶。 |
| `bss_cleared` | `call start` | 保存返回地址并调用 M 态 C 初始化函数。 |
| `bss_cleared` | `j park` | `start()` 意外返回时进入停放循环。 |
| `machine_trap` | `csrci mstatus, 8` | 发生 M 态陷阱后再次关闭全局中断。 |
| `machine_trap` | `csrw mie, zero` | 关闭所有 M 态中断源。 |
| `machine_trap` | `wfi` | 等待事件，降低停机循环的空转开销。 |
| `machine_trap` | `j machine_trap` | 被唤醒后仍停留在异常停机入口。 |
| `park` | `wfi` | 停放非 0 号 hart 或意外返回的 hart。 |
| `park` | `j park` | 被唤醒后继续停放。 |

`la`、`call` 和 `j` 是汇编器伪指令，汇编器会把它们展开为适合当前地址模型的真实
指令序列；表中说明的是它们在启动代码中的语义。

### 4.2 影响入口和内存布局的汇编指示

| 指示或标签 | 作用 |
| --- | --- |
| `#include "course_sid.h"` | 取得 `LAB1_STACK_KB` 个性化参数。 |
| `.section .text` | 把入口代码放入链接脚本的代码段。 |
| `.globl _entry`、`_entry:` | 导出链接脚本指定的入口符号。 |
| `.balign 4`、`machine_trap:` | 令 `mtvec` 目标至少 4 字节对齐。 |
| `.section .bss` | 把初始栈放入 BSS。 |
| `.balign 16`、`bss_start:`、`boot_stack:` | 对齐 BSS 起点，并标出清零起点和栈低地址。 |
| `.skip LAB1_STACK_KB * 1024` | 按个性化宏预留初始栈空间。 |
| `.balign 16`、`boot_stack_top:` | 保证初始 `sp` 满足 16 字节对齐要求。 |

## 5. Lab1 个性化设计

- 学号为 `2024302131066`，`LAB1_BANNER_PROTOCOL = 0`，因此使用原始明文并在末尾输出
  一个换行。Banner 内容为 `OSLAB1 sid=2024302131066 mod97=0x44\n`。
- 说明书给出的栈容量公式是 `4 KB × (1 + COURSE_SID % 3)`。本学号模 3 为 0，
  因此容量为 4 KB；`course_sid.h` 中对应 `LAB1_STACK_KB = 4`，汇编代码从该宏派生
  字节数。
- UART 节流周期使用源码表达式 `16 + COURSE_SID % 16`。本学号模 16 为 10，周期
  为 26 个实际写入 UART 的字节。计数达到周期后归零。
- 当前节流循环执行 32 次 `asm volatile("nop")`。说明书没有规定循环次数，32 是实现
  选择；`volatile` 防止这些 `nop` 被优化掉。
- 十六进制统一使用小写数字、`0x` 前缀且不补前导零，所以学号模 97 的十进制结果 68
  输出为 `0x44`。

## 6. UART 轮询输出设计

| 项目 | 设计 |
| --- | --- |
| 硬件基地址 | `UART0 = 0x10000000`，来自 `memlayout.h` |
| THR | 偏移 0，写入一个待发送字节 |
| LSR | 偏移 5，读取线路状态 |
| THRE | LSR bit 5；为 1 时 THR 可以接收新字节 |
| MMIO 语义 | 使用 `volatile uint8 *`，防止编译器合并或删除设备访问 |
| 等待策略 | 无超时地忙等 THRE，满足条件后写 THR |

职责分层如下：

1. `uartputc_sync(char c)` 只轮询 LSR 并向 THR 实际发送一个字节，不处理个性化协议。
2. `console_putc(char c)` 调用硬件发送函数，并按实际写入字节数执行个性化节流。
3. `main.c` 的 `banner_putc()` 处理 Banner 协议，协议逻辑不进入硬件发送函数。

Lab1 启动阶段只允许 hart 0 进入 C 代码，输出路径按单核、单上下文设计，不可重入，
不使用锁。本实验只实现轮询输出，不实现串口接收或中断驱动。

## 7. 最小 `printf` 格式规范

| 格式 | 规则 |
| --- | --- |
| `%d` | 参数类型为 `int`，按有符号十进制输出；负数先输出 `-`，再输出无符号幅值。 |
| `%x` | 参数类型为 `uint`，输出小写十六进制，固定带 `0x` 前缀，不输出前导零。 |
| `%s` | 逐字节输出字符串；空字符串不输出正文字符，空指针输出 `(null)`。 |
| `%c` | 参数按可变参数规则读取为 `int`，再输出一个字符。 |
| `%%` | 输出一个 `%`。 |

整数转换反复执行取余和除法，把最低位到最高位暂存到局部缓冲区，再逆序输出。
`do ... while` 保证数值 0 输出为一个 `0`。

对于最小 `int`，其绝对值无法用正的 `int` 表示，直接计算 `-value` 会发生有符号溢出。
当前实现先计算 `-(value + 1)`，再转换为 `uint64` 并加 1，得到可表示的无符号幅值，
因此可以正确输出 `-2147483648`。完整学号不经过 32 位 `%d`，而由
`banner_put_uint(uint64, base)` 输出，避免整数截断。
