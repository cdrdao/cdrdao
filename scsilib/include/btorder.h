/* @(#)btorder.h	1.9 99/04/29 Copyright 1996 J. Schilling */
/*
 *	Definitions for Bitordering
 *
 *	Copyright (c) 1996 J. Schilling
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


#ifndef	_BTORDER_H
#define	_BTORDER_H

#include <sys/types.h>			/* try to load isa_defs.h on Solaris */

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif

#if defined(BIT_FIELDS_LTOH)		/* Use definition from xconfig.h */
#define	_BIT_FIELDS_LTOH
#endif

#if defined(BIT_FIELDS_HTOL)		/* Use definition from xconfig.h */
#define	_BIT_FIELDS_HTOL
#endif


#if	defined(_BIT_FIELDS_LTOH) || defined(_BIT_FIELDS_HTOL)
/*
 * Bitorder is known.
 */
#else
/*
 * Bitorder not known.
 */
#	if defined(sun3) || defined(mc68000) || \
	   defined(sun4) || defined(__sparc) || defined(sparc) || \
	   defined(__hppa) || defined(_ARCH_PPC) || defined(_IBMR2)
#		define _BIT_FIELDS_HTOL
#	endif

#	if defined(__sgi) && defined(__mips)
#		define _BIT_FIELDS_HTOL
#	endif

#	if defined(__i386) || defined(i386) || \
	   defined(__alpha) || defined(alpha) || \
	   defined(__arm) || defined(arm)
#		define _BIT_FIELDS_LTOH
#	endif

#	if defined(__ppc__) || defined(ppc) || defined(__ppc) || \
	   defined(__PPC) || defined(powerpc)

#		if	defined(__BIG_ENDIAN__)
#			define _BIT_FIELDS_HTOL
#		else
#			define _BIT_FIELDS_LTOH
#		endif
#	endif
#endif

#endif	/* _BTORDER_H */
