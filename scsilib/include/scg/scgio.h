/* @(#)scgio.h	2.14 98/11/29 Copyright 1986 J. Schilling */
/*
 *	Definitions for the SCSI general driver 'scg'
 *
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

#ifndef	_SCGIO_H
#define	_SCGIO_H

#ifndef	_SCGCMD_H
#include <scg/scgcmd.h>
#endif

#if	defined(SVR4)
#include <sys/ioccom.h>
#endif
#if	defined(__STDC__) || defined(SVR4)
#define	SCGIOCMD	_IOWR('G', 1, struct scg_cmd)	/* do a SCSI cmd   */
#define	SCGIORESET	_IO('G', 2)			/* reset SCSI bus  */
#define	SCGIOGDISRE	_IOR('G', 4, int)		/* get sc disre Val*/
#define	SCGIOSDISRE	_IOW('G', 5, int)		/* set sc disre Val*/
#define	SCGIOIDBG	_IO('G', 100)			/* Inc Debug Val   */
#define	SCGIODDBG	_IO('G', 101)			/* Dec Debug Val   */
#define	SCGIOGDBG	_IOR('G', 102, int)		/* get Debug Val   */
#define	SCGIOSDBG	_IOW('G', 103, int)		/* set Debug Val   */
#define	SCIOGDBG	_IOR('G', 104, int)		/* get sc Debug Val*/
#define	SCIOSDBG	_IOW('G', 105, int)		/* set sc Debug Val*/
#else
#define	SCGIOCMD	_IOWR(G, 1, struct scg_cmd)	/* do a SCSI cmd   */
#define	SCGIORESET	_IO(G, 2)			/* reset SCSI bus  */
#define	SCGIOGDISRE	_IOR(G, 4, int)			/* get sc disre Val*/
#define	SCGIOSDISRE	_IOW(G, 5, int)			/* set sc disre Val*/
#define	SCGIOIDBG	_IO(G, 100)			/* Inc Debug Val   */
#define	SCGIODDBG	_IO(G, 101)			/* Dec Debug Val   */
#define	SCGIOGDBG	_IOR(G, 102, int)		/* get Debug Val   */
#define	SCGIOSDBG	_IOW(G, 103, int)		/* set Debug Val   */
#define	SCIOGDBG	_IOR(G, 104, int)		/* get sc Debug Val*/
#define	SCIOSDBG	_IOW(G, 105, int)		/* set sc Debug Val*/
#endif

#define	SCGIO_CMD	SCGIOCMD	/* backward ccompatibility */

#endif	/* _SCGIO_H */
