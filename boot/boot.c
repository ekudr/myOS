#include <common.h>
#include <string.h>

extern char _bss_start, _bss_end;
extern char _pgtable_start, _pgtable_end;


u64 boot_cpu_hartid;
static char* version = VERSION_STR;

#define FB	(u8*)0xfe000000
#define FB_LEN	1920*1080

void plic_init(void); 
void tasks_init(void);
void mm_init(void);


void memset(void *b, int c, int len)
    {
       char *s = b;

        while(len--)
            *s++ = c;
    }



int boot_start(void)
{

    /* Prepare the bss memory region */
//    memset(&_bss_start, 0, (&_bss_end - &_bss_start));


    /* Crashing the screen image. test :) */
    int x = 0;
    for(int i = 0; i < FB_LEN; i++) {
	*(FB+(i*4)) = x++;
	*(FB+(i*4)+1) = x++;
	*(FB+(i*4)+2) = x++;
	*(FB+(i*4)+3) = 0x00;
    }

    sbi_ecall_console_puts("\nTest SBI console output\n");


    lib_puts("myOS version ");
    lib_puts( version);
    lib_putc('\n');
    printf("Boot HART is 0x%lX\n", boot_cpu_hartid);

    mm_init();

    printf("[SCHED] init tasks ... ");
    tasks_init();
    printf("Done.\n");

	printf("[PLIC] init interrupts ... ");
	plic_init();
    printf("Done.\n");

	printf("[CONSOLE] init ... ");
	uart_init();
	printf("Done.\n");

	__sync_synchronize();
	printf("[SCHED] cpu id = 0x%lX\n", cpuid()) ;
	scheduler(); 
}