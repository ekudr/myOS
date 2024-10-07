#ifndef __BOARD_VF2_H__
#define __BOARD_VF2_H__

#include "jh7110_memmap.h"

#define __JH7110__

#define CONFIG_MP_NUM_CPUS      4    /* all harts */

#define CONFIG_NUM_IRQS  136 // Num interrupts
#define NR_IRQS    CONFIG_NUM_IRQS

#define CONFIG_SYS_TIMER_RATE 4000000;

int board_timer_init(void);
int board_timer_reset(void);
int board_timer_stop(void);
void board_timer_wait(void);
int board_timer_set_irq(void);
void led_init(void );
void led_on(void );
void led_off(void);
void led_switch(void);

// chache.c
void flush_dcache_range(unsigned long start, unsigned long end);

#endif /* __BOARD_VF2_H__ */