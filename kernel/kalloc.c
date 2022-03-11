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
void _kinit();

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
} kmems[NCPU];

// note that kinit() is exceuted in main in cpu0
void
kinit()
{
  struct kmem * kmem;

  // hard code here, since I don't know how to get
  // number of cores dynamicly.
  for(kmem = kmems; kmem < kmems + 3; kmem++) 
    initlock(&kmem->lock, "kmem");
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
  struct kmem * kmem;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  

  push_off();

  kmem = kmems + cpuid();

  acquire(&kmem->lock);
  r->next = kmem->freelist;
  kmem->freelist = r;
  release(&kmem->lock);

  pop_off();
}

// Must be called with interrupts disabled,
// kmems[cpuid()].lock must be hold.
void *
steal(void)
{ 
  struct run *r = 0, *slow, *fast;
  struct kmem * kmem;
  int hartid = cpuid();

  if(intr_get())
    panic("steal(): interrupt enable");

  for(int i = 0; i < 3; i++) {
    // Avoid acquiring a lock that is already hold.
    if(i == hartid)
      continue;
    kmem = kmems + i;

    acquire(&kmem->lock);

    r = kmem->freelist;
    // bug: fucking stupid
    // if(r == 0) continue;
    if(!r){
      release(&kmem->lock);
      continue;
    }
    slow = r;
    fast = slow->next;

    while(fast){
      fast = fast->next;
      if(fast)
        fast = fast->next;
      slow = slow->next;
    }
    
    kmem->freelist = slow->next;
    slow->next = 0;

    release(&kmem->lock);

    return r;
  }
  
  if(r)
    panic("steal");
  return r;
}

void *
kalloc(void)
{
  struct run *r = 0;
  struct kmem * kmem;

  push_off();

  kmem = kmems + cpuid();

  acquire(&kmem->lock);

  r = kmem->freelist;
  if(!r)
    r = steal();
  if(r)
    kmem->freelist = r->next;

  release(&kmem->lock);
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE);
  return (void*)r;
}
