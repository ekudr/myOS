#include <common.h>

extern char _bss_start, _bss_end;



u64 boot_cpu_hartid;

#define FB	(u8*)0xfe000000
#define FB_LEN	1920*1080


void lib_putc(char ch);


void lib_puts(char *s) {
    while (*s) lib_putc(*s++);

}

    void memset(void *b, int c, int len)
    {
       char *s = b;

        while(len--)
            *s++ = c;
    }

int boot_start(void)
{

    /* Prepare the bss memory region */
    memset(&_bss_start, 0, (&_bss_end - &_bss_start));


    int x = 0;

    for(int i = 0; i < FB_LEN; i++) {
	*(FB+(i*4)) = x++;
	*(FB+(i*4)+1) = x++;
	*(FB+(i*4)+2) = x++;
	*(FB+(i*4)+3) = 0x00;
    }

    lib_puts("myOS version %s\n", VERSION_STR);
    printf("Boot HART is %%d\n", boot_cpu_hartid);
    printf("Loading ...");
    while (1) {}
    return 0;
}