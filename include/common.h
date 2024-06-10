
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>


#  define locate_data(n) __attribute__((section(n)))

void lib_putc(char ch);
void lib_puts(char *s);

int printf(const char *format, ...);
int sprintf(char *out, const char *format, ...);

#endif /* _COMMON_H */