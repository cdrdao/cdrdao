/* @(#)default.c	1.2 98/12/01 Copyright 1997 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)default.c	1.2 98/12/01 Copyright 1997 J. Schilling";
#endif
/*
 *	Copyright (c) 1997 J. Schilling
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
#include <stdio.h>
#include <strdefs.h>
#include <deflts.h>

#define	MAXLINE	512

static	FILE	*dfltfile	= (FILE *)NULL;

int
defltopen(name)
	const char	*name;
{
	if (dfltfile != (FILE *)NULL)
		fclose(dfltfile);

	if (name == (char *)NULL) {
		dfltfile = NULL;
		return (0);
	}

	if ((dfltfile = fopen(name, "r")) == (FILE *)NULL) {
		return (-1);
	}
	return (0);
}

int
defltclose()
{
	int	ret;

	if (dfltfile != (FILE *)NULL) {
		ret = fclose(dfltfile);
		dfltfile = NULL;
		return (ret);
	}
	return (0);
}

char *
defltread(name)
	const char	*name;
{
	register int	len;
	register int	namelen;
	static	 char	buf[MAXLINE];

	if (dfltfile == (FILE *)NULL) {
		return ((char *)NULL);
	}
	namelen = strlen(name);

	rewind(dfltfile);
	while (fgets(buf, sizeof(buf), dfltfile)) {
		len = strlen(buf);
		if (buf[len-1] == '\n') {
			buf[len-1] = 0;
		} else {
			return ((char *)NULL);
		}
		if (strncmp(name, buf, namelen) == 0) {
			return (&buf[namelen]);
		}
	}
	return ((char *)NULL);
}

int
defltcntl(cmd, flags)
	int	cmd;
	int	flags;
{
	int  oldflags = 0;

	return (oldflags);
}
