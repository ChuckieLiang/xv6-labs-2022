// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// 新增一个全局struct，用于表示物理页的引用计数 (lab5 COW)
struct COW_struct {
  struct spinlock lock;
  int useRef[(PHYSTOP - KERNBASE) >> 12];
};

struct COW_struct cow_counting;

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

// COW结构的初始化函数
void
cowinit()
{
  initlock(&cow_counting.lock, "cowlock");
  for (int i = 0; i < ((PHYSTOP - KERNBASE) >> 12); i++) {
    cow_counting.useRef[i] = 1;
  }
}

void
cowinc(void *pa)
{
  acquire(&cow_counting.lock);
  cow_counting.useRef[COWINDEX((uint64)pa)]++;
  release(&cow_counting.lock);
}

void
cowdec(void *pa)
{
  acquire(&cow_counting.lock);
  cow_counting.useRef[COWINDEX((uint64)pa)]--;
  release(&cow_counting.lock);
}

int
cowgetref(void *pa)
{
  return cow_counting.useRef[COWINDEX((uint64)pa)];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  cowinit();
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

  // Fill with junk to catch dangling refs.
  cowdec(pa);
  if(cowgetref(pa) == 0){    
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
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
    cowinc((void*)r);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
