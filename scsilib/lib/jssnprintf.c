/* @(#)jssnprintf.c	1.4 99/08/30 Copyright 1985 J. Schilling */
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
#include <stdio.h>		/* Try to get size_t	*/
#include <stdxlib.h>		/* Try again for size_t	*/
#include <vadefs.h>
#include <standard.h>

#ifdef	HAVE_SIZE_T
EXPORT	int js_snprintf __PR((char *, size_t maxcnt, const char *, ...));
#else
typedef	unsigned int	size_t;
#endif

typedef struct {
	char	*ptr;
	int	count;
} *BUF, _BUF;

#ifdef	PROTOTYPES
static void _cput(char c, long l)
#else
static void _cput(c, l)
	char	c;
	long	l;
#endif
{
	register BUF	bp = (BUF)l;

	if (--bp->count > 0) {
		*bp->ptr++ = c;
	} else {
		/*
		 * Make sure that there will never be a negative overflow.
		 */
		bp->count++;
	}
}

/* VARARGS2 */
#ifdef	PROTOTYPES
int js_snprintf(char *buf, size_t maxcnt, const char *form, ...)
#else
int js_snprintf(buf, maxcnt, form, va_alist)
	char	*buf;
	unsigned maxcnt;
	char	*form;
	va_dcl
#endif
{
	va_list	args;
	int	cnt;
	_BUF	bb;

	bb.ptr = buf;
	bb.count = maxcnt;

#ifdef	PROTOTYPES
	va_start(args, form);
#else
	va_start(args);
#endif
	cnt = format(_cput, (long)&bb, form,  args);
	va_end(args);
	*(bb.ptr) = '\0';
	if (bb.count <= 0)
		return (-1);

	return (cnt);
}
