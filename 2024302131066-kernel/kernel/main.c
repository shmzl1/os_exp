#include "types.h"
#include "course_sid.h"

void printf(const char *format, ...);

void main(void)
{
  printf("OSLAB1 sid=%d mod97=0x%x\n",
         (long)COURSE_SID, (uint)(COURSE_SID % 97));

  for (;;)
    asm volatile("wfi");
}
