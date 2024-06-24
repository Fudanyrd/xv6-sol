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

static unsigned char *ref_counts;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// map a physical address to slot to page-counts
int 
pa2ref_idx(uint64 pa) {
  if (pa < (uint64)end) {
    printf("%p %p\n", pa, end);
    panic("pa2ref_idx");
  }
  return (pa - (uint64)end) / PGSIZE;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  ref_counts = (unsigned char *)end;
  printf("start of ref: %p\n", ref_counts);

  for (int i = 0; i < 32768; ++i) {
    ref_counts[i] = 1U;
  }
  freerange(end + 32768, (void*)PHYSTOP);
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    printf("%p %p\n", pa, end);
    panic("kfree");
  }

  // decrement ref count
  acquire(&kmem.lock);
  int idx = pa2ref_idx((uint64) pa);
  if (ref_counts[idx] == 0U) {
    release(&kmem.lock);
    panic("kfree: ref count is 0\n");
  }
  if (--ref_counts[idx] > 0U) {
    // still have refs to it!
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);

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
  else {
    release(&kmem.lock);
    return 0;
  }
  int idx = pa2ref_idx((uint64)r);
  ++ref_counts[idx];
  release(&kmem.lock);

    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// realloc page: simply increment ref count!
// panic if the page is not allocated by kalloc before.
void *
krealloc(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krealloc");

  acquire(&kmem.lock); 

  const int idx = pa2ref_idx((uint64) pa);
  if (ref_counts[idx] == 0) {
    release(&kmem.lock); 
    panic("krealloc, 129");
  } 
  ++ref_counts[idx];
  release(&kmem.lock); 

  return pa;
}

// make a unique copy from a page, return 0 if cannot fetch new pages.
void *
kmake_unique(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kmake_unique");

  acquire(&kmem.lock); 
  const int idx = pa2ref_idx((uint64)pa);  // index into the ref counts
  if (ref_counts[idx] == 0) {
    // not allocated, abort.
    release(&kmem.lock);
    return 0;
  }

  if (ref_counts[idx] == 1) {
    // this is not a shared page, just return pa.
    release(&kmem.lock);
    return pa;
  }

  --ref_counts[idx];
  // look for a new page.
  struct run *r;
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // set ref count of this unique page
    ref_counts[pa2ref_idx((uint64)r)] = 1;
    // copy the data of the page
    memmove(r, pa, PGSIZE);
  } 
  release(&kmem.lock);

  return (void *)r;
}
