/* @(#)fileread.c	1.8 97/04/20 Copyright 1986 J. Schilling */
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
#include <errno.h>
#include "io.h"

static	char	_readerr[]	= "file_read_err";

#ifdef	HAVE_USG_STDIO

int fileread(f, buf, len)
	register FILE	*f;
	void	*buf;
	int	len;
{
	int	cnt;
	register int	n;

	down2(f, _IOREAD, _IORW);

	if (f->_flag & _IONBF){
		cnt = _niread (fileno(f), buf, len);
		if (cnt < 0) {
			f->_flag |= _IOERR;
			if (!(my_flag(f) & _IONORAISE))
				raisecond(_readerr, 0L);
		}
		if (cnt == 0 && len)
			f->_flag |= _IOEOF;
		return cnt;
	}
	cnt = 0;
	while (len > 0) {
		if (f->_cnt <= 0){
			if (_filbuf(f) == EOF)
				break;
			f->_cnt++;
			f->_ptr--;
		}
		n = f->_cnt >= len ? len : f->_cnt;
		buf = (void *)movebytes (f->_ptr, buf, n);
		f->_ptr += n;
		f->_cnt -= n;
		cnt += n;
		len -= n;
	}
	if (!ferror(f))
		return cnt;
	if (!(my_flag(f) & _IONORAISE))
		raisecond(_readerr, 0L);
	return -1;
}

#else

int fileread(f, buf, len)
	register FILE	*f;
	void	*buf;
	int	len;
{
	int	cnt;

	down2(f, _IOREAD, _IORW);

	if (my_flag(f) & _IOUNBUF)
		return _niread(fileno(f), buf, len);
	cnt = fread(buf, 1, len, f);

	if (!ferror(f))
		return cnt;
	if (!(my_flag(f) & _IONORAISE))
		raisecond(_readerr, 0L);
	return -1;
}

#endif	/* HAVE_USG_STDIO */
