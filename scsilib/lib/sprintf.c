/* @(#)sprintf.c	1.12 98/09/05 Copyright 1985 J. Schilling */
/*
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
#include <vadefs.h>
#include <standard.h>

/*
 * Do not include stdio.h, BSD systems define sprintf the wrong way!
 */
EXPORT	int sprintf __PR((char *, const char *, ...));

#ifdef	PROTOTYPES
static void _cput(char c, long ba)
#else
static void _cput(c, ba)
	char	c;
	long	ba;
#endif
{
	*(*(char **) ba)++ = c;
}

/* VARARGS2 */
#ifdef	PROTOTYPES
int sprintf(char *buf, const char *form, ...)
#else
int sprintf(buf, form, va_alist)
	char	*buf;
	char	*form;
	va_dcl
#endif
{
	va_list	args;
	int	cnt;
	char	*bp = buf;

#ifdef	PROTOTYPES
	va_start(args, form);
#else
	va_start(args);
#endif
	cnt = format(_cput, (long)&bp, form,  args);
	va_end(args);
	*bp = '\0';

	return (cnt);
}
