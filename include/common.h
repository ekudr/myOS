
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>
#include <printf.h>


#define locate_data(n) __attribute__((section(n)))

#define _putchar    lib_putc

void lib_putc(char ch);
void lib_puts(char *s);



#endif /* _COMMON_H */