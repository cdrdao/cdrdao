/* @(#)utypes.h	1.5 99/07/31 Copyright 1997 J. Schilling */
/*
 *	Definitions for some user defined types
 *
 *	Copyright (c) 1997 J. Schilling
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef	_UTYPES_H
#define	_UTYPES_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifdef	__CHAR_UNSIGNED__	/* GNU GCC define     (dynamic)	*/
#ifndef CHAR_IS_UNSIGNED
#define	CHAR_IS_UNSIGNED	/* Sing Schily define (static)	*/
#endif
#endif

/*
 * Several unsigned cardinal types
 */
typedef	unsigned long	Ulong;
typedef	unsigned int	Uint;
typedef	unsigned short	Ushort;
typedef	unsigned char	Uchar;

/*
 * This is a definition for a compiler dependant 64 bit type.
 * It currently is silently a long if the compiler does not
 * support it. Check if this is the right way.
 */
#ifndef	NO_LONGLONG
#	if	defined(HAVE_LONGLONG)
#		define	USE_LONGLONG
#	endif
#endif

#ifdef	USE_LONGLONG

typedef	long long		Llong;
typedef	unsigned long long	Ullong;

#else

typedef	long			Llong;
typedef	unsigned long		Ullong;

#endif

/*
 * The IBM AIX C-compiler seems to be the only compiler on the world
 * which does not allow to use unsigned char bit fields as a hint
 * for packed bit fields. Define a pesical type to avoid warnings.
 * The packed attribute is honored wit unsigned int in this case too.
 */
#ifdef	_AIX

typedef unsigned int	Ucbit;

#else

typedef unsigned char	Ucbit;

#endif

/*
 * Start inttypes.h emulation.
 *
 * Thanks to Solaris 2.4 and even recent 1999 Linux versions, we
 * cannot use the official UNIX-98 names here. Old Solaris versions
 * define parts of the types in some exotic include files.
 * Linux even defines incompatible types in <sys/types.h>.
 */

#ifdef	HAVE_INTTYPES_H
#	include <inttypes.h>
#	define	HAVE_INT64_T
#	define	HAVE_UINT64_T

#define	Int8_t			int8_t
#define	Int16_t			int16_t
#define	Int32_t			int32_t
#define	Int64_t			int64_t
#define	UInt8_t			uint8_t
#define	UInt16_t		uint16_t
#define	UInt32_t		uint32_t
#define	UInt64_t		uint64_t

#define	Intptr_t		intptr_t
#define	UIntptr_t		uintptr_t

#else	/* !HAVE_INTTYPES_H */

#if SIZEOF_CHAR != 1 || SIZEOF_UNSIGNED_CHAR != 1
/*
 * #error will not work for all compilers (e.g. sunos4)
 * The following line will abort compilation on all compilers
 * if the above is true. And that's what we want.
 */
error  Sizeof char is not equal 1
#endif

#if	defined(__STDC__) || defined(CHAR_IS_UNSIGNED)
	typedef	signed char		Int8_t;
#else
	typedef	char			Int8_t;
#endif

#if SIZEOF_SHORT_INT == 2
	typedef	short			Int16_t;
#else
	error		No int16_t found
#endif

#if SIZEOF_INT == 4
	typedef	int			Int32_t;
#else
	error		No int32_t found
#endif

#if SIZEOF_LONG_INT == 8
	typedef		long		Int64_t;
#	define	HAVE_INT64_T
#else
#if SIZEOF_LONG_LONG == 8
	typedef		long long	Int64_t;
#	define	HAVE_INT64_T
#else
/*	error		No int64_t found*/
#endif
#endif

#if SIZEOF_CHAR_P == SIZEOF_INT
	typedef		int		Intptr_t;
#else
#if SIZEOF_CHAR_P == SIZEOF_LONG_INT
	typedef		long		Intptr_t;
#else
	error		No intptr_t found
#endif
#endif

typedef	unsigned char		UInt8_t;

#if SIZEOF_UNSIGNED_SHORT_INT == 2
	typedef	unsigned short		UInt16_t;
#else
	error		No uint16_t found
#endif

#if SIZEOF_UNSIGNED_INT == 4
	typedef	unsigned int		UInt32_t;
#else
	error		No int32_t found
#endif

#if SIZEOF_UNSIGNED_LONG_INT == 8
	typedef	unsigned long		UInt64_t;
#	define	HAVE_UINT64_T
#else
#if SIZEOF_UNSIGNED_LONG_LONG == 8
	typedef	unsigned long long	UInt64_t;
#	define	HAVE_UINT64_T
#else
/*	error		No uint64_t found*/
#endif
#endif

#if SIZEOF_CHAR_P == SIZEOF_UNSIGNED_INT
	typedef		unsigned int	UIntptr_t;
#else
#if SIZEOF_CHAR_P == SIZEOF_UNSIGNED_LONG_INT
	typedef		unsigned long	UIntptr_t;
#else
	error		No uintptr_t found
#endif
#endif

#endif	/* HAVE_INTTYPES_H */

#endif	/* _UTYPES_H */
