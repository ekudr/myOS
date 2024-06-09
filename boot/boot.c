#include <common.h>

unsigned long boot_cpu_hartid;

#define FB	(u8*)0xfe000000
#define FB_LEN	1920*1080


void lib_putc(char ch);


void lib_puts(char *s) {
    while (*s) lib_putc(*s++);

}


int boot_start(void)
{
    int x = 0;

    for(int i = 0; i < FB_LEN; i++) {
	*(FB+(i*4)) = x++;
	*(FB+(i*4)+1) = x++;
	*(FB+(i*4)+2) = x++;
	*(FB+(i*4)+3) = 0x00;
    }

    lib_puts("myOS version 0.1 build 5\n");
    
    while (1) {}
    return 0;
}