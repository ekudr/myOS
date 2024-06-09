


#ifndef _ASM_H
#define _ASM_H




.macro load_global_pointer
.option push
.option norelax
	la gp, __global_pointer$
.option pop
.endm

#endif /* _ASM_H */