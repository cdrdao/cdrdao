/* @(#)getfp.c	1.9 99/09/11 Copyright 1988 J. Schilling */
/*
 *	Get frame pointer
 *
 *	Copyright (c) 1988 J. Schilling
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

#include <mconfig.h>
#include <standard.h>

#ifdef	NO_SCANSTACK
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif

#ifdef	HAVE_SCANSTACK
#include <stkframe.h>

#define	MAXWINDOWS	32
#define	NWINDOWS	7

#if defined(sparc) && defined(__GNUC__)
#	define	IDX		3	/* some strange things on sparc gcc */
#else
#	define	IDX		1
#endif

void **getfp()
{
		long	**dummy[1];
	static	int	idx = IDX;	/* fool optimizer in c compiler */

#ifdef	sparc
	flush_reg_windows(MAXWINDOWS-2);
#endif
	return ((void **)((struct frame *)&dummy[idx])->fr_savfp);
}

#ifdef	sparc
int flush_reg_windows(n)
	int	n;
{
	if (--n > 0)
		flush_reg_windows(n);
	return (0);
}
#endif

#endif	/* HAVE_SCANSTACK */
