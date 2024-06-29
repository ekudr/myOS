
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>
#include <printf.h>
#include <string.h>


#define locate_data(n) __attribute__((section(n)))

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))


void _putchar(char character);

void lib_putc(char ch);
void lib_puts(char *s);

void panic(char*) __attribute__((noreturn));


#endif /* _COMMON_H */