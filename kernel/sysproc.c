#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 start_va;
  if(argaddr(0, &start_va) < 0)
    return -1;
  int num;
  if(argint(1, &num) < 0)
    return -1;
  if(num > 64)
    // the num of the pages should not be bigger than 64
    return -1;
  uint64 user_addr;
  if(argaddr(2, &user_addr) < 0)
    return -1;
  
  uint64 bitmask = 0;
  pagetable_t pagetable = myproc()->pagetable;
  // printf("the table of pgaccess:\n");
  // vmprint(pagetable);
  for (int i = 0; i < num; i++) {
    pte_t* pte = walk(pagetable, start_va, 0);
    // printf("pte: %p\n", *pte);
    if (*pte & PTE_A) {
      bitmask |= (1 << i);
      // clear the PTE_A bit
      *pte &= (~PTE_A);
    }
    // check the next page(not sure)
    start_va += (1 << 12);
  }
  // printf("bitmask: %p\n", bitmask);
  copyout(pagetable, user_addr, (char *)(&bitmask), sizeof(bitmask));
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
