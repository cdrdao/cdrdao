/* @(#)gethostname.c	1.13 99/08/30 Copyright 1995 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)gethostname.c	1.13 99/08/30 Copyright 1995 J. Schilling";
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
#ifdef	HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif
#include <libport.h>

#ifndef	HAVE_GETHOSTNAME
EXPORT	int	gethostname	__PR((char *name, int namelen));
#endif


#if	!defined(HAVE_GETHOSTNAME) && defined(SI_HOSTNAME)

EXPORT int
gethostname(name, namelen)
	char	*name;
	int	namelen;
{
	if (sysinfo(SI_HOSTNAME, name, namelen) < 0)
		return (-1);
	return (0);
}
#endif
