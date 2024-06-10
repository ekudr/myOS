
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
typedef signed char	__s8;
typedef unsigned short	__u16;
typedef signed short	__s16;
typedef unsigned int	__u32;
typedef signed int	__s32;
typedef unsigned long long __u64;
typedef signed long long __s64;




typedef __u8	uint8_t;
typedef __u16	uint16_t;
typedef __u32	uint32_t;
typedef __u64	uint64_t;

typedef __u8	_uint8_t;
typedef __s8	_int8_t;
typedef __u16	_uint16_t;
typedef __s16	_int16_t;
typedef __u32	_uint32_t;
typedef __s32	_int32_t;
typedef __u64	_uint64_t;
typedef __s64	_int64_t;


typedef __u8	u8;
typedef __u16	u16;
typedef __u32	u32;
typedef __u64	u64;

typedef _int64_t                _intmax_t;
typedef _uint64_t               _uintmax_t;

typedef unsigned long   _size_t;
typedef _size_t         size_t;
typedef _size_t         uintptr_t;

/* Greatest-width integer types */

typedef _intmax_t           intmax_t;
typedef _uintmax_t          uintmax_t;

#endif  /* _STDINT_H */