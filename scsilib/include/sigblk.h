/* @(#)sigblk.h	1.5 96/02/04 Copyright 1985 J. Schilling */
/*
 *	software signal block definition
 *
 *	Copyright (c) 1985,1995 J. Schilling
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

#ifndef	_SIGBLK_H
#define	_SIGBLK_H

typedef struct sigblk {
	long		**sb_savfp;
	struct sigblk	*sb_signext;
	short		sb_siglen;
#ifdef	__STDC__
	const char	*sb_signame;
	int		(*sb_sigfun)(const char *, long, long);
#else
	char		*sb_signame;
	int		(*sb_sigfun)();
#endif
	long		sb_sigarg;
} SIGBLK;

#if	defined(__STDC__)

typedef	int	(*handlefunc_t)(const char *, long, long);

extern	void	handlecond(const char *, SIGBLK *,
				int(*)(const char *, long, long), long);
extern	void	raisecond(const char *, long);
extern	void	unhandlecond(void);

#else

typedef	int	(*handlefunc_t)();

extern	void	handlecond();
extern	void	raisecond();
extern	void	unhandlecond();

#endif

#endif	/* _SIGBLK_H */
