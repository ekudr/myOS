
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#ifndef __ASSEMBLY__
#  include <stdint.h>
#endif


/* Use uintreg_t for register-width integers */

#ifdef CONFIG_ARCH_RV32
typedef _int32_t           intreg_t;
typedef _uint32_t          uintreg_t;
#else
typedef _int64_t           intreg_t;
typedef _uint64_t          uintreg_t;
#endif

/* pid_t is used for process IDs and process group IDs. It must be signed
 * because negative PID values are used to represent invalid PIDs.
 */

typedef int          pid_t;

#endif /* _SYS_TYPES_H */