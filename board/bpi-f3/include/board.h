#ifndef __BOARD_VF2_H__
#define __BOARD_VF2_H__

#include "board_memmap.h"
#include "k1-x-pinctrl.h"


#define __SPACEMIT_K1__

#define CONFIG_MP_NUM_CPUS      8   /* all harts */
#define CONFIG_NUM_IRQS  159 // Num interrupts
#define NR_IRQS    CONFIG_NUM_IRQS

#define CONFIG_SYS_TIMER_RATE 24000000;

#define HEARTBEAT_PIN   DVL0
#define HEARTBEAT_SEL   (MUX_MODE1 | EDGE_NONE | PULL_DOWN | PAD_1V8_DS2)


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