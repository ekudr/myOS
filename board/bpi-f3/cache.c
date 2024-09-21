#include <common.h>

#define RISCV_CBOM_BLOCK_SIZE   64

#define cbo_clean(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("cbo.clean"	\
							 " 0(%0)"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})

#define cbo_invalid(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("cbo.inval"	\
							 " 0(%0)"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})

#define cbo_flush(start)			\
	({								\
		unsigned long __v = (unsigned long)(start); \
		__asm__ __volatile__("cbo.flush"	\
							 " 0(%0)"		\
							 :				\
							 : "rK"(__v)	\
							 : "memory");	\
	})

int check_cache_range(unsigned long start, unsigned long end)
{
	int ok = 1;

	if (start & (RISCV_CBOM_BLOCK_SIZE - 1))
		ok = 0;

	if (end & (RISCV_CBOM_BLOCK_SIZE - 1))
		ok = 0;

	if (!ok) {
		debug("CACHE: Misaligned operation at range [%08lx, %08lx]\n",
			start, end);
	}

	return ok;
}

void flush_dcache_range(unsigned long start, unsigned long end)
{
  /*  
	if (!check_cache_range(start, end))
		return;

	while (start < end) {
//		cbo_flush(start);
		start += RISCV_CBOM_BLOCK_SIZE;
	}
*/
}