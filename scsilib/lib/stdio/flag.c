/* @(#)flag.c	2.5 98/09/05 Copyright 1986 J. Schilling */
/*
 *	Copyright (c) 1986 J. Schilling
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
#include <stdxlib.h>

#ifdef	DO_MYFLAG

#define	FL_INIT	10

int	_io_glflag;		/* global default flag */
int	_fl_inc = 10;		/* increment for expanding flag struct */
int	_fl_max = FL_INIT;	/* max fd currently in _io_myfl */
_io_fl	_io_smyfl[FL_INIT];	/* initial static space */
_io_fl	*_io_myfl = _io_smyfl;	/* init to static space */

LOCAL int _more_flags	__PR((FILE * ));

LOCAL int _more_flags(fp)
	FILE	*fp;
{
	register int	f = fileno(fp);
	register int	n = _fl_max;
	register _io_fl	*np;

	while (n <= f)
		n += _fl_inc;

	if (_io_myfl == _io_smyfl) {
		np = (_io_fl *) malloc(n * sizeof(*np));
		fillbytes(np, n * sizeof(*np), '\0');
		movebytes(_io_smyfl, np, sizeof(_io_smyfl)/sizeof(*np));
	} else {
		np = (_io_fl *) realloc(_io_myfl, n * sizeof(*np));
		if (np)
			fillbytes(&np[_fl_max], (n-_fl_max)*sizeof(*np), '\0');
	}
	if (np) {
		_io_myfl = np;
		_fl_max = n;
		return (_io_get_my_flag(fp));
	} else {
		return (_IONORAISE);
	}
}

int _io_get_my_flag(fp)
	register FILE	*fp;
{
	register int	f = fileno(fp);
	register _io_fl	*fl;

	if (f >= _fl_max)
		return (_more_flags(fp));

	fl = &_io_myfl[f];

	if (fl->fl_io == 0 || fl->fl_io == fp)
		return (fl->fl_flags);

	while (fl && fl->fl_io != fp)
		fl = fl->fl_next;

	if (fl == 0)
		return (0);

	return (fl->fl_flags);
}

void _io_set_my_flag(fp, flag)
	FILE	*fp;
	int	flag;
{
	register int	f = fileno(fp);
	register _io_fl	*fl;
	register _io_fl	*fl2;

	if (f >= _fl_max)
		(void) _more_flags(fp);

	fl = &_io_myfl[f];

	if (fl->fl_io != (FILE *)0) {
		fl2 = fl;

		while (fl && fl->fl_io != fp)
			fl = fl->fl_next;
		if (fl == 0) {
			if ((fl = (_io_fl *) malloc(sizeof(*fl))) == 0)
				return;
			fl->fl_next = fl2->fl_next;
			fl2->fl_next = fl;
		}
	}
	fl->fl_io = fp;
	fl->fl_flags = flag;
}

void _io_add_my_flag(fp, flag)
	FILE	*fp;
	int	flag;
{
	int	oflag = _io_get_my_flag(fp);

	oflag |= flag;

	_io_set_my_flag(fp, oflag);
}

#endif	/* DO_MYFLAG */
