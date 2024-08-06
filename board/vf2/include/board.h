#ifndef __BOARD_VF2_H__
#define __BOARD_VF2_H__

#include "jh7110_memmap.h"

#define __JH7110__

#define CONFIG_MP_NUM_CPUS      4    /* all harts */

#define CONFIG_NUM_IRQS  136 // Num interrupts
#define NR_IRQS    CONFIG_NUM_IRQS

#define CONFIG_SYS_TIMER_RATE 4000000;

#endif /* __BOARD_VF2_H__ */