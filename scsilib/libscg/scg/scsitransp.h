/* @(#)scsitransp.h	1.24 99/09/07 Copyright 1995 J. Schilling */
/*
 *	Definitions for commands that use functions from scsitransp.c
 *
 *	Copyright (c) 1995 J. Schilling
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

#ifndef	_SCSITRANSP_H
#define	_SCSITRANSP_H

typedef struct {
	int	scsibus;	/* SCSI bus #    for next I/O		*/
	int	target;		/* SCSI target # for next I/O		*/
	int	lun;		/* SCSI lun #    for next I/O		*/

	int	flags;		/* Libscg flags (see below)		*/
	int	kdebug;		/* Kernel debug value for next I/O	*/
	int	debug;		/* Debug value for SCSI library		*/
	int	silent;		/* Be silent if value > 0		*/
	int	verbose;	/* Be verbose if value > 0		*/
	int	disre_disable;
	int	deftimeout;
	int	noparity;	/* Do not use SCSI parity fo next I/O	*/
	int	dev;		/* from scsi_cdr.c			*/
	struct scg_cmd *scmd;
	char	*cmdname;
	char	*curcmdname;
	BOOL	running;
	int	error;		/* libscg error number			*/

	struct timeval	*cmdstart;
	struct timeval	*cmdstop;
	const char	**nonstderrs;
	void	*local;		/* Local data from the low level code	*/
	void	*bufbase;	/* needed for scsi_freebuf()		*/
	char	*errstr;	/* Error string for scsi_open/sendmcd	*/
	char	*errptr;	/* Actual write pointer into errstr	*/

	struct scsi_inquiry *inq;
	struct scsi_capacity *cap;
} SCSI;

/*
 * Flags for struct SCSI:
 */
/* NONE yet */

#define	SCSI_ERRSTR_SIZE	4096

/*
 * Libscg error codes:
 */
#define	SCG_ERRBASE		1000000
#define	SCG_NOMEM		1000001

/*
 * From scsitransp.c:
 */

#ifdef __cplusplus
extern "C" {
#endif

extern	int	scsi_open		__PR((SCSI *scgp, char *device, int busno, int tgt, int tlun));
extern	int	scsi_close		__PR((SCSI *scgp));
extern	BOOL	scsi_havebus		__PR((SCSI *scgp, int));
extern	int	scsi_fileno		__PR((SCSI *scgp, int, int, int));
extern	int	scsi_initiator_id	__PR((SCSI *scgp));
extern	int	scsi_isatapi		__PR((SCSI *scgp));
extern	int	scsireset		__PR((SCSI *scgp));
extern	void	*scsi_getbuf		__PR((SCSI *scgp, long));
extern	void	scsi_freebuf		__PR((SCSI *scgp));
extern	long	scsi_bufsize		__PR((SCSI *scgp, long));
extern	void	scsi_setnonstderrs	__PR((SCSI *scgp, const char **));
extern	int	scsicmd			__PR((SCSI *scgp));
extern	int	scsigetresid		__PR((SCSI *scgp));
extern	void	scsiprinterr		__PR((SCSI *scgp));
extern	void	scsiprintcdb		__PR((SCSI *scgp));
extern	void	scsiprintwdata		__PR((SCSI *scgp));
extern	void	scsiprintrdata		__PR((SCSI *scgp));
extern	void	scsiprintresult		__PR((SCSI *scgp));
extern	void	scsiprintstatus		__PR((SCSI *scgp));
extern	void	scsiprbytes		__PR((char *, unsigned char *, int));
extern	void	scsiprsense		__PR((unsigned char *, int));
extern	int	scsi_sense_key		__PR((SCSI *scgp));
extern	int	scsi_sense_code		__PR((SCSI *scgp));
extern	int	scsi_sense_qual		__PR((SCSI *scgp));
#ifdef	_SCSIREG_H
extern	void	scsiprintdev		__PR((struct scsi_inquiry *));
#endif

/*
 * From scsierrmsg.c:
 */
extern	const char	*scsisensemsg	__PR((int, int, int,
						const char **, char *));
#ifdef	_SCGIO_H
extern	void		scsierrmsg	__PR((SCSI *scgp, struct scsi_sense *,
						struct scsi_status *,
						int, const char **));
#endif

/*
 * From scsiopen.c:
 */
extern	SCSI	*open_scsi	__PR((char *scsidev, char *errs, int slen, int debug, int be_verbose));
extern	int	close_scsi	__PR((SCSI * scgp));
extern	void	scsi_settimeout	__PR((SCSI * scgp, int timeout));
extern	SCSI	*scsi_smalloc	__PR((void));
extern	void	scsi_sfree	__PR((SCSI *scgp));

#ifdef __cplusplus
}
#endif

#endif	/* _SCSITRANSP_H */
