/* @(#)standard.h	1.21 99/08/12 Copyright 1985 J. Schilling */
/*
 *	standard definitions
 *
 *	This file should be included past:
 *
 *	mconfig.h / config.h
 *	stdio.h
 *	stdlib.h
 *	unistd.h
 *	string.h
 *	sys/types.h
 *
 *	Copyright (c) 1985 J. Schilling
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

#ifndef _STANDARD_H
#define _STANDARD_H

#ifdef	M68000
#	ifndef	tos
#		define	JOS	1
#	endif
#endif
#include <prototyp.h>

/*
 *	fundamental constants
 */
#ifndef	NULL
#	define	NULL		0
#endif
#ifndef	TRUE
#	define	TRUE		1
#	define	FALSE		0
#endif
#define	YES			1
#define	NO			0

/*
 *	Program exit codes
 */
#define	EX_BAD			(-1)

/*
 *	standard storage class definitions
 */
#define	GLOBAL	extern
#define	IMPORT	extern
#define	EXPORT
#define	INTERN	static
#define	LOCAL	static
#define	FAST	register
#if defined(_JOS) || defined(JOS)
#	define	global	extern
#	define	import	extern
#	define	export
#	define	intern	static
#	define	local	static
#	define	fast	register
#endif
#ifndef	PROTOTYPES
#	ifndef	const
#		define	const
#	endif
#	ifndef	signed
#		define	signed
#	endif
#	ifndef	volatile
#		define	volatile
#	endif
#endif	/* PROTOTYPES */

/*
 *	standard type definitions
 *
 *	The hidden Schily BOOL definition is used in case we need to deal
 *	with other BOOL defines on systems we like to port to.
 */
typedef int __SBOOL;
typedef int BOOL;
#ifndef __cplusplus
typedef int bool;
#endif
#ifdef	JOS
#	define	NO_VOID
#endif
#ifdef	NO_VOID
	typedef	int	VOID;
#	ifndef	lint
		typedef int void;
#	endif
#else
	typedef	void	VOID;
#endif

#if	defined(_SIZE_T)     || defined(_T_SIZE_) || defined(_T_SIZE) || \
	defined(__SIZE_T)    || defined(_SIZE_T_) || \
	defined(_GCC_SIZE_T) || defined(_SIZET_)  || \
	defined(__sys_stdtypes_h) || defined(___int_size_t_h)

#ifndef	HAVE_SIZE_T
#	define	HAVE_SIZE_T
#endif

#endif

/*
 * Until we fixed all programs that #include <standard.h>
 */
#include <schily.h>

#if defined(_JOS) || defined(JOS)
#	include <jos_io.h>
#endif

#endif	/* _STANDARD_H */
