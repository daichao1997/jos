#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;
	cprintf("i am parent environment %08x\n", thisenv->env_id);
/*
	if ((r = spawnl("hello", "hello", 0)) < 0)
		panic("spawn(hello) failed: %e", r);
	if ((r = spawnl("testpipe", "testpipe", 0)) < 0)
		panic("spawn(testpipe) failed: %e", r);
*/
	int child = fork();
	if(child == 0) {
		if ((r = execl("forktree", "forktree", 0)) < 0)
			panic("spawn(forktree) failed: %e", r);
	}
}
