#include "types.h"
#include "memlayout.h"
#include "course_sid.h"

#define UART_THR 0
#define UART_LSR 5
#define UART_LSR_THRE (1 << 5)

#define UART_THROTTLE_PERIOD (16 + COURSE_SID % 16)
// 说明书未规定 nop 次数；32 是本实现选用的小型节流循环次数。
#define UART_THROTTLE_NOPS 32

#define UART_REG(offset) ((volatile uint8 *)(UART0 + (offset)))

static uint throttle_count;

void
uartputc_sync(char c)
{
  // THRE 为 1 才能向发送保持寄存器写入下一个字节。
  while ((*UART_REG(UART_LSR) & UART_LSR_THRE) == 0)
    ;

  *UART_REG(UART_THR) = (uint8)c;
}

void
console_putc(char c)
{
  uint i;

  uartputc_sync(c);
  throttle_count++;

  if (throttle_count == UART_THROTTLE_PERIOD) {
    for (i = 0; i < UART_THROTTLE_NOPS; i++)
      asm volatile("nop");
    throttle_count = 0;
  }
}
