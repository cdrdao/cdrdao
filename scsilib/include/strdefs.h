/* @(#)strdefs.h	1.1 96/06/26 Copyright 1996 J. Schilling */
/*
 *	Definitions for strings
 *
 *	Copyright (c) 1996 J. Schilling
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

#ifndef _STRDEFS_H
#define	_STRDEFS_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif

#ifdef	HAVE_STRING_H
#include <string.h>
#else	/* HAVE_STRING_H */

#ifndef NULL
#define	NULL	0
#endif

extern void *memcpy	__PR((void *, const void *, int));
extern void *memmove	__PR((void *, const void *, int));
extern char *strcpy	__PR((char *, const char *));
extern char *strncpy	__PR((char *, const char *, int));

extern char *strcat	__PR((char *, const char *));
extern char *strncat	__PR((char *, const char *, int));

extern int memcmp	__PR((const void *, const void *, int));
extern int strcmp	__PR((const char *, const char *));
extern int strcoll	__PR((const char *, const char *));
extern int strncmp	__PR((const char *, const char *, int));
extern int strxfrm	__PR((char *, const char *, int));

extern void *memchr	__PR((const void *, int, int));
extern char *strchr	__PR((const char *, int));

extern int strcspn	__PR((const char *, const char *));
/*#pragma int_to_unsigned strcspn*/

extern char *strpbrk	__PR((const char *, const char *));
extern char *strrchr	__PR((const char *, int));

extern int strspn	__PR((const char *, const char *));
/*#pragma int_to_unsigned strspn*/

extern char *strstr	__PR((const char *, const char *));
extern char *strtok	__PR((char *, const char *));
extern void *memset	__PR((void *, int, int));
extern char *strerror	__PR((int));

extern int strlen	__PR((const char *));
/*#pragma int_to_unsigned strlen*/

extern void *memccpy	__PR((void *, const void *, int, int));

extern int strcasecmp	__PR((const char *, const char *));
extern int strncasecmp	__PR((const char *, const char *, int));

/*#define	index	strchr*/
/*#define	rindex	strrchr*/

#endif	/* HAVE_STRING_H */

#endif	/* _STRDEFS_H */
