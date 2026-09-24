#include <stdarg.h>

#include "types.h"

void console_putc(char c);

static void
print_unsigned(uint64 value, uint base)
{
  static const char digits[] = "0123456789abcdef";
  char buffer[32];
  uint length = 0;

  // 至少生成一个数字，因此数值 0 会输出为 "0"。
  do {
    buffer[length++] = digits[value % base];
    value /= base;
  } while (value != 0);

  // 余数按低位到高位产生，逆序发送即可得到正常数字顺序。
  while (length != 0)
    console_putc(buffer[--length]);
}

static void
print_signed(long value)
{
  uint64 magnitude;

  if (value < 0) {
    console_putc('-');
    // 先加一再取负，避免对最小有符号整数直接取负而溢出。
    magnitude = (uint64)(-(value + 1)) + 1;
  } else {
    magnitude = (uint64)value;
  }

  print_unsigned(magnitude, 10);
}

static void
print_string(const char *s)
{
  if (s == 0)
    s = "(null)";

  while (*s != '\0')
    console_putc(*s++);
}

void
printf(const char *format, ...)
{
  va_list arguments;//初始化可变参数
  char specifier;

  va_start(arguments, format);
  while (*format != '\0') {
    if (*format != '%') {
      console_putc(*format++);
      continue;
    }

    format++;
    specifier = *format;
    if (specifier == '\0') {
      console_putc('%');
      break;
    }
    format++;

    switch (specifier) {
    case 'd':
      // 本内核的 %d 接收 RV64 long，调用者需传入 long 以保留完整学号。
      print_signed(va_arg(arguments, long));
      break;
    case 'x':
      // 只输出小写十六进制数字，0x 前缀由格式串提供。
      print_unsigned(va_arg(arguments, uint), 16);
      break;
    case 's':
      print_string(va_arg(arguments, const char *));
      break;
    case 'c':
      console_putc((char)va_arg(arguments, int));
      break;
    case '%':
      console_putc('%');
      break;
    default:
      console_putc('%');
      console_putc(specifier);
      break;
    }
  }
  va_end(arguments);//结束参数访问
}
