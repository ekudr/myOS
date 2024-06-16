
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>
#include <printf.h>


#define locate_data(n) __attribute__((section(n)))

void _putchar(char character);

void lib_putc(char ch);
void lib_puts(char *s);

void panic(char*) __attribute__((noreturn));

void memset(void *b, int c, int len);

#endif /* _COMMON_H */