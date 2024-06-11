#include <common.h>

extern char _bss_start, _bss_end;



u64 boot_cpu_hartid;
static char* version = VERSION_STR;

#define FB	(u8*)0xfe000000
#define FB_LEN	1920*1080



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

    /* Prepare the page tables memory region */
    memset(&_pgtable_start, 0, (&_pgtable_end - &_pgtable_start));

    /* Crashing the screen image. test :) */
    int x = 0;
    for(int i = 0; i < FB_LEN; i++) {
	*(FB+(i*4)) = x++;
	*(FB+(i*4)+1) = x++;
	*(FB+(i*4)+2) = x++;
	*(FB+(i*4)+3) = 0x00;
    }

    lib_puts("myOS version ");
    lib_puts( version);
    lib_putc('\n');
    printf("Boot HART is 0x%lX\n", boot_cpu_hartid);

    mm_init();

    while (1) {}
    return 0;
}