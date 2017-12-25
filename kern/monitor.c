// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/env.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Trace back through the stack", mon_backtrace },
	{ "showmap", "Given a virtual address range, display its physical page mappings", mon_showmap },
	{ "setperm", "Set, clear, or change the permissions of any mapping in the current address space", mon_setperm },
	{ "showmem", "Dump the contents of a range of memory given either a virtual or physical address range", mon_showmem },
	{ "continue", "Single-step one instruction at a time", mon_continue }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Stack backtrace:\n");

	uint32_t *ebp, eip, arg[5];
	struct Eipdebuginfo info_;
	struct Eipdebuginfo* info = &info_; // to prevent "uninitialized warning"

	// trace back through the stack, until ebp is 0
	for(ebp = (uint32_t*)(read_ebp()); ebp != 0; ebp = (uint32_t*)(*ebp))
	{
		// read eip
		eip = ebp[1];
		if(debuginfo_eip((uintptr_t)(eip), info) < 0)
		{
			cprintf("%s\n", info->msg);
		}
		// read 5 arguments
		for(int i = 0; i < 5; i++) {arg[i] = ebp[i+2];}
		cprintf("  ebp %08x eip %08x args %08x %08x %08x %08x %08x\n",
			ebp, eip, arg[0], arg[1], arg[2], arg[3], arg[4]);
		cprintf("         %s:%d: ", info->eip_file, info->eip_line);
		// deal with non-null terminated string
		cprintf("%.*s", info->eip_fn_namelen, info->eip_fn_name);
		cprintf("+%u\n",(uint32_t)(info->eip_fn_addr));
	}
	return 0;
}

int mon_showmap(int argc, char **argv, struct Trapframe *tf) {
	uint32_t begin, end, va;
	pte_t pte, *entry;

	if(argc != 2 && argc != 3) {
		cprintf("usage: showmap <begin> [end]\n");
		return 0;
	}

	begin = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	end = (argc == 3) ? ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE) : 0;
	if(begin > end)
		end = begin;

	cprintf("   VA          PA         PERM    Present  Writable  User  Dirty\n");

	for(va = begin; va <= end; va += PGSIZE) {
		if(!(entry = pgdir_walk(kern_pgdir, (void *)va, 0)))
			continue;
		pte = *entry;
		cprintf("%08x    %08x    %08x      %u       %u       %u      %u\n", va, pte & 0xFFFFF000, pte & 0xFFF,
						!!(pte & PTE_P), !!(pte & PTE_W), !!(pte & PTE_U), !!(pte & PTE_D));
	}
	return 0;
}

int mon_setperm(int argc, char **argv, struct Trapframe *tf) {
	if(argc != 3 || strlen(argv[2]) != 3) {
		cprintf("usage: setperm <virtual address> <permission>." \
			"\"permission\" must be a 3-digit hexadecimal number.\n");
		return 0;
	}

	uint32_t va = strtol(argv[1], NULL, 16);
	uint32_t perm = strtol(argv[2], NULL, 16);
	pte_t *entry = pgdir_walk(kern_pgdir, (void *)va, 1);
	if(!entry) {
		cprintf("setperm failed\n");
		return 0;
	}
	*entry &= 0xFFFFF000;
	*entry |= perm;
	cprintf("Set permission of VA %08x to %x success.\n", va, perm);
	return 0;
}

int mon_showmem(int argc, char **argv, struct Trapframe *tf) {
	int option = 0; // 0: virtual, 1: physical
	uint32_t begin, end, addr;

	if(argc != 3 && argc != 4) {
		cprintf("usage: showmem [-v(default)|-p] <begin> <end>\n");
		return 0;
	}

	if(argv[1][0] == '-') {
		switch(argv[1][1]) {
			case 'v':
				option = 0;
				break;
			case 'p':
				option = 1;
				break;
			default:
				cprintf("unsupported option '%s'\n", argv[1]);
				return 0;
		}
		begin = ROUNDDOWN(strtol(argv[2], NULL, 16), 8);
		end = ROUNDDOWN(strtol(argv[3], NULL, 16), 8);
	}
	else {
		begin = ROUNDDOWN(strtol(argv[1], NULL, 16), 8);
		end = ROUNDDOWN(strtol(argv[2], NULL, 16), 8);
	}

	if(option == 1) {
		extern size_t totalmem;
		if(end >= totalmem) {
			cprintf("Physical memory limit exceeded\n");
			return 0;
		}
		begin += KERNBASE;
		end += KERNBASE;
	}

	if(begin > end)
		end = begin;

	for(addr = begin; addr <= end; ) {
		cprintf("%08x:", addr);
		for(int i = 0; i < 8; i++, addr++) {
			cprintf(" %02x", (uint8_t)(*(char *)addr));
		}
		cprintf("\n");
	}
	return 0;
}

int mon_continue(int argc, char **argv, struct Trapframe *tf) {
	tf->tf_eflags |= FL_TF;
	env_run(curenv);
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
