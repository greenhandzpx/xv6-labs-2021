// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

// lab lock
#define BUCKETS 13

// trap.c
extern uint ticks;
// extern struct spinlock tickslock;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[BUCKETS];
  struct spinlock bucket_locks[BUCKETS];
} bcache;


void
binit(void)
{
  struct buf *b;
  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < BUCKETS; i++) {
    initlock(&bcache.bucket_locks[i], "bcache");
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }
  
  // Create linked list of buffers
  // let all the bufs go into bucket 0
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head[0].next;
    b->prev = &bcache.head[0];
    b->timestamp = 0;
    initsleeplock(&b->lock, "buffer");
    bcache.head[0].next->prev = b;
    bcache.head[0].next = b;
  }

}

// lab lock
void swap(int* a, int* b)
{
  int tmp = *a;
  *a = *b;
  *b = tmp;
}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // printf("bget\n");
  // printf("blockno get:%d\n", blockno);
  int index = blockno % BUCKETS;
  acquire(&bcache.bucket_locks[index]);

  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  release(&bcache.bucket_locks[index]);

  acquire(&bcache.lock);
  // to ensure that no repeated file in buffers, we should check the
  // the buffer again when we get the big lock
  acquire(&bcache.bucket_locks[index]);
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[index]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[index]);

  // sort the buf
  int indexes[NBUF]; 
  for (int i = 0; i < NBUF; i++) {
    indexes[i] = i;
  }
  for (int i = 0; i < NBUF; i++) {
    int init = i;
    for (int k = i + 1; k < NBUF; k++) {
      if (bcache.buf[init].timestamp < bcache.buf[k].timestamp) {
        init = k;
      } 
    }
    swap(&indexes[i], &indexes[init]);
  }

  for (int i = 0; i < NBUF; i++) {
    int k = indexes[i];
    b = &bcache.buf[k];
    int index_for_change = b->blockno % BUCKETS;
    acquire(&bcache.bucket_locks[index_for_change]);
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      // remove this buffer from its original bucket
      b->next->prev = b->prev;
      b->prev->next = b->next;
      release(&bcache.bucket_locks[index_for_change]);

      // add this buffer to the new bucket
      acquire(&bcache.bucket_locks[index]);
      b->next = bcache.head[index].next;
      b->prev = &bcache.head[index];
      bcache.head[index].next->prev = b;
      bcache.head[index].next = b;  
      release(&bcache.bucket_locks[index]);

      release(&bcache.lock);
      acquiresleep(&b->lock);

      return b;
    } 
    release(&bcache.bucket_locks[index_for_change]);
    
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  // printf("blockno rel:%d\n", b->blockno);

  int index = b->blockno % BUCKETS;

  releasesleep(&b->lock);


  acquire(&bcache.bucket_locks[index]);

  // (not sure)
  // acquire(&tickslock);
  b->timestamp = ticks;
  // release(&tickslock);

  b->refcnt--;

  // we don't care about the sequence of the buffers
  // if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;

    // b->next = bcache.head[index].next;
    // b->prev = &bcache.head[index];
    // bcache.head[index].next->prev = b;
    // bcache.head[index].next = b;
  // }
  
  release(&bcache.bucket_locks[index]);
}

void
bpin(struct buf *b) {
  int index = b->blockno % BUCKETS;
  acquire(&bcache.bucket_locks[index]);
  b->refcnt++;
  release(&bcache.bucket_locks[index]);
}

void
bunpin(struct buf *b) {
  int index = b->blockno % BUCKETS;
  acquire(&bcache.bucket_locks[index]);
  b->refcnt--;
  release(&bcache.bucket_locks[index]);
}


