#include <stdint.h>
#include <syscall.h>

int exit(int) __attribute__((noreturn));


//
// wrapper so that it's OK if main() does not call exit().
//
void
_start()
{
  extern int main();
  main();
  exit(0);
}


