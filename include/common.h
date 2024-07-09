
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>
#include <printf.h>
#include <string.h>

typedef enum { false, true } bool;

#define locate_data(n) __attribute__((section(n)))
#define _ALIGN(n) __attribute__ ((aligned (n)))


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


void _putchar(char character);

void lib_putc(char ch);
void lib_puts(char *s);

void panic(char*) __attribute__((noreturn));

uint64_t timer_get_count(void);
void udelay(unsigned long usec);
void *malloc(uint64 size);
void mfree(void *ptr);


#endif /* _COMMON_H */