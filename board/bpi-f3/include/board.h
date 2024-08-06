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


#endif /* __BOARD_VF2_H__ */