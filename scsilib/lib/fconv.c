/* @(#)fconv.c	1.20 99/09/08 Copyright 1985 J. Schilling */
/*
 *	Convert floating point numbers to strings for format.c
 *	Should rather use the MT-safe routines [efg]convert()
 *
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

#include <mconfig.h>	/* <- may define NO_FLOATINGPOINT */
#ifndef	NO_FLOATINGPOINT

#include <stdxlib.h>
#include <standard.h>
#include <strdefs.h>

#if	!defined(HAVE_STDLIB_H) || defined(HAVE_DTOA)
extern	char	*ecvt __PR((double, int, int *, int *));
extern	char	*fcvt __PR((double, int, int *, int *));
#endif

#include <math.h>

#if	defined(__hpux) || defined(VMS) || defined(_SCO_DS)
#define	isnan(val)	(0)
#define	isinf(val)	(0)
#endif
#ifdef	SVR4
#include <ieeefp.h>
#define	isnan	isnand
#define	isinf	!finite
#endif
#if	defined(__osf__) || defined(_IBMR2) || defined(_AIX)
#include <fp.h> 
#define	isinf	!FINITE
/*#define	isinf	IS_INF*/
#define	isnan	IS_NAN
#endif 

#if !defined(HAVE_ECVT) || !defined(HAVE_FCVT) || !defined(HAVE_GCVT)
#include "cvt.c"
#endif

static	char	_nan[] = "(NaN)";
static	char	_inf[] = "(Infinity)";

static	int	_ferr __PR((char *, double));

#ifdef	abs
#	undef	abs
#endif
#define	abs(i)	((i) < 0 ? -(i) : (i))

EXPORT int
ftoes(s, val, fieldwidth, ndigits)
	register	char 	*s;
			double	val;
	register	int	fieldwidth;
	register	int	ndigits;
{
	register	char	*b;
	register	char	*rs;
	register	int	len;
	register	int	rdecpt;
			int 	decpt;
			int	sign;

	if ((len = _ferr(s, val)) > 0)
		return len;
	rs = s;
#ifdef	V7_FLOATSTYLE
	b = ecvt(val, ndigits, &decpt, &sign);
	rdecpt = decpt;
#else
	b = ecvt(val, ndigits+1, &decpt, &sign);
	rdecpt = decpt-1;
#endif
	len = ndigits + 6;			/* Punkt e +/- nnn */
	if (sign)
		len++;
	if (fieldwidth > len)
		while (fieldwidth-- > len)
			*rs++ = ' ';
	if (sign)
		*rs++ = '-';
#ifndef	V7_FLOATSTYLE
	if (*b)
		*rs++ = *b++;
#endif
	*rs++ = '.';
	while (*b && ndigits-- > 0)
		*rs++ = *b++;
	*rs++ = 'e';
	*rs++ = rdecpt >= 0 ? '+' : '-';
	rdecpt = abs(rdecpt);
#ifndef	V7_FLOATSTYLE
	if (rdecpt >= 100)
#endif
	{
		*rs++ = rdecpt / 100 + '0';
		rdecpt %= 100;
	}
	*rs++ = rdecpt / 10 + '0';
	*rs++ = rdecpt % 10 + '0';
	*rs = '\0';
	return rs - s;
}

/*
 * fcvt() from Cygwin32 is buggy.
 */
#if	!defined(HAVE_FCVT) && defined(HAVE_ECVT)
#define	USE_ECVT
#endif

EXPORT int
ftofs(s, val, fieldwidth, ndigits)
	register	char 	*s;
			double	val;
	register	int	fieldwidth;
	register	int	ndigits;
{
	register	char	*b;
	register	char	*rs;
	register	int	len;
	register	int	rdecpt;
			int 	decpt;
			int	sign;

	if ((len = _ferr(s, val)) > 0)
		return len;
	rs = s;
#ifdef	USE_ECVT
	b = ecvt(val, ndigits, &decpt, &sign);
#else
	b = fcvt(val, ndigits, &decpt, &sign);
#endif
	rdecpt = decpt;
	len = rdecpt + ndigits + 1;
	if (rdecpt < 0)
		len -= rdecpt;
	if (sign)
		len++;
	if (fieldwidth > len)
		while (fieldwidth-- > len)
			*rs++ = ' ';
	if (sign)
		*rs++ = '-';
	if (rdecpt > 0) {
		len = rdecpt;
		while (*b && len-- > 0)
			*rs++ = *b++;
	}
#ifndef	V7_FLOATSTYLE
	else {
		*rs++ = '0';
	}
#endif
	*rs++ = '.';
	if (rdecpt < 0) {
		len = rdecpt;
		while (len++ < 0 && ndigits-- > 0)
			*rs++ = '0';
	}
	while (*b && ndigits-- > 0)
		*rs++ = *b++;
#ifdef	USE_ECVT
	while (ndigits-- > 0)
		*rs++ = '0';
#endif
	*rs = '\0';
	return rs - s;
}

LOCAL int
_ferr(s, val)
	char	*s;
	double	val;
{
	if (isnan(val)){
		strcpy(s, _nan);
		return (sizeof (_nan) - 1);
	}

	/*
	 * Check first for NaN because finite() will return 1 on Nan too.
	 */
	if (isinf(val)){
		strcpy(s, _inf);
		return (sizeof (_inf) - 1);
	}
	return 0;
}
#endif	/* NO_FLOATINGPOINT */
