
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#ifndef __ASSEMBLY__
#  include <stdint.h>
#endif

/* pid_t is used for process IDs and process group IDs. It must be signed
 * because negative PID values are used to represent invalid PIDs.
 */

typedef int          pid_t;

#endif /* _SYS_TYPES_H */