/* @(#)usleep.c	1.13 99/08/30 Copyright 1995 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)usleep.c	1.13 99/08/30 Copyright 1995 J. Schilling";
#endif
/*
 *	Copyright (c) 1995 J. Schilling
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
#include <stdxlib.h>
#include <timedefs.h>
#ifdef	HAVE_POLL_H
#	include <poll.h>
#else
#	ifdef	HAVE_SYS_POLL_H
#	include <sys/poll.h>
#	endif
#endif
#ifdef	HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif
#include <libport.h>

#ifdef	OPENSERVER
/*
 * Don't use the usleep() from libc on SCO's OPENSERVER.
 * It will kill our processes with SIGALRM.
 */
#undef	HAVE_USLEEP
#endif

#ifndef	HAVE_USLEEP
EXPORT	int	usleep		__PR((int usec));
#endif

#if	!defined(HAVE_USLEEP)

EXPORT int
usleep(usec)
	int	usec;
{
#if	defined(HAVE_SELECT)
#define	HAVE_USLEEP

	struct timeval tv;
	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;
	select(0, 0, 0, 0, &tv);
#endif

#if	defined(HAVE_POLL) && !defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	if (poll(0, 0, usec/1000) < 0)
		comerr("poll delay failed.\n");

#endif

#if	defined(HAVE_NANOSLEEP) && !defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	struct timespec ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;

	nanosleep(&ts, 0);
#endif


#if	!defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	sleep((usec+500000)/1000000);
#endif

	return (0);
}
#endif
