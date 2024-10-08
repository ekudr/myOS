#include <stdint.h>
#include <syscall.h>




//
// wrapper so that it's OK if main() does not call exit().
//
void
_start()
{
    int ret;
    extern int main();
    ret = main();
    exit(ret);
}

// Print string
void
lib_puts(char *s) {  
    while (*s) {
        putc(*s++);
    }
}
