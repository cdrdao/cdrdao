/* @(#)deflts.h	1.3 98/12/05 Copyright 1997 J. Schilling */
/*
 *	Definitions for reading program defaults.
 *
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

#ifndef	_DEFLTS_H
#define	_DEFLTS_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif

#define	DEFLT	"/etc/default"

/*
 * cmd's to defltcntl()
 */
#define	DC_GETFLAGS	0	/* Get actual flags	*/
#define	DC_SETFLAGS	1	/* Set new flags	*/

/*
 * flags to defltcntl()
 *
 * Make sure that when adding features, the default behaviour
 * is the same as old behaviour.
 */
#define	DC_CASE		0x0001	/* Don't ignore case	*/

#define	DC_STD		DC_CASE	/* Default flags	*/

/*
 * Macros to handle flags
 */
#ifndef	TURNON
#define	TURNON(flags, mask)	flags |= mask
#define	TURNOFF(flags, mask)	flags &= ~(mask)
#define	ISON(flags, mask)	(((flags) & (mask)) == (mask))
#define	ISOFF(flags, mask)	(((flags) & (mask)) != (mask))
#endif

extern	int	defltopen	__PR((const char *name));
extern	int	defltclose	__PR((void));
extern	char	*defltread	__PR((const char *name));
extern	int	defltcntl	__PR((int cmd, int flags));

#endif	/* _DEFLTS_H */
