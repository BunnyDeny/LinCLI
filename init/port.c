#include <stdio.h>
//移植的时候实现
__attribute__((weak)) void cli_putc(char ch)
{
	printf("%c", ch);
}
