/*
 * Copyright (C) 2007-2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Apple Computer, Inc. All rights reserved.
 *
 * This document is the property of Apple Computer, Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Computer, Inc.
 */
#ifndef __TYPES_H
#define __TYPES_H

 ///////////////////////////////////////////////
 // cdefs.h
 ///////////////////////////////////////////////
#if defined(__cplusplus)

#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	}

#define NULL			0

#else /* !defined(__cplusplus) */

#define	__BEGIN_DECLS
#define	__END_DECLS

#define NULL			((void *)0)

#endif /* defined(__cplusplus) */

///////////////////////////////////////////////
// bsd/arm/types.h
///////////////////////////////////////////////

typedef signed char           int8_t;
typedef unsigned char		u_int8_t;
typedef short			int16_t;
typedef unsigned short		u_int16_t;
typedef int			int32_t;
typedef unsigned int		u_int32_t;
typedef long long		int64_t;
typedef unsigned long long	u_int64_t;

#if __LP64__
typedef int64_t			register_t;
#else
typedef int32_t			register_t;
#endif

//typedef long			intptr_t;
//typedef unsigned long		uintptr_t;

#if defined(__SIZE_TYPE__)
typedef __SIZE_TYPE__		size_t;        /* sizeof() */
#else
//typedef unsigned long		size_t;        /* sizeof() */
#endif
typedef long			ssize_t;
#if defined(__PTRDIFF_TYPE__)
typedef __PTRDIFF_TYPE__	ptrdiff_t;     /* ptr1 - ptr2 */
#else
typedef int			ptrdiff_t;     /* ptr1 - ptr2 */
#endif

//typedef long	 		time_t;
#define INFINITE_TIME 		0xffffffffUL

///////////////////////////////////////////////
// stdint.h
///////////////////////////////////////////////

/* from ISO/IEC 988:1999 spec */

/* 7.18.1.1 Exact-width integer types */
typedef u_int8_t              uint8_t;
typedef u_int16_t            uint16_t;
typedef u_int32_t            uint32_t;
typedef u_int64_t            uint64_t;

/* 7.18.2.1 Limits of exact-width integer types */
#define INT8_MIN         (-127-1)
#define INT16_MIN        (-32767-1)
#define INT32_MIN        (-2147483647-1)
#define INT64_MIN        (-9223372036854775807LL-1LL)

#define INT8_MAX         +127
#define INT16_MAX        +32767
#define INT32_MAX        +2147483647
#define INT64_MAX        +9223372036854775807LL

#define UINT8_MAX         255
#define UINT16_MAX        65535
#define UINT32_MAX        4294967295U
#define UINT64_MAX        18446744073709551615ULL

/* 7.18.2.4 Limits of integer types capable of holding object pointers */

#if __LP64__
#define INTPTR_MIN        INT64_MIN
#define INTPTR_MAX        INT64_MAX
#else
#define INTPTR_MIN        INT32_MIN
#define INTPTR_MAX        INT32_MAX
#endif

#if __LP64__
#define UINTPTR_MAX       UINT64_MAX
#else
#define UINTPTR_MAX       UINT32_MAX
#endif

/* 7.18.3 "Other" */
#if __LP64__
#define PTRDIFF_MIN       INT64_MIN
#define PTRDIFF_MAX       INT64_MAX
#else
#define PTRDIFF_MIN       INT32_MIN
#define PTRDIFF_MAX       INT32_MAX
#endif

#if __LP64__
#define SIZE_MAX          UINT64_MAX
#else
#define SIZE_MAX          UINT32_MAX
#endif

///////////////////////////////////////////////
// libkern/libkern/OSTypes.h
///////////////////////////////////////////////
typedef unsigned char		UInt8;
typedef unsigned short		UInt16;
typedef unsigned int		UInt32;
typedef unsigned long long	UInt64;

typedef signed char		SInt8;
typedef signed short		SInt16;
typedef signed int		SInt32;
typedef signed long long	SInt64;

#include <stdbool.h>


#endif /* __TYPES_H */