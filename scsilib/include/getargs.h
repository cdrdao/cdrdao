/* @(#)getargs.h	1.5 96/05/09 Copyright 1985 J. Schilling */
/*
 *	Definitions for getargs()/getallargs()/getfiles()
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

#ifndef	_GETARGS_H
#define	_GETARGS_H

#define	NOARGS		  0	/* No more args			*/
#define	NOTAFLAG	  1	/* Not a flag type argument	*/
#define BADFLAG		(-1)	/* Not a valid flag argument	*/
#define BADFMT		(-2)	/* Error in format string	*/
#define	NOTAFILE	(-3)	/* Seems to be a flag type	*/

typedef	int	(*getargfun)	__PR((const void *, void *));

#endif	/* _GETARGS_H */
