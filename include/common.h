
#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <config.h>


#  define locate_data(n) __attribute__((section(n)))

#endif /* _COMMON_H */