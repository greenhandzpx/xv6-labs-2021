#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
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


// lab mmap
uint64
sys_mmap(void)
{
  uint len;
  int prot, flags, fd, offset;
  if (argint(1, (int*)&len) < 0 ||
      argint(2, &prot) < 0      ||
      argint(3, &flags) < 0     ||
      argint(4, &fd) < 0        ||
      argint(5, &offset) < 0) 
  {
    return -1;
  }
  // increase the va, but don't allocate pa
  struct proc* p = myproc();
  uint64 start_addr = p->sz;
  while (len > 0) {
    p->sz += PGSIZE;
    len -= PGSIZE;
  }

  // increase the ref of the file
  struct file* f = filedup(p->ofile[fd]);
  struct vma_for_mmap vma_for_mmap = {
    start_addr,
    p->sz - PGSIZE,
    len,
    prot,
    f,
    offset
  };
  p->vma_for_mmap[p->mmap_cnt] = vma_for_mmap;
  p->mmap_cnt += 1;
  return start_addr;
}

uint64
sys_munmap(void)
{
  return -1;
}