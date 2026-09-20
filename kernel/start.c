#include "types.h"
#include "riscv.h"

#define MSTATUS_MIE (1L << 3)

void main(void);

void start(void) __attribute__((noreturn));

void
start(void)
{
  uint64 mstatus;

  // 保持 M 态全局中断关闭，并指定 mret 返回到 S 态。
  mstatus = r_mstatus();
  mstatus &= ~(MSTATUS_MPP_MASK | MSTATUS_MIE);
  mstatus |= MSTATUS_MPP_S;
  w_mstatus(mstatus);

  // mret 将从 S 态的 main() 开始执行。
  w_mepc((uint64)main);

  // 将可委托的异常和中断交给 S 态处理。
  w_medeleg(0xffff);
  w_mideleg(0xffff);

  // 分页尚未建立，S 态按 Bare 模式直接使用物理地址。
  w_satp(0);

  // TOR 区域覆盖所需物理地址，并授予 S 态读、写、执行权限。
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // 按 mstatus.MPP 和 mepc 完成 M 态到 S 态的切换。
  asm volatile("mret");
  __builtin_unreachable();
}
