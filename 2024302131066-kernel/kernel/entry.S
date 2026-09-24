#include "course_sid.h"

    .section .text
    .globl _entry
_entry:
    csrci mstatus, 8          # 清除 mstatus.MIE，关闭 M 态全局中断。
    csrw mie, zero            # 清空 M 态中断使能，屏蔽所有中断源。

    la t0, machine_trap       # 取最小 M 态陷阱入口的地址。
    csrw mtvec, t0            # 以 Direct 模式安装 M 态陷阱向量。

    csrr t0, mhartid          # 读取当前硬件线程编号。
    bnez t0, park             # 非 0 号 hart 不参与内核初始化。

    la t0, bss_start          # t0 指向需要清零的 BSS 首字节。
    la t1, kernel_end         # t1 指向链接脚本给出的内核末地址。
clear_bss:
    bgeu t0, t1, bss_cleared  # 到达 BSS 末尾后停止清零。
    sb zero, 0(t0)            # 将当前 BSS 字节写成零。
    addi t0, t0, 1            # 前进到下一个 BSS 字节。
    j clear_bss               # 继续清零剩余 BSS。

bss_cleared:
    la sp, boot_stack_top     # 清零完成后再启用向低地址增长的初始栈。
    call start                # 使用已对齐的栈调用 M 态初始化函数。
    j park                    # start 意外返回时进入安全停机循环。

    .balign 4
machine_trap:
    csrci mstatus, 8          # 陷阱发生后再次确保 M 态全局中断关闭。
    csrw mie, zero            # 屏蔽所有 M 态中断源，防止重复进入。
    wfi                       # 等待事件以降低停机循环的空转开销。
    j machine_trap            # 被唤醒后仍停留在异常停机入口。

park:
    wfi                       # 停放非 0 号 hart 或意外返回的 hart。
    j park                    # 被事件唤醒后继续保持停放。

    .section .bss
    .balign 16
bss_start:
boot_stack:
    .skip LAB1_STACK_KB * 1024 # 按课程参数分配初始内核栈容量。
    .balign 16
boot_stack_top:               # 16 字节对齐，满足 RISC-V ABI 的 sp 要求。
