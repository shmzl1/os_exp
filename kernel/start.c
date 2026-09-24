//entry.S 已经完成了关闭中断、清零 BSS、建立栈等初始化
#include "types.h"
#include "riscv.h"

#define MSTATUS_MIE (1L << 3)

void main(void);

void start(void) __attribute__((noreturn));//不会以正常方式结束

void
start(void)
{
  uint64 mstatus;

  // 保持 M 态全局中断关闭，并指定 mret 返回到 S 态。
  mstatus = r_mstatus();//读取 CPU 的 mstatus 寄存器，保存到 C 变量中
  mstatus &= ~(MSTATUS_MPP_MASK | MSTATUS_MIE);//把 mstatus 寄存器中的 MPP 位和 MIE 位全部清零，其他位保持不变
  //MPP 由两个位组成，用于决定执行 mret 后进入哪个特权级。清除 MIE：关闭 M 态全局中断

  mstatus |= MSTATUS_MPP_S;//把 mstatus 寄存器中的 MPP 字段设置为 S 模式
  w_mstatus(mstatus);//把修改后的值写回 CPU 寄存器

  // mret 将从 S 态的 main() 开始执行。把 main() 的地址写进 mepc，作为之后执行 mret 的跳转目标
  w_mepc((uint64)main);

  // 将可委托的异常和中断交给 S 态处理。
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  // 分页尚未建立，satp 控制 S 态地址翻译.S 态按 Bare 模式直接使用物理地址。
  w_satp(0);

  // TOR 区域覆盖所需物理地址，并授予 S 态读、写、执行权限。
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // 按 mstatus.MPP 和 mepc 完成 M 态到 S 态的切换。mret 是 RISC-V 的机器态陷阱返回指令
  asm volatile("mret");
  __builtin_unreachable();
}
