#include "types.h"
#include "course_sid.h"

void console_putc(char c);
void printf(const char *format, ...);

static uint64 banner_checksum;

static void
banner_putc(char c)
{
#if LAB1_BANNER_PROTOCOL == 0
  console_putc(c);
#elif LAB1_BANNER_PROTOCOL == 1
  console_putc(c);
  console_putc('.');
#elif LAB1_BANNER_PROTOCOL == 2
  console_putc(c);
  banner_checksum += (uint8)c;
#else
#error "Unsupported LAB1_BANNER_PROTOCOL"
#endif
}

static void
banner_puts(const char *s)
{
  while (*s != '\0')
    banner_putc(*s++);
}

static void
banner_put_uint(uint64 value, uint base)
{
  static const char digits[] = "0123456789abcdef";
  char buffer[32];
  uint length = 0;

  do {
    buffer[length++] = digits[value % base];
    value /= base;
  } while (value != 0);

  while (length != 0)
    banner_putc(buffer[--length]);
}

static void
print_banner(void)
{
  banner_checksum = 0;
  banner_puts("OSLAB1 sid=");
  banner_put_uint((uint64)COURSE_SID, 10);
  banner_puts(" mod97=0x");
  banner_put_uint((uint64)COURSE_SID % 97, 16);
  banner_putc('\n');

#if LAB1_BANNER_PROTOCOL == 2
  // 协议 2 采用 Banner 字节的无符号 ASCII 累加和。
  printf("[chk=%d]", (int)banner_checksum);
#endif
}

static void
print_printf_checks(void)
{
  // 这些带固定前缀的行是开发自检，不属于首行 Banner 验收数据。
  printf("[printf-test] zero=%d negative=%d min=%d max=%d "
         "hex=%x empty=[%s] char=%c percent=%%\n",
         0, -123, (-2147483647 - 1), 2147483647,
         (uint)0xabcdef, "", 'Z');
  printf("[printf-test] long=%s\n",
         "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
         "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
         "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
}

void
main(void)
{
  print_banner();
  print_printf_checks();

  for (;;)
    asm volatile("wfi");
}
