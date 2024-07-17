
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>
#include <printf.h>
#include <string.h>
#include <board.h>

typedef enum { false, true } bool;

#define locate_data(n) __attribute__((section(n)))
#define _ALIGN(n) __attribute__ ((aligned (n)))
/*
 * Define __packed if someone hasn't beat us to it.  Linux kernel style
 * checking prefers __packed over __attribute__((packed)).
 */
#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define BIT(nr)			(1 << (nr))

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


#define debug	printf


/* Kernel code memory (RX) */


#define KSTART      (uintptr_t)_start
#define STACK_TOP       (uintptr_t)_stack_top
#define BSS_START     (uintptr_t)_bss_start
#define BSS_END       (uintptr_t)_bss_end


/****************************************************************************
 * Public Data
 ****************************************************************************/


/* Kernel RAM (RW) */

extern uint8_t          __ksram_start[];
extern uint8_t          __ksram_size[];
extern uint8_t          __ksram_end[];

/* Page pool (RWX) */

extern uint8_t          __pgheap_start[];
extern uint8_t          __pgheap_size[];

extern uint8_t      _start[];
extern uint8_t      _stack_top[];
extern uint8_t      _bss_start[];
extern uint8_t      _bss_end[];



void _putchar(char character);

void lib_putc(char ch);
void lib_puts(char *s);

void panic(char*) __attribute__((noreturn));

void udelay(unsigned long usec);
void *malloc(uint64 size);
void mfree(void *ptr);


#endif /* _COMMON_H */