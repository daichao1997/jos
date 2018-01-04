// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
#define GET_PDE(addr)	*(pde_t *)(uvpd + ((uint32_t)(addr) >> 22))
#define GET_PTE(addr)	*(pte_t *)(uvpt + ((uint32_t)(addr) >> 12))
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *)((uint32_t)utf->utf_fault_va & ~0xFFF);
	uint32_t err = utf->utf_err;
	if(!(GET_PDE(addr) & PTE_P))
		panic("Not a valid page table.\n");
	pte_t pte = GET_PTE(addr);
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
	// LAB 4: Your code here.
	if(!(err & FEC_WR))
		panic("Not a write.\n");
	if(!(pte & PTE_COW))
		panic("Not a COW page.\n");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	// LAB 4: Your code here.
	if((r = sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_alloc panic in pgfault: %e\n", r);

	memcpy(PFTEMP, addr, PGSIZE);

	if((r = sys_page_map(0, PFTEMP, 0, addr, PTE_SYSCALL & ((PGOFF(pte) & ~PTE_COW) | PTE_W | PTE_U | PTE_P))) < 0)
		panic("sys_page_map panic in pgfault: %e\n", r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	// LAB 4: Your code here.
	pte_t pte, newpte;
	void *addr = (void *)(pn*PGSIZE);
	if((GET_PDE(addr) & (PTE_U | PTE_P)) == (PTE_U | PTE_P))
		newpte = pte = GET_PTE(addr);
	else
		panic("duppage: the page table of page pn is not accessible.\n");

	if(pte & PTE_SHARE)
		return sys_page_map(0, addr, envid, addr, PTE_SYSCALL & PGOFF(pte));

	// permission needs modification if the src-page is writable or copy-on-write
	if(pte & (PTE_COW | PTE_W)) 
		newpte = (pte & ~PTE_W) | PTE_COW;

	// map the dest-page first
	if((r = sys_page_map(0, addr, envid, addr, PTE_SYSCALL & PGOFF(newpte))) < 0)
		return r;

	if((r = sys_page_map(0, addr, 0, addr, PTE_SYSCALL & PGOFF(newpte))) < 0)
		return r;
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t child_id;
	pte_t pte;
	uint32_t i;
	int r;
	extern void _pgfault_upcall(void);

	set_pgfault_handler(pgfault);

	if((child_id = sys_exofork()) < 0)
		return child_id;
	
	if(child_id == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// go through the page table and copy them
	for(i = 0; i < UXSTACKTOP - PGSIZE; i += PGSIZE) {
		// valid page table entry?
		if(GET_PDE(i) & PTE_P) {
			pte = GET_PTE(i);
			
			if(!(pte & PTE_P)) continue;
		} else continue;

		// for writable/copy-on-write pages, call duppage
		if(pte & (PTE_W | PTE_COW)) {
			if((r = duppage(child_id, i/PGSIZE)) < 0)
				return r;
		}
		// for read-only pages, simply map them
		else {
			if((r = sys_page_map(0, (void *)i, child_id, (void *)i, PTE_SYSCALL & PGOFF(pte))) < 0)
				return r;
		}
	}

	if((sys_env_set_pgfault_upcall(child_id, _pgfault_upcall)) < 0)
		panic("Can't set page fault upcall.\n");
	// allocate a new page for user exception stack
	if((sys_page_alloc(child_id, (void *)UXSTACKTOP- PGSIZE, PTE_U | PTE_W | PTE_P)) < 0)
		panic("Can't allocate user exception stack.\n");
	// mark it runnable
	if((sys_env_set_status(child_id, ENV_RUNNABLE) < 0))
		panic("Can't set status.\n");

	return child_id;
}

// Challenge!

static int
sduppage(envid_t envid, unsigned pn, int cow)
{
	int r;
	// LAB 4: Your code here.
	pte_t pte, newpte;
	void *addr = (void *)(pn*PGSIZE);

	if((GET_PDE(addr) & (PTE_U | PTE_P)) == (PTE_U | PTE_P))
		newpte = pte = GET_PTE(addr);
	else
		panic("sduppage: the page table of page pn is not accessible.\n");

	if(pte & PTE_SHARE)
		return sys_page_map(0, addr, envid, addr, PTE_SYSCALL & PGOFF(pte));

	// permission needs modification if the src-page is copy-on-write or cow != 0
	if(cow || (pte & PTE_COW))
		newpte = (pte & ~PTE_W) | PTE_COW;
	else
		newpte = pte & ~PTE_COW;

	// map the dest-page first
	if((r = sys_page_map(0, addr, envid, addr, PTE_SYSCALL & PGOFF(newpte))) < 0)
		return r;

	if((r = sys_page_map(0, addr, 0, addr, PTE_SYSCALL & PGOFF(newpte))) < 0)
		return r;

	return 0;
}

int
sfork(void)
{
	envid_t child_id;
	pte_t pte;
	uint32_t i;
	int r;
	extern void _pgfault_upcall(void);

	set_pgfault_handler(pgfault);

	if((child_id = sys_exofork()) < 0)
		return child_id;

	if(child_id == 0) {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	// go through the page table and copy them
	int cow = 1;
	for(i = USTACKTOP - PGSIZE; i >= UTEXT; i -= PGSIZE) {
		// valid page table entry?
		if((GET_PDE(i) & PTE_P) > 0 && ((pte = GET_PTE(i)) & PTE_P) > 0 && (pte & PTE_U) > 0) {
			if((r = sduppage(child_id, i/PGSIZE, cow)) < 0)
				return r;
		} else
			cow = 0;
	}

	if((sys_env_set_pgfault_upcall(child_id, _pgfault_upcall)) < 0)
		panic("Can't set page fault upcall.\n");
	// allocate a new page for user exception stack
	if((sys_page_alloc(child_id, (void *)UXSTACKTOP- PGSIZE, PTE_U | PTE_W | PTE_P)) < 0)
		panic("Can't allocate user exception stack.\n");
	// mark it runnable
	if((sys_env_set_status(child_id, ENV_RUNNABLE) < 0))
		panic("Can't set status.\n");

	return child_id;
}
