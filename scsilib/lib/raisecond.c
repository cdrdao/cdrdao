/* @(#)raisecond.c	1.11 99/09/11 Copyright 1985 J. Schilling */
/*
 *	raise a condition (software signal)
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
/*
 *	Check for installed condition handlers.
 *	If a handler is found, the function is called with the appropriate args.
 *	If no handler is found or no handler signals success,
 *	the program will be aborted.
 *
 *	Copyright (c) 1985 J. Schilling
 */
#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include <sigblk.h>
#include <unixstd.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <avoffset.h>

#if	!defined(AV_OFFSET) || !defined(FP_INDIR)
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif
#ifdef	NO_SCANSTACK
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif

/*
 * Macros to print to stderr without stdio, to avoid screwing up.
 */
#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif
#define	eprints(a)	(void)write(STDERR_FILENO, (a), sizeof(a)-1)
#define	eprintl(a)	(void)write(STDERR_FILENO, (a), strlen(a))

#define	is_even(p)	((((long)(p)) & 1) == 0)
#define	even(p)		(((long)(p)) & ~1L)
#ifdef	__future__
#define	even(p)		(((long)(p)) - 1)/* will this work with 64 bit ?? */
#endif


LOCAL	void raiseabort  __PR((const char *));

#ifdef	HAVE_SCANSTACK

#include <stkframe.h>

LOCAL	BOOL framehandle __PR((SIGBLK *, const char *, const char *, long));

/*
 *	Loop through the chain of procedure frames on the stack.
 *
 *	Frame pointers normally have even values.
 *	Frame pointers of procedures with an installed handler are marked odd.
 *	The even base value, in this case actually points to a SIGBLK which
 *	holds the saved "real" frame pointer.
 *	The SIGBLK mentioned above may me the start of a chain of SIGBLK's,
 *	containing different handlers.
 */
void raisecond(signame, arg2)
	const char	*signame;
	long		arg2;
{
	register struct frame *fp = (struct frame *)getfp();

	for(; fp; fp = (struct frame *)fp->fr_savfp) {
		if (is_even(fp))
			continue;
		fp = (struct frame *)even(fp);	/* really is (SIGBLK *) */

		if (framehandle((SIGBLK *)fp, signame, signame, arg2))
			return;
		else if (framehandle((SIGBLK *)fp, "any_other", signame, arg2))
			return;
		fp = (struct frame *)((SIGBLK *)fp)->sb_savfp;
	}
	/*
	 * No matching handler that signals success found.
	 * Print error message and abort.
	 */
	raiseabort(signame);
	/* NOTREACHED */
}

/*
 *	Loop through the handler chain for a procedure frame.
 *
 *	If no handler with matching name is found, return FALSE,
 *	otherwise the first handler with matching name is called.
 *	The return value in the latter case depends on the called function.
 */
LOCAL BOOL framehandle(sp, handlename, signame, arg2)
	register SIGBLK *sp;
	const char	*handlename;
	const char	*signame;
	long		arg2;
{
	for(; sp; sp = sp->sb_signext) {
		if (streql(sp->sb_signame, handlename)) {
			if (sp->sb_sigfun == NULL) {	/* deactivated */
				return (FALSE);
			} else {
				return (*sp->sb_sigfun)(signame,
							sp->sb_sigarg, arg2);
			}
		}
	}
	return (FALSE);
}

#else	/* HAVE_SCANSTACK */

void raisecond(signame, arg2)
	const char	*signame;
	long		arg2;
{
	/*
	 * Print error message and abort.
	 */
	raiseabort(signame);
	/* NOTREACHED */
}

#endif	/* HAVE_SCANSTACK */

LOCAL void raiseabort(signame)
	const	char	*signame;
{
	eprints("Condition not caught: "); eprintl(signame); eprints(".\n");
#ifndef	HAVE_SCANSTACK
	eprints("Raisecond: not implemented.\n");
#endif
	abort();
	/* NOTREACHED */
}
