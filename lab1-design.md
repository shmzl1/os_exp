# Lab1 设计笔记

## 1. 启动时序图

```text
QEMU 上电
    ↓
PC = 0x1000，执行启动 ROM
    ↓
加载并跳转至内核 _entry
PC = 0x80000000
M 态
    ↓
关闭中断，读取 mhartid
    ↓
非 0 号核心进入 park
    ↓
0 号核心设置初始内核栈
    ↓
清零 BSS，进入 start()
    ↓
配置 mstatus、mepc、medeleg、mideleg
    ↓
设置 satp = 0（Bare 模式）
    ↓
配置 PMP，允许 S 态访问物理内存
    ↓
mret：M 态 → S 态
    ↓
main()
    ↓
UART 初始化
    ↓
printf 输出 Banner 和自检信息
```

说明：具体启动指令、BSS 清零和寄存器配置顺序在实现时逐项核对，不能在栈建立前使用普通 C 函数。

## 2. 内核内存布局图

```text
低地址
0x00001000   QEMU 启动 ROM

0x10000000   UART 16550

0x80000000   内核 RAM 起点
             |
             +-- .text
             +-- .rodata
             +-- .data
             +-- .bss
             |     +-- 初始内核栈（4 KB）
             |
             +-- kernel_end

高地址
```

内核链接地址由 `kernel/kernel.ld` 中的 `BASE_ADDRESS = 0x80000000` 确定。

初始内核栈使用 `LAB1_STACK_KB` 宏决定容量。为栈保留对齐空间，初始化 sp 指向栈顶，栈向低地址增长。

## 3. 特权级切换清单

从 M 态进入 S 态之前：

* [ ] 关闭 M 态全局中断。
* [ ] 设置 mstatus.MPP = S。
* [ ] 设置 mepc = main 入口。
* [ ] 配置异常与中断委托寄存器。
* [ ] 设置 satp = 0，暂时关闭分页。
* [ ] 设置 PMP，授权 S 态访问内存。
* [ ] 设置对齐的 mtvec 异常入口。
* [ ] 执行 mret，进入 S 态。

本实验在没有建立页表前使用 Bare 模式，避免过早依赖尚未实现的虚拟内存管理。

## 4. UART 输出设计

硬件基地址：0x10000000。

THR 偏移：0。

LSR 偏移：5。

LSR bit5（THRE）表示发送保持寄存器可接收新字符。

输出流程：

检查 LSR bit5 → 忙等直到为 1 → 写入 THR。

本轮仅实现轮询输出，不实现串口输入或接收中断。

## 5. 个人参数与输出规范

学号：2024302131066。

Banner 协议：0，明文输出并以换行结束。

初始内核栈：4 KB。

空转节流：按照课程公式和 COURSE_SID 宏计算，不硬编码数值。

printf 需要覆盖数字 0、负数、最大整数、空字符串和连续长字符串。

十六进制输出统一使用小写 0x，无前导零。

最终 expect_banner.txt 必须与实际 UART 输出逐字节一致。
