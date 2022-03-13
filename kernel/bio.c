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

#define NBUCKET 103 

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

struct {
  struct buf* next;
  struct spinlock lock;
} table[NBUCKET];

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }

  for(int i = 0; i < NBUCKET; i++) {
    initlock(&table[i].lock, "bcache");
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int key = blockno % NBUCKET;

  acquire(&table[key].lock);
  for(b = table[key].next; b != 0; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&table[key].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  acquire(&bcache.lock);
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) { 
    // precheck to avoid acquire lock of a in-use buf
    if(b->refcnt != 0) continue;
    int oldkey = b->blockno % NBUCKET;
    
    // need no more lock, just modify the buf 
    if(b->blockno == 0 || oldkey == key) {
      // use for the first time, put it into hash table
      // do this before b->blockno modified
      if(b->blockno == 0){
        // oldkey is invalid for a buf never used
        b->next = table[key].next;
        table[key].next = b; 
      }
      // otherwise, the buf is already in right bucket
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      release(&bcache.lock);
      release(&table[key].lock);
      acquiresleep(&b->lock);
      return b;
    }
    
    // we need another bucket lock
    // check refcnt after acquire the lock, since
    // the refcnt may be changed by other threads
    acquire(&table[oldkey].lock);
    if(b->refcnt != 0){
      release(&table[oldkey].lock);
      continue;
    }
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1; 

    // move the buf from table[oldkey] to table[key]
    struct buf *pre;
    if(table[oldkey].next == b)
      table[oldkey].next = b->next;
    else{
    for(pre = table[oldkey].next; pre != 0; pre = pre->next) {
      if(pre->next == b){
        pre->next = b->next;
        break;
      }
    }
    }
    b->next = table[key].next;
    table[key].next = b;
     
    release(&table[oldkey].lock);
    release(&bcache.lock);
    release(&table[key].lock);

    acquiresleep(&b->lock);
    return b;
  }
  // remember to release all lock in every branch
  release(&bcache.lock);
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
  
  int key = b->blockno % NBUCKET;
  acquire(&table[key].lock);
  b->refcnt--;
  release(&table[key].lock);
}

void
bpin(struct buf *b) {
  int key = b->blockno % NBUCKET;
  acquire(&table[key].lock);
  b->refcnt++;
  release(&table[key].lock);
}

void
bunpin(struct buf *b) {
  int key = b->blockno % NBUCKET;
  acquire(&table[key].lock);
  b->refcnt--;
  release(&table[key].lock);
}
