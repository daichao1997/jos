// hello, world
#include <inc/lib.h>

volatile int a = 1;

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
}
