#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
// lab mmap
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"



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
      argint(2, &prot)      < 0 ||
      argint(3, &flags)     < 0 ||
      argint(4, &fd)        < 0 ||
      argint(5, &offset)    < 0) 
  {
    return -1;
  }
  
  struct proc* p = myproc();
  acquire(&p->lock);
  struct file* f = filedup(p->ofile[fd]);
  if (f->writable==0 && flags & MAP_SHARED && (prot & PROT_WRITE)) {
    release(&p->lock);
    return -1;
  }
  // if (f->readable==0 && (prot & PROT_READ)) {
  //   release(&p->lock);
  //   return -1;
  // }

  // increase the va, but don't allocate pa
  uint64 start_addr = p->sz;
  while (len > 0) {
    p->sz += PGSIZE;
    len -= PGSIZE;
  }
  // increase the ref of the file
  struct vma_for_mmap vma_for_mmap = {
    start_addr,
    p->sz - PGSIZE,
    len,
    prot,
    flags,
    offset,
    f,
  };
  p->vma_for_mmap[p->mmap_cnt] = vma_for_mmap;
  p->mmap_cnt += 1;
  release(&p->lock);
  return start_addr;
}

uint64
sys_munmap(void)
{
  uint64 addr;
  uint len;
  if (argaddr(0, &addr) < 0 ||
      argint(1, (int*)&len) < 0)
  {
    return -1;
  }
  struct proc* p = myproc();
  // acquire(&p->lock);

  int cnt = 0;
  for (int i = 0; i < p->mmap_cnt; i++) {
    uint64 start_addr = p->vma_for_mmap[i].start_addr;
    uint64 end_addr = p->vma_for_mmap[i].end_addr;
    if (start_addr <= addr && end_addr >= addr)
    {
      uint64 addr_upper = addr;  
      int len_tmp = len + addr - PGROUNDDOWN(addr); 
      addr = PGROUNDDOWN(addr); 
      while (len_tmp > 0 && addr_upper <= end_addr) {
        addr_upper += PGSIZE;
        len_tmp -= PGSIZE;
        cnt += 1;
      }
      addr_upper -= PGSIZE;
      // we assume that the addr is either start or end
      if (addr == start_addr) {
        // printf("old addr:%p\n", start_addr);
        // printf("modify addr:%p\n", addr_upper);
        // printf("addr_end:%p\n", end_addr);
        p->vma_for_mmap[i].start_addr = addr_upper + PGSIZE;
      }
      if (addr_upper == end_addr) {
        p->vma_for_mmap[i].end_addr = addr - PGSIZE;
      }
      if (p->vma_for_mmap[i].flags & MAP_SHARED) {
        // update the related file in disk
        // printf("addr:%p\n", addr);
        filewrite(p->vma_for_mmap[i].file, addr, len + addr - PGROUNDDOWN(addr));
      }

      // printf("addr:%p\n", addr);
      // printf("cnt:%d\n", cnt);
      for (int i = 0; i < cnt; i++) {
        if (*walk(p->pagetable, addr, 0) & PTE_V)
          uvmunmap(p->pagetable, addr, 1, 0);
      }
      if (p->vma_for_mmap[i].start_addr > p->vma_for_mmap[i].end_addr) {
        // all the pages have been unmap
        fileclose(p->vma_for_mmap[i].file);
      }
      // release(&p->lock);
      return 0;
    }
  }

  // release(&p->lock);
  return -1;
}