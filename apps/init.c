
#include "user.h"

char *hello="myOS init process\n";
int HT;

char *argv[] = { "/sh", 0 };

int
main(void)
{
    lib_puts(hello);
    lib_puts("ver. 0\n");
    exec("/sh", argv);
    for(;;){}
    return 0;
}