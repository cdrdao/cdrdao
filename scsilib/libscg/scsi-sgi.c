/* @(#)scsi-sgi.c	1.18 99/09/07 Copyright 1997 J. Schilling */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-sgi.c	1.18 99/09/07 Copyright 1997 J. Schilling";
#endif
/*
 *	Interface for the SGI generic SCSI implementation.
 *
 *	First Hacky implementation
 *	(needed libds, did not support bus scanning and had no error checking)
 *	from "Frank van Beek" <frank@neogeo.nl>
 *
 *	Actual implementation supports all scg features.
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

#include <dslib.h>

#ifdef	USE_DSLIB

struct dsreq * dsp = 0;
#define	MAX_SCG		1	/* Max # of SCSI controllers */

#else

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

#endif

#define	MAX_DMA_SGI	(256*1024)	/* Check if this is not too big */


#ifndef	USE_DSLIB
LOCAL	int	scsi_sendreq	__PR((SCSI *scgp, int f, struct scg_cmd *sp, struct dsreq *dsp));
#endif


EXPORT int
scsi_open(scgp, device, busno, tgt, tlun)
	SCSI	*scgp;
	char	*device;
	int	busno;
	int	tgt;
	int	tlun;
{
	register int	f;
	register int	b;
	register int	t;
	register int	l;
	register int	nopen = 0;
	char		devname[64];

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
		errno = EINVAL;
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Open by 'devname' not supported on this OS");
		return (-1);
	}

	if (scgp->local == NULL) {
		scgp->local = malloc(sizeof(struct scg_local));
		if (scgp->local == NULL)
			return (0);

		for (b=0; b < MAX_SCG; b++) {
			for (t=0; t < MAX_TGT; t++) {
				for (l=0; l < MAX_LUN ; l++)
					scglocal(scgp)->scgfiles[b][t][l] = (short)-1;
			}
		}
	}

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {	

		sprintf(devname, "/dev/scsi/sc%dd%dl%d", busno, tgt, tlun);
#ifdef	USE_DSLIB
		dsp = dsopen(devname, O_RDWR);
		if (dsp == 0)
			return(-1);
#else
		f = open(devname, O_RDWR);
		if (f < 0)
			return(-1);
		scglocal(scgp)->scgfiles[busno][tgt][tlun] = f;
#endif
		return(1);
	} else {
#ifdef	USE_DSLIB
		return(-1);
#else
		for (b=0; b < MAX_SCG; b++) {
			for (t=0; t < MAX_TGT; t++) {
/*				for (l=0; l < MAX_LUN ; l++) {*/
				for (l=0; l < 1 ; l++) {
					sprintf(devname, "/dev/scsi/sc%dd%dl%d", b, t, l);
					f = open(devname, O_RDWR);
					if (f >= 0) {
						scglocal(scgp)->scgfiles[b][t][l] = (short)f;
						nopen++;
					}
				}
			}
		}
#endif
	}
	return (nopen);
}

EXPORT int
scsi_close(scgp)
	SCSI	*scgp;
{
#ifndef	USE_DSLIB
	register int	f;
	register int	b;
	register int	t;
	register int	l;

	if (scgp->local == NULL)
		return (-1);

	for (b=0; b < MAX_SCG; b++) {
		for (t=0; t < MAX_TGT; t++) {
			for (l=0; l < MAX_LUN ; l++)
				f = scglocal(scgp)->scgfiles[b][t][l];
				if (f >= 0)
					close(f);
				scglocal(scgp)->scgfiles[b][t][l] = (short)-1;
		}
	}
#else
	dsclose(dsp);
#endif
	return (0);
}

LOCAL long
scsi_maxdma(scgp)
	SCSI	*scgp;
{
	return	(MAX_DMA_SGI);
}

EXPORT void *
scsi_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	if (amt <= 0 || amt > scsi_maxdma(scgp))
		return ((void *)0);
	if (scgp->debug)
		printf("scsi_getbuf: %ld bytes\n", amt);
	scgp->bufbase = valloc((size_t)(amt));
	return (scgp->bufbase);
}

EXPORT void
scsi_freebuf(scgp)
	SCSI	*scgp;
{
	if (scgp->bufbase)
		free(scgp->bufbase);
	scgp->bufbase = NULL;
}

EXPORT
BOOL scsi_havebus(scgp, busno)
	SCSI	*scgp;
	int	busno;
{
	register int	t;
	register int	l;

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (scgp->local == NULL)
		return (FALSE);

	for (t=0; t < MAX_TGT; t++) {
		for (l=0; l < MAX_LUN ; l++)
			if (scglocal(scgp)->scgfiles[busno][t][l] >= 0)
				return (TRUE);
	}
	return (FALSE);
}

EXPORT
int scsi_fileno(scgp, busno, tgt, tlun)
	SCSI	*scgp;
	int	busno;
	int	tgt;
	int	tlun;
{
#ifdef	USE_DSLIB
	if (dsp == NULL)
		return (-1);
	return (getfd(dsp));
#else
	if (busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	if (scgp->local == NULL)
		return (-1);

	return ((int)scglocal(scgp)->scgfiles[busno][tgt][tlun]);
#endif
}

EXPORT int
scsi_initiator_id(scgp)
	SCSI	*scgp;
{
	return (-1);
}

EXPORT
int scsi_isatapi(scgp)
	SCSI	*scgp;
{
	return (FALSE);
}

EXPORT
int scsireset(scgp)
	SCSI	*scgp;
{
	/*
	 * Do we have a SCSI reset on SGI?
	 */
#ifdef	DS_RESET
	int	f = scsi_fileno(scgp, scgp->scsibus, scgp->target, scgp->lun);

	return (ioctl(f, DS_RESET, 0));
#else
	return (-1);
#endif
}

#ifndef	USE_DSLIB
LOCAL int
scsi_sendreq(scgp, f, sp, dsp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
	struct dsreq	*dsp;
{
	int	ret;
	int	retries = 4;
	u_char	status;

/*	if ((sp->flags & SCG_CMD_RETRY) == 0)*/
/*		retries = 0;*/

	while (--retries > 0) {
		ret = ioctl(f, DS_ENTER, dsp);
		if (ret < 0)  {
			RET(dsp) = DSRT_DEVSCSI;
			return (-1);
		}
		/*
		 * SGI implementattion botch!!!
		 * If a target does not select the bus,
		 * the return code is DSRT_TIMEOUT
		 */
		if (RET(dsp) == DSRT_TIMEOUT) {
			struct timeval to;

			to.tv_sec = TIME(dsp)/1000;
			to.tv_usec = TIME(dsp)%1000;
			scsitimes(scgp);

			if (sp->cdb.g0_cdb.cmd == SC_TEST_UNIT_READY &&
			    scgp->cmdstop->tv_sec < to.tv_sec ||
			    (scgp->cmdstop->tv_sec == to.tv_sec &&
				scgp->cmdstop->tv_usec < to.tv_usec)) {

				RET(dsp) = DSRT_NOSEL;
				return (-1);
			}
		}
		if (RET(dsp) == DSRT_NOSEL)
			continue;		/* retry noselect 3X */

		status = STATUS(dsp);
		switch (status) {

		case 0x08:		/*  BUSY */
		case 0x18:		/*  RESERV CONFLICT */
			if (retries > 0)
				sleep(2);
			continue;
		case 0x00:		/*  GOOD */
		case 0x02:		/*  CHECK CONDITION */
		case 0x10:		/*  INTERM/GOOD */
		default:
			return (status);
		}
	}
	return (-1);	/* failed retry limit */
}
#endif

LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	int	ret;
	int	i;
	int	amt = sp->cdb_len;
	int flags;
#ifndef	USE_DSLIB
	struct dsreq	ds;
	struct dsreq	*dsp = &ds;

	dsp->ds_iovbuf = 0;
	dsp->ds_iovlen = 0;
#endif
	
	if (f < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	flags = DSRQ_SENSE;
	if (sp->flags & SCG_RECV_DATA)
		flags |= DSRQ_READ;
	else if (sp->size > 0)
		flags |= DSRQ_WRITE;
	
	dsp->ds_flags	= flags;
	dsp->ds_link	= 0;
	dsp->ds_synch	= 0;
	dsp->ds_ret  	= 0;

	DATABUF(dsp) 	= sp->addr;
	DATALEN(dsp)	= sp->size;
	CMDBUF(dsp)	= (void *) &sp->cdb;
	CMDLEN(dsp)	= sp->cdb_len;
	SENSEBUF(dsp)	= sp->u_sense.cmd_sense;
	SENSELEN(dsp)	= sizeof (sp->u_sense.cmd_sense);
	TIME(dsp)	= (sp->timeout * 1000) + 100;
	
	errno		= 0;
	sp->ux_errno	= 0;
	sp->sense_count	= 0;

#ifdef	USE_DSLIB
	ret = doscsireq(f, dsp);
#else
	ret = scsi_sendreq(scgp, f, sp, dsp);
#endif

	if (RET(dsp) != DSRT_DEVSCSI)
		ret = 0;

	if (RET(dsp)) {
		if (RET(dsp) == DSRT_SHORT) {
			sp->resid = DATALEN(dsp)- DATASENT(dsp);
		} else if (errno) {
			sp->ux_errno = errno;
		} else {
			sp->ux_errno = EIO;
		}

		sp->u_scb.cmd_scb[0] = STATUS(dsp);

		sp->sense_count = SENSESENT(dsp);
		if (sp->sense_count > SCG_MAX_SENSE)
			sp->sense_count = SCG_MAX_SENSE;

	}
	switch(RET(dsp)) {

	default:
		sp->error = SCG_RETRYABLE;	break;

	case 0:			/* OK		 		      */
	case DSRT_SHORT:	/* not implemented 		      */
		sp->error = SCG_NO_ERROR;	break;

	case DSRT_UNIMPL:	/* not implemented 		      */
	case DSRT_REVCODE:	/* software obsolete must recompile   */
	case DSRT_NOSEL:
		sp->u_scb.cmd_scb[0] = 0;
		sp->error = SCG_FATAL;		break;

	case DSRT_TIMEOUT:
		sp->u_scb.cmd_scb[0] = 0;
		sp->error = SCG_TIMEOUT;	break;
	}
	return (ret);
}
