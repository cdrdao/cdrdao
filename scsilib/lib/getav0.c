/* @(#)getav0.c	1.9 99/09/11 Copyright 1985 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)getav0.c	1.9 99/09/11 Copyright 1985 J. Schilling";
#endif
/*
 *	Get arg vector by scanning the stack
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

#include <mconfig.h>
#include <sigblk.h>
#include <avoffset.h>
#include <standard.h>

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

#ifdef	HAVE_SCANSTACK

#include <stkframe.h>

#define	is_even(p)	((((long)(p)) & 1) == 0)
#define	even(p)		(((long)(p)) & ~1L)
#ifdef	__future__
#define	even(p)		(((long)(p)) - 1)/* will this work with 64 bit ?? */
#endif

char **getavp()
{
	register struct frame *fp;
	register struct frame *ofp;
		 char	**av;
#if	FP_INDIR > 0
	register int	i = 0;
#endif

	ofp = fp = (struct frame *)getfp();
	while (fp) {

#if	FP_INDIR > 0
		i++;
#endif
		ofp = fp;
		if (!is_even(fp->fr_savfp)) {
			fp = (struct frame *)even(fp->fr_savfp);
			fp = (struct frame *)((SIGBLK *)fp)->sb_savfp;
			continue;
		}
		fp = fp->fr_savfp;
	}

#if	FP_INDIR > 0
	i -= FP_INDIR;
        ofp = fp = (struct frame *)getfp();
	while (fp) {
		if (--i < 0)
			break;
		ofp = fp;
		if (!is_even(fp->fr_savfp)) {
			fp = (struct frame *)even(fp->fr_savfp);
			fp = (struct frame *)((SIGBLK *)fp)->sb_savfp;
			continue;
		}
		fp = fp->fr_savfp;
	}
#endif

        av = (char **)ofp + AV_OFFSET;	/* aus avoffset.h */
					/* (wird generiert mit av_offset) */
	return (av);
}

char *getav0()
{
	return (getavp()[0]);
}

#else

char *getav0()
{
	return ("???");
}

#endif	/* HAVE_SCANSTACK */
