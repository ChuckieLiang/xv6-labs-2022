#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va;  // 待检测用户页面的起始虚拟地址
  int nums; // 待检测用户页面的数量
  uint64 maskaddr; // 用于记录检测结果的掩码的虚拟地址（来自用户空间）
  argaddr(0, &va);
  argint(1, &nums);
  if(nums <= 0 || nums > 64) {  // 可扫描页面数量的上限
    return -1;
  }
  argaddr(2, &maskaddr);

  struct proc *p = myproc();
  uint64 mask = 0;
  // 遍历待检测的用户页面，调用 walk 函数获取对应的 PTE
  for(int i = 0; i < nums; i++) {
    pte_t *pte = walk(p->pagetable, va + i * PGSIZE, 0);
    if(pte && (*pte & PTE_V) && (*pte & PTE_A)) {
      // 如果 PTE 存在且有效且已访问过，则将掩码对应位置为 1
      mask |= (1 << i);
      *pte &= ~PTE_A; // 清除 PTE_A 位
    } 
  }
  // 将掩码复制到用户空间
  if(copyout(p->pagetable, maskaddr, (char *)&mask, sizeof(mask)) < 0) {
    return -1;
  }
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
