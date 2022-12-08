// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#define PA2INDEX(pa) ((uint64)pa>>12)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint16 refcount[PA2INDEX(PHYSTOP)];
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  memset(kmem.refcount, 0, sizeof(kmem.refcount));
}


uint32 
krefcount(void *pa){
 uint32 ret = 0;
 acquire(&kmem.lock);
 ret = kmem.refcount[PA2INDEX(pa)];
 release(&kmem.lock);
 return ret;
}

void 
krefadd(void *pa){
 acquire(&kmem.lock);
 kmem.refcount[PA2INDEX(pa)]++;
 release(&kmem.lock);
}

void 
krefminus(void *pa){
 acquire(&kmem.lock);
 kmem.refcount[PA2INDEX(pa)]--;
 release(&kmem.lock);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // if the page's refcount is zero, then the page can be released
  if(krefcount(pa) > 0){
  	krefminus(pa);	 
	  if(krefcount(pa) > 0) //not zero, don't release
  	 	return;
  }

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

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    krefadd((void *)r);
  }
  return (void*)r;
}

