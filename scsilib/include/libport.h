/* @(#)libport.h	1.2 99/08/30 Copyright 1995 J. Schilling */
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


#ifndef _LIBPORT_H
#define _LIBPORT_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif
#ifndef	_PROTOTYP_H
#include <prototyp.h>
#endif

#ifdef	OPENSERVER
/*
 * Don't use the usleep() from libc on SCO's OPENSERVER.
 * It will kill our processes with SIGALRM.
 */
#undef	HAVE_USLEEP
#endif

#ifndef	HAVE_GETHOSTID
extern	long		gethostid	__PR((void));
#endif
#ifndef	HAVE_GETHOSTNAME
extern	int		gethostname	__PR((char *name, int namelen));
#endif
#ifndef	HAVE_GETDOMAINNAME
extern	int		getdomainname	__PR((char *name, int namelen));
#endif
#ifndef	HAVE_USLEEP
extern	int		usleep		__PR((int usec));
#endif

#endif	/* _LIBPORT_H */
