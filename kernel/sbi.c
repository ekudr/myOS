#include <sbi_ecall_interface.h>
#include <common.h>
#include <string.h>


/* default SBI version is 0.1 */
unsigned long sbi_version = 0x1; 

struct sbiret {
	unsigned long error;
	unsigned long value;
};

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5)
{
	struct sbiret ret;

	register unsigned long a0 asm ("a0") = (unsigned long)(arg0);
	register unsigned long a1 asm ("a1") = (unsigned long)(arg1);
	register unsigned long a2 asm ("a2") = (unsigned long)(arg2);
	register unsigned long a3 asm ("a3") = (unsigned long)(arg3);
	register unsigned long a4 asm ("a4") = (unsigned long)(arg4);
	register unsigned long a5 asm ("a5") = (unsigned long)(arg5);
	register unsigned long a6 asm ("a6") = (unsigned long)(fid);
	register unsigned long a7 asm ("a7") = (unsigned long)(ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}

static inline long sbi_get_version(void) {

    struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_GET_SPEC_VERSION, 0, 0, 0, 0, 0, 0);

	if (!ret.error)
		return ret.value;
	else
		return ret.error;
    
}

/**
 * sbi_probe_extension() - Check if an SBI extension ID is supported or not.
 * @extid: The extension ID to be probed.
 *
 * Return: 1 or an extension specific nonzero value if yes, 0 otherwise.
 */
long sbi_probe_extension(int extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, extid,
			0, 0, 0, 0, 0);
	if (!ret.error)
		return ret.value;

	return 0;
}

void sbi_set_timer(uint64_t stime_value) {
#if __riscv_xlen == 32
	sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
#else
	sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, stime_value, 0,
		  0, 0, 0, 0);
#endif
}

int sbi_hsm_hart_get_status(unsigned long hartid) {
    struct sbiret ret;
    ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_GET_STATUS,
        hartid, 0, 0, 0, 0, 0);
    if (!ret.error)
        return ret.value;
    else
        return ret.error;
}

void sbi_console_putc(char ch) {
    sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}

int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv) {
    struct sbiret ret;

    ret = sbi_ecall(SBI_EXT_HSM, SBI_EXT_HSM_HART_START, hartid, saddr, priv, 0, 0, 0);

    if (!ret.error)
        return ret.value;
    else
        return ret.error;
}

void sbi_init (void) {
    int ret;

    ret = sbi_get_version();

    if (ret) sbi_version = ret;

    printf("SBI detected version %d.%d\n", (sbi_version >> 24) & 0x7f, sbi_version & 0x7f);

if (sbi_probe_extension(SBI_EXT_HSM)) {
			
	printf("SBI HSM extension detected\n");
	for(int i=0; i<CONFIG_MP_NUM_CPUS; i++){
		printf("SBI HSM hart %d status %d\n", i, sbi_hsm_hart_get_status(i));
	}
}
    
}

