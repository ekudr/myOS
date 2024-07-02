#include <common.h>



static inline uint64
csr_read_time(void)
{
	uint64 x;
	asm volatile ("csrr %0, 0xc01" : "=r" (x) :	  : "memory");
	return x;
}

uint64 timer_get_count(void)
{
	return csr_read_time();
}