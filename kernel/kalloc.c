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

// need lock
int mref[NPAGE]; //pages references counter

void refinit()
{
  int i;
  for(i = 0; i<NPAGE; i++)
    mref[i] = 1;
}
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  refinit();
  freerange(end, (void*)PHYSTOP);
  //memset((void *)mref, 0, sizeof mref);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  // only free when the ref equals 1
  int idx = PAX(pa); 

  acquire(&kmem.lock);
  if(mref[idx] == 0)
    panic("kfree(): zero ref");
  // hold by other process
  if(mref[idx] > 1) {
    mref[idx]--;
    release(&kmem.lock);
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  //acquire(&kmem.lock);
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
  if(r){
    kmem.freelist = r->next;
    // the first process hold it from kalloc
    mref[PAX(r)] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// increase page ref for shared or cow page
void duppage(uint64 pa){
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("duppage(): invalid pa");
  int idx = PAX(pa);
  acquire(&kmem.lock);
  if(mref[idx] == 0)
    panic("duppage(): dup free page");
  mref[idx]++;
  release(&kmem.lock);
}
