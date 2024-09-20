#include <common.h>

#define STARFIVE_JH7110_L2CC_FLUSH_START 0x40000000UL
#define STARFIVE_JH7110_L2CC_FLUSH_SIZE 0x400000000UL

#define L2_CACHE_FLUSH64	0x200
#define L2_CACHE_BASE_ADDR 	0x2010000UL

#define CONFIG_SYS_CACHELINE_SIZE 64


#define RISCV_FENCE(p, s) \
	__asm__ __volatile__ ("fence " #p "," #s : : : "memory")

/* These barriers need to enforce ordering on both devices or memory. */
#define mb()		RISCV_FENCE(iorw,iorw)
#define rmb()		RISCV_FENCE(ir,ir)
#define wmb()		RISCV_FENCE(ow,ow)

void flush_dcache_range(unsigned long start, unsigned long end)
{
	unsigned long line;
	volatile unsigned long *flush64;

	/* make sure the address is in the range */
	if(start > end ||
		start < STARFIVE_JH7110_L2CC_FLUSH_START ||
		end > (STARFIVE_JH7110_L2CC_FLUSH_START +
				STARFIVE_JH7110_L2CC_FLUSH_SIZE))
		return;

	/*In order to improve the performance, change base addr to a fixed value*/
	flush64 = (volatile unsigned long *)(L2_CACHE_BASE_ADDR + L2_CACHE_FLUSH64);

	/* memory barrier */
	mb();
	for (line = start; line < end; line += CONFIG_SYS_CACHELINE_SIZE) {
		(*flush64) = line;
		/* memory barrier */
		mb();
	}

	return;
}