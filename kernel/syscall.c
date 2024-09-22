#include <common.h>
#include <sched.h>
#include <syscall.h>
#include <ext4.h>




// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct task *t = mytask();
  if(addr >= t->sz || addr+sizeof(uint64) > t->sz) // both tests needed, in case of overflow
    return -1;
  if(copyin(t->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct task *t = mytask();
  if(copyinstr(t->pagetable, buf, addr, max) < 0)
    return -1;
  return sbi_strlen(buf);
}

static uint64
argraw(int n)
{
  struct task *t = mytask();
  switch (n) {
  case 0:
    return t->trapframe->a0;
  case 1:
    return t->trapframe->a1;
  case 2:
    return t->trapframe->a2;
  case 3:
    return t->trapframe->a3;
  case 4:
    return t->trapframe->a4;
  case 5:
    return t->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_putc(void)
{
    int c;
    argint(0, &c);
    console_putc(c);
    return 0;
}

uint64
sys_exec(void)
{
  char  *argv[32];
  char path[256];  // should be EXT2_PATH_MAX
  int i;
  uint64 uargv, uarg;

  //path = malloc(EXT2_PATH_MAX);

  if(argstr(0, path, EXT2_PATH_MAX) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = pg_alloc(); /// rewrite to malloc()
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PAGESIZE) < 0)
      goto bad;
  }
    debug("EXEC %s\n", path);
  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    pg_free(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    pg_free(argv[i]);
  return -1;
}

// Prototypes for the functions that handle system calls.
//extern uint64 sys_fork(void);
//extern uint64 sys_exit(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
    [SYS_fork]    sys_fork,
    [SYS_exit]    sys_exit,
    [SYS_exec]    sys_exec,
    [SYS_putc]    sys_putc,
};

void
syscall(void)
{
  int num;
  struct task *t = mytask();

  num = t->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
//    debug("SYSCALL %d handler 0x%lX\n", num, syscalls[num]);
    t->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            t->pid, t->name, num);
    t->trapframe->a0 = -1;
  }
}