
#ifndef _STDINT_H
#define _STDINT_H


#ifndef NULL
  /* SDCC is sensitive to NULL pointer type conversions, and C++ defines
   * NULL as zero
   */

#  if defined(SDCC) || defined(__SDCC) || defined(__cplusplus)
#    define NULL (0)
#  else
#    define NULL ((void*)0)
#  endif
#endif

typedef unsigned char	__u8;
typedef unsigned short	__u16;
typedef unsigned int	__u32;
typedef unsigned long long __u64;




typedef __u8	uint8_t;
typedef __u16	uint16_t;
typedef __u32	uint32_t;
typedef __u64	uint64_t;


typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef unsigned long   _size_t;
typedef _size_t         size_t;
typedef _size_t         uintptr_t;

#endif  /* _STDINT_H */