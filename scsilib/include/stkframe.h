/* @(#)stkframe.h	1.5 99/09/11 Copyright 1995 J. Schilling */
/*
 * Common definitions for routines that parse the stack frame.
 *
 * This file has to be fixed if you want to port routines which use getfp().
 * Have a look at struct frame below and use it as a sample,
 * the new struct frame must at least contain a member 'fr_savfp'.
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

#ifndef _STKFRAME_H
#define _STKFRAME_H

#if defined (sun) && (defined(SVR4) || defined(__SVR4) || defined(__SVR4__))
#	ifdef	i386
		/*
		 * On Solaris 2.1 x86 sys/frame.h is not useful at all
		 * On Solaris 2.4 x86 sys/frame.h is buggy (fr_savfp is int!!)
		 */
#		include <sys/reg.h>
#	endif
#	include <sys/frame.h>

#elif	defined (sun)
#	include <machine/frame.h>
#else

/*
 * XXX: I hope this will be useful on other machines (no guarantee)
 * XXX: It is taken from a sun Motorola system, but should also be useful
 * XXX: on a i386.
 * XXX: In general you have to write a sample program, set a breakpoint
 * XXX: on a small function and inspect the stackframe with adb.
 */

struct frame {
	struct frame	*fr_savfp;	/* saved frame pointer */
	int		fr_savpc;	/* saved program counter */
	int		fr_arg[1];	/* array of arguments */
};

#endif

#endif	/* _STKFRAME_H */
