/* @(#)cvt.c	1.3 99/09/08 Copyright 1998 J. Schilling */
/*
 *	Compatibility routines for 4.4BSD based C-libraries ecvt()/fcvt()
 *	and a working gcvt() that is needed on 4.4BSD and GNU libc systems.
 *
 *	On 4.4BSD, gcvt() is missing, the gcvt() implementation from GNU libc
 *	is not working correctly.
 *
 *	Neither __dtoa() nor [efg]cvt() are MT safe.
 *
 *	Copyright (c) 1998 J. Schilling
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

#include <stdxlib.h>
#include <utypes.h>
#include <standard.h>

#ifdef	HAVE_DTOA	/* 4.4BSD floating point implementation */
extern	char *__dtoa	__PR((double value, int mode, int ndigit, int *decpt, int *sign, char **ep));
#endif

#ifndef	HAVE_ECVT
EXPORT	char *ecvt	__PR((double value, int ndigit, int *decpt, int *sign));
#endif
#ifndef	HAVE_FCVT
EXPORT	char *fcvt	__PR((double value, int ndigit, int *decpt, int *sign));
#endif
#ifndef	HAVE_GCVT
EXPORT	char *gcvt	__PR((double value, int ndigit, char *buf));
#endif

#if	!defined(HAVE_ECVT) && defined(HAVE_DTOA)
#define	HAVE_ECVT
char *
ecvt(value, ndigit, decpt, sign)
	double	value;
	int	ndigit;
	int	*decpt;
	int	*sign;
{
static	Uint	bufsize;
static	char	*buf;
	char	*bufend;
	char	*ep;
	char	*bp = __dtoa(value, 2, ndigit, decpt, sign, &ep);

	if (value == 0.0) {
		/*
		 * Handle __dtoa()'s deviation from ecvt():
		 * 0.0 is converted to "0" instead of 'ndigit' zeroes.
		 * The string "0" is not allocated, so
		 * we need to allocate buffer to hold 'ndigit' zeroes.
		 */
		if (bufsize < ndigit + 1) {
			if (buf != (char *)0)
				free(buf);
			bufsize = ndigit + 1;
			buf = malloc(bufsize);
		}
		ep = bp = buf;
	}

	/*
	 * Fill up trailing zeroes suppressed by __dtoa()
	 * From an internal __dtoa() comment:
	 *	Sufficient space is allocated to the return value
	 *	to hold the suppressed trailing zeros.
	 */
	for (bufend = &bp[ndigit]; ep < bufend; )
		*ep++ = '0';
	*ep = '\0';

	return (bp);
}
#endif

#if	!defined(HAVE_FCVT) && defined(HAVE_DTOA)
#define	HAVE_FCVT
char *
fcvt(value, ndigit, decpt, sign)
	double	value;
	int	ndigit;
	int	*decpt;
	int	*sign;
{
static	Uint	bufsize;
static	char	*buf;
	char	*bufend;
	char	*ep;
	char	*bp = __dtoa(value, 3, ndigit, decpt, sign, &ep);

	if (value == 0.0) {
		/*
		 * Handle __dtoa()'s deviation from fcvt():
		 * 0.0 is converted to "0" instead of 'ndigit' zeroes.
		 * The string "0" is not allocated, so
		 * we need to allocate buffer to hold 'ndigit' zeroes.
		 */
		if (bufsize < ndigit + 1) {
			if (buf != (char *)0)
				free(buf);
			bufsize = ndigit + 1;
			buf = malloc(bufsize);
		}
		ep = bp = buf;
		*decpt = 0;
	}

	/*
	 * Fill up trailing zeroes suppressed by __dtoa()
	 * From an internal __dtoa() comment:
	 *	Sufficient space is allocated to the return value
	 *	to hold the suppressed trailing zeros.
	 */
	for (bufend = &bp[*decpt + ndigit]; ep < bufend; )
		*ep++ = '0';
	*ep = '\0';

	return (bp);
}
#endif

#ifndef	HAVE_GCVT
#define	HAVE_GCVT
char *
gcvt(number, ndigit, buf)
	double	number;
	int	ndigit;
	char	*buf;
{
		 int	sign;
		 int	decpt;
	register char	*b;
	register char	*rs;
	register int	i;

	b = ecvt(number, ndigit, &decpt, &sign);
	rs = buf;
	if (sign)
		*rs++ = '-';
	for (i = ndigit-1; i > 0 && b[i] == '0'; i--)
		ndigit--;
#ifdef	V7_FLOATSTYLE
	if ((decpt >= 0 && decpt-ndigit > 4) ||
#else
	if ((decpt >= 0 && decpt-ndigit > 0) ||
#endif
	    (decpt < 0 && decpt < -3)) {	/* e-format */
		decpt--;
		*rs++ = *b++;
		*rs++ = '.';
		for (i = 1; i < ndigit; i++)
			*rs++ = *b++;
		*rs++ = 'e';
		if (decpt < 0) {
			decpt = -decpt;
			*rs++ = '-';
		} else {
			*rs++ = '+';
		}
		if (decpt >= 100) {
			*rs++ = decpt / 100 + '0';
			decpt %= 100;
		}
		*rs++ = decpt / 10 + '0';
		*rs++ = decpt % 10 + '0';
	} else {				/* f-format */
		if (decpt <= 0) {
			if (*b != '0') {
#ifndef	V7_FLOATSTYLE
				*rs++ = '0';
#endif
				*rs++ = '.';
			}
			while (decpt < 0) {
				decpt++;
				*rs++ = '0';
			}
		}
		for (i = 1; i <= ndigit; i++) {
			*rs++ = *b++;
			if (i == decpt)
				*rs++ = '.';
		}
		if (ndigit < decpt) {
			while (ndigit++ < decpt)
				*rs++ = '0';
			*rs++ = '.';
		}
	}
	if (rs[-1] == '.')
		rs--;
	*rs = '\0';
	return (buf);
}
#endif
