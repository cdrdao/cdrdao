/* @(#)cvmod.c	2.4 98/09/05 Copyright 1986, 1995 J. Schilling */
/*
 *	Copyright (c) 1986, 1995 J. Schilling
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

#ifndef	O_BINARY
#define	O_BINARY	0
#endif

int _cvmod(mode, omode, flag)
	const char	*mode;
	int		*omode;
	int		*flag;
{
	while (*mode){
		switch (*mode) {
		
		case 'r':   *omode |= O_RDONLY;	*flag |= FI_READ;	break;
		case 'w':   *omode |= O_WRONLY;	*flag |= FI_WRITE;	break;
		case 'e':   *omode |= O_EXCL;				break;
		case 'c':   *omode |= O_CREAT;	*flag |= FI_CREATE;	break;
		case 't':   *omode |= O_TRUNC;	*flag |= FI_TRUNC;	break;
		case 'a':   *omode |= O_APPEND;	*flag |= FI_APPEND;	break;
		case 'u':			*flag |= FI_UNBUF;	break;
			/* dummy on UNIX */
		case 'b':   *omode |= O_BINARY; *flag |= FI_BINARY;	break;
		default:    raisecond(_badmode, 0L);
			    return 0;
		}
		mode++;
	}
	if (*flag & FI_READ && *flag & FI_WRITE) {
		*omode &= ~(O_RDONLY|O_WRONLY);
		*omode |= O_RDWR;
	}
	return 1;
}
