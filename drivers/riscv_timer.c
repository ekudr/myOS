#include <common.h>



static inline uint64_t
csr_read_time(void)
{
	uint64_t x;
	asm volatile ("csrr %0, 0xc01" : "=r" (x) :	  : "memory");
	return x;
}

uint64_t timer_get_count(void)
{
	return csr_read_time();
}



// rewrite
// NOT usec.  count ticks
void udelay(unsigned long usec)
{

	uint64_t tmp;

	tmp = timer_get_count() + (usec * 2);	/* get current timestamp */

	while (timer_get_count() < tmp+1)	/* loop till event */
		 /*NOP*/;
}