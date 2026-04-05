#include <unistd.h>
//移植的时候实现
__attribute__((weak)) void cli_putc(char ch)
{
	write(STDOUT_FILENO, &ch, 1);
}
