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

#define NBUCKET 13
// #define HASH(blockno) blockno % NBUCKET

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  int size;
  struct buf bucket[NBUCKET];
  struct spinlock bucket_lock[NBUCKET];
  struct spinlock hash_lock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

// struct {
//   struct buf bucket[NUM_LOCKS];
//   struct spinlock bucket_lock[NUM_LOCKS];
// }hash_table;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  initlock(&bcache.hash_lock,"bcache_hash_lock");
  for(int i=0; i < NBUCKET;i++){
    initlock(&bcache.bucket_lock[i],"bcache_bucket");    
  }

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    // acquire(&bcache.lock);
    // int index = hash(bcache.buf->blockno);
    // acquire(&hash_table.bucket_lock[index]);
    // hash_table.bucket[index] = &bcache.buf;
    // release(&hash_table.bucket_lock[index]);
    // release(&bcache.lock);
  }
  
}

int hash(uint blockno){
  return blockno % NBUCKET; 
}

void hashToBucket(struct buf *b){
  int index = hash(b->blockno);
  acquire(&bcache.bucket_lock[index]);
  struct buf *p = bcache.bucket[index].next;
  struct buf *q = &bcache.bucket[index];
  if(!p){
    bcache.bucket[index].next = b;
    b->next = 0;
  }
  while(p){
    if(p->refcnt == 0){ //fill or replace
      q->next = b;
      b->next = p;
    }
    q = p;
    p = p -> next;
  }
  release(&bcache.bucket_lock[index]);
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b,*pre,*minb=0,*minpre=0;
  uint minitimestamp;

  int index = hash(blockno);
  acquire(&bcache.bucket_lock[index]);
  b = bcache.bucket[index].next;
  //cached
  //may collision
  while(b){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
    b = b -> next;
  }

  acquire(&bcache.lock);
  if(bcache.size < NBUF){
    b = &bcache.buf[bcache.size++];
    release(&bcache.lock);
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next = bcache.bucket[index].next;
    bcache.bucket[index].next = b;
    release(&bcache.bucket_lock[index]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  release(&bcache.bucket_lock[index]);

  acquire(&bcache.hash_lock);
  for(int i=0;i < NBUCKET; i++){
    minitimestamp = -1;
    acquire(&bcache.bucket_lock[index]);
    for(pre = &bcache.bucket[index], b = pre->next;b;pre = b, b = b->next){
      if(index == hash(blockno) && b->dev == dev && b->blockno == blockno){
        b->refcnt++;
        release(&bcache.bucket_lock[index]);
        release(&bcache.hash_lock);
        acquiresleep(&b->lock);
        return b;
      }
      if(b->refcnt == 0 && b->bticks < minitimestamp){
        minitimestamp = b->bticks;
        minb = b;
        minpre = pre;
      }
    }
    if(minb){
      b = minb;
      pre = minpre;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      if(index != hash(blockno)){
        minpre -> next = minb -> next;
        release(&bcache.bucket_lock[index]);
        index = hash(blockno);
        acquire(&bcache.bucket_lock[index]);
        b->next = bcache.bucket[index].next;
        bcache.bucket[index].next = b;
      }
      release(&bcache.bucket_lock[index]);
      release(&bcache.hash_lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bcache.bucket_lock[index]);
    if(++index == NBUCKET){
      index = 0;
    }
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

  releasesleep(&b->lock);

  // acquire(&bcache.lock);
  int index = hash(b->blockno);
  acquire(&bcache.bucket_lock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // b->next->prev = b->prev;
    // b->prev->next = b->next;
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // bcache.head.next->prev = b;
    // bcache.head.next = b;
    b->bticks = ticks;
  }
  
  release(&bcache.bucket_lock[index]);
}

void
bpin(struct buf *b) {
  int index = hash(b->blockno);
  acquire(&bcache.bucket_lock[index]);
  b->refcnt++;
  release(&bcache.bucket_lock[index]);
}

void
bunpin(struct buf *b) {
  int index = hash(b->blockno);
  acquire(&bcache.bucket_lock[index]);
  b->refcnt--;
  release(&bcache.bucket_lock[index]);
}


