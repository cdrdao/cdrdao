/* @(#)timedefs.h	1.5 99/05/13 Copyright 1996 J. Schilling */
/*
 *	Generic header for users of gettimeofday() ...
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

#ifndef	_TIMEDEFS_H
#define	_TIMEDEFS_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifdef	TIME_WITH_SYS_TIME_H
#	include <sys/time.h>
#	include <time.h>
#else
#ifdef	HAVE_SYS_TIME_H
#	include <sys/time.h>
#else
#	include <time.h>
#endif
#endif

#ifdef	__CYGWIN32__
/*
 * Cygnus defines struct timeval in sys/time.h but not timerclear
 * timerclear is defined in windows32/Sockets.h ???
 */
#ifndef	timerclear
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0
#endif
#endif

#ifdef	__EMX__
/*
 * EMX for OS/2 defines struct timeval in sys/time.h but not timerclear
 */
#ifndef	timerclear
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0
#endif
#endif

#ifndef	timerclear

#ifndef	VMS
struct timeval {
	long	tv_sec;
	long	tv_usec;
};
#endif

struct timezone {
	int	tz_minuteswest;
	int	tz_dsttime;
};

#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0

#endif

#endif	/* _TIMEDEFS_H */
