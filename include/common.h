
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>
#include <printf.h>
#include <string.h>


#define locate_data(n) __attribute__((section(n)))
#define _ALIGN(n) __attribute__ ((aligned (n)))

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))


void _putchar(char character);

void lib_putc(char ch);
void lib_puts(char *s);

void panic(char*) __attribute__((noreturn));

uint64 timer_get_count(void);
void *malloc(uint64 size);
void mfree(void *ptr);


#endif /* _COMMON_H */