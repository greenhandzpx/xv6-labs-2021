// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// count the reference of each physical page
int reference_cnt[PHYSTOP / PGSIZE + 1];
struct spinlock ref_lock;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_lock, "ref cnt");
  acquire(&ref_lock);
  for (int i = 0; i < PHYSTOP / PGSIZE + 1; i++) {
    reference_cnt[i] = 0;
  }
  release(&ref_lock);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  acquire(&ref_lock);
  reference_cnt[(uint64)pa/PGSIZE] -= 1;
  if (reference_cnt[(uint64)pa/PGSIZE] > 0) {
    // ref cnt > 0
    // printf("cannot free %p\n", pa);
    release(&ref_lock);
    return;
  }
  release(&ref_lock);
  // printf("free %p\n", pa);
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&ref_lock);
    reference_cnt[(uint64)r / PGSIZE] = 1;  // init the ref cnt
    release(&ref_lock);
  }
  return (void*)r;
}
