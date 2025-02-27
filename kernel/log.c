#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;  // 当前日志中已记录的块数
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(int recovering)
{
  int tail;

  // 遍历日志头中记录的所有块号
  for (tail = 0; tail < log.lh.n; tail++) {
    // 从磁盘的日志区域读取一个日志块
    struct buf *lbuf = bread(log.dev, log.start+tail+1); 
    // 从磁盘的目标位置读取一个目标块
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); 
    // 将日志块的数据复制到目标块中
    memmove(dbuf->data, lbuf->data, BSIZE);  
    // 将目标块写入磁盘
    bwrite(dbuf);  
    // 如果不是恢复模式，减少目标块的引用计数
    if(recovering == 0)
      bunpin(dbuf);
    // 释放日志块
    brelse(lbuf);
    // 释放目标块
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  // 从磁盘读取日志头块
  struct buf *buf = bread(log.dev, log.start);
  // 将缓冲区的数据转换为日志头结构体指针
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  // 将内存中日志头结构体的块数复制到磁盘日志头块中
  hb->n = log.lh.n;
  // 遍历内存中日志头结构体记录的所有块号
  for (i = 0; i < log.lh.n; i++) {
    // 将内存中日志头结构体记录的块号复制到磁盘日志头块中
    hb->block[i] = log.lh.block[i];
  }
  // 将修改后的日志头块写回磁盘
  bwrite(buf);
  // 释放日志头块，减少其引用计数
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk 参数 1 表示当前处于恢复模式
  log.lh.n = 0;   // 清空内存中日志头结构体记录的块数
  write_head(); // clear the log // 将清空后的日志头信息写回磁盘，以清空日志
}

// called at the start of each FS system call.
// 主要目的是确保在系统调用开始时，日志系统有足够的空间来记录该操作的更改，并且当前没有正在进行的提交操作
void
begin_op(void)
{
  acquire(&log.lock); // 确保在检查和更新日志状态时的线程安全。
  while(1){
    if(log.committing){
      sleep(&log, &log.lock); // 说明当前正在进行日志提交操作。此时，函数调用 sleep 函数，将当前线程挂起，直到日志提交完成。
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1; // 表示有一个新的操作开始
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0; // 标记提交操作已完成
    wakeup(&log); // 唤醒可能正在等待提交完成的其他进程
    release(&log.lock); // 释放锁
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  // 遍历日志头中记录的所有块号
  for (tail = 0; tail < log.lh.n; tail++) {
    // 从磁盘的日志区域读取一个日志块
    struct buf *to = bread(log.dev, log.start+tail+1); 
    // 从磁盘的缓存区域读取一个缓存块
    struct buf *from = bread(log.dev, log.lh.block[tail]); 
    // 将缓存块的数据复制到日志块中
    memmove(to->data, from->data, BSIZE);
    // 将日志块写入磁盘
    bwrite(to);  
    // 释放缓存块
    brelse(from);
    // 释放日志块
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(0); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
// 主要功能是将修改后的缓冲区数据记录到日志中，而不是直接写入磁盘。
void
log_write(struct buf *b)
{
  int i;

  // 加锁以确保线程安全，防止多个线程同时修改日志状态
  acquire(&log.lock);

  // 检查日志是否有足够的空间来记录新的块
  // 如果日志中的块数已经达到最大日志大小或者日志块数达到了日志空间的上限，抛出错误
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");

  // 检查是否有正在进行的文件系统操作
  // 如果没有正在进行的操作，说明这个日志写入操作是在事务之外进行的，抛出错误
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  // 遍历日志头中的块号数组，检查当前缓冲区的块号是否已经存在于日志中
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorption
      break;
  }

  // 将当前缓冲区的块号记录到日志头的块号数组中
  log.lh.block[i] = b->blockno;

  // 如果块号不在日志中，说明这是一个新的块，需要添加到日志中
  if (i == log.lh.n) {  
    // 增加缓冲区的引用计数，防止其被释放
    bpin(b);  
    log.lh.n++; // 增加日志中记录的块数
  }

  // 释放锁，允许其他线程访问日志
  release(&log.lock);
}

