/* @(#)fcons.c	2.7 98/02/15 Copyright 1986, 1995 J. Schilling */
/*
 *	Copyright (c) 1986, 1995  J. Schilling
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

#include <stdio.h>
#include "io.h"

LOCAL	char	*fmtab[] = {
			"",	/* 0	FI_NONE				*/
			"r",	/* 1	FI_READ				*/
			"w",	/* 2	FI_WRITE		**1)	*/
			"r+",	/* 3	FI_READ  | FI_WRITE		*/
			"b",	/* 4	FI_NONE  | FI_BINARY		*/
			"rb",	/* 5	FI_READ  | FI_BINARY		*/
			"wb",	/* 6	FI_WRITE | FI_BINARY	**1)	*/
			"r+b",	/* 7	FI_READ  | FI_WRITE | FI_BINARY	*/
		};
/*
 * NOTES:
 *	1)	fdopen() guarantees not to create/trunc files in this case
 *
 *	"w"	will create/trunc files with fopen()
 *	"a"	will create files with fopen()
 */


FILE *_fcons(fd, f, flag)
	register FILE	*fd;
		 int	f;
		 int	flag;
{
	int	my_gflag = _io_glflag;

	if (fd == (FILE *)NULL)
		fd = fdopen(f, fmtab[flag&(FI_READ|FI_WRITE|FI_BINARY)]);

	if (fd != (FILE *)NULL) {
		if (flag & FI_APPEND) {
			(void) fseek(fd, 0L, 2);
		}
		if (flag & FI_UNBUF) {
			setbuf(fd, NULL);
			my_gflag |= _IOUNBUF;
		}
		set_my_flag(fd, my_gflag); /* must clear it if fd is reused */
		return (fd);
	}
	if (flag & FI_CLOSE)
		close (f);

	return ((FILE *) NULL);
}
