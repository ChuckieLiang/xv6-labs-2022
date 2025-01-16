#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;  //所有核心可以访问到的全局变量（位于内存中），用于同步

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if(cpuid() == 0){ //核心0执行以下代码，核心0负责初始化
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode table
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize(); //阻止编译优化（编译器可能重新排列代码），在做完上述任务前不要执行后面的语句
    started = 1;
  } else {  //其他核心执行的代码
    while(started == 0) //等待核心0初始化完成
      ;
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        // 调度器，寻找一个要进行的进程
}
