/* @(#)vadefs.h	1.2 98/03/02 Copyright 1998 J. Schilling */
/*
 *	Generic header for users of var args ...
 *
 *	Includes a default definition for va_copy()
 *	and some magic know how about the SVr4 Power PC var args ABI
 *	to create a __va_arg_list() macro.
 *
 *	Copyright (c) 1998 J. Schilling
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

#ifndef	_VADEFS_H
#define	_VADEFS_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifdef	HAVE_STDARG_H
#	include <stdarg.h>
#else
#	include <varargs.h>
#endif

#if (defined(__linux__) || defined(__linux) || defined(sun)) && \
			(defined(__ppc) || defined(__PPC) || defined(powerpc))

#	ifndef	VA_LIST_IS_ARRAY
#	define	VA_LIST_IS_ARRAY
#	endif
#endif


/*
 * __va_copy() is used by GCC 2.8 or newer until va_copy() becomes
 * a final ISO standard.
 */
#if !defined(va_copy) && !defined(HAVE_VA_COPY)
#	if	defined(__va_copy)
#		define	va_copy(to, from)	__va_copy(to, from)
#	endif
#endif

/*
 * va_copy() is a Solaris extension to provide a portable way to perform a
 * variable argument list "bookmarking" function.
 * If it is not available via stdarg.h, use a simple assignement for backward
 * compatibility.
 */
#if !defined(va_copy) && !defined(HAVE_VA_COPY)
#	define	va_copy(to, from)	((to) = (from))
#endif

/*
 * I don't know any portable way to get an arbitrary
 * C object from a var arg list so I use a
 * system-specific routine __va_arg_list() that knows
 * if 'va_list' is an array. You will not be able to
 * assign the value of __va_arg_list() but it works
 * to be used as an argument of a function.
 * It is a requirement for recursive printf to be able
 * to use this function argument. If your system
 * defines va_list to be an array you need to know this
 * via autoconf or another mechanism.
 * It would be nice to have something like
 * __va_arg_list() in stdarg.h
 */

#ifdef	VA_LIST_IS_ARRAY
#	define	__va_arg_list(list)	va_arg(list, void *)
#else
#	define	__va_arg_list(list)	va_arg(list, va_list)
#endif

#endif	/* _VADEFS_H */
