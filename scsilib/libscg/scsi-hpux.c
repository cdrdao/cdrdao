/* @(#)scsi-hpux.c	1.15 99/09/07 Copyright 1997 J. Schilling */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-hpux.c	1.15 99/09/07 Copyright 1997 J. Schilling";
#endif
/*
 *	Interface for the HP-UX generic SCSI implementation.
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

#undef	sense
#include <sys/scsi.h>

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

#ifdef	SCSI_MAXPHYS
#	define	MAX_DMA_HP	SCSI_MAXPHYS
#else
#	define	MAX_DMA_HP	(63*1024)	/* Check if this is not too big */
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

		sprintf(devname, "/dev/rscsi/c%dt%dl%d", busno, tgt, tlun);
		f = open(devname, O_RDWR);
		if (f < 0)
			return(-1);
		scglocal(scgp)->scgfiles[busno][tgt][tlun] = f;
		return(1);
	} else {
		for (b=0; b < MAX_SCG; b++) {
			for (t=0; t < MAX_TGT; t++) {
/*				for (l=0; l < MAX_LUN ; l++) {*/
				for (l=0; l < 1 ; l++) {
					sprintf(devname, "/dev/rscsi/c%dt%dl%d", b, t, l);
/*error("name: '%s'\n", devname);*/
					f = open(devname, O_RDWR);
					if (f >= 0) {
						scglocal(scgp)->scgfiles[b][t][l] = (short)f;
						nopen++;
					} else if (scgp->debug) {
						errmsg("open '%s'\n", devname);
					}
				}
			}
		}
	}
	return (nopen);
}

EXPORT int
scsi_close(scgp)
	SCSI	*scgp;
{
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
	return (0);
}

LOCAL long
scsi_maxdma(scgp)
	SCSI	*scgp;
{
	return	(MAX_DMA_HP);
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
	if (busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	if (scgp->local == NULL)
		return (-1);

	return ((int)scglocal(scgp)->scgfiles[busno][tgt][tlun]);
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
	int	f = scsi_fileno(scgp, scgp->scsibus, scgp->target, scgp->lun);

	return (ioctl(f, SIOC_RESET_BUS, 0));
}

LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	int	ret;
	int	flags;
	struct sctl_io	sctl_io;

	if ((f < 0) || (sp->cdb_len > sizeof(sctl_io.cdb))) {
		sp->error = SCG_FATAL;
		return (0);
	}

	fillbytes((caddr_t)&sctl_io, sizeof(sctl_io), '\0');

	flags = 0;
/*	flags = SCTL_INIT_WDTR|SCTL_INIT_SDTR;*/
	if (sp->flags & SCG_RECV_DATA)
		flags |= SCTL_READ;
	if ((sp->flags & SCG_DISRE_ENA) == 0)
		flags |= SCTL_NO_ATN;

	sctl_io.flags		= flags;

	movebytes(&sp->cdb, sctl_io.cdb, sp->cdb_len);
	sctl_io.cdb_length	= sp->cdb_len;

	sctl_io.data_length	= sp->size;
	sctl_io.data		= sp->addr;

	if (sp->timeout == 0)
		sctl_io.max_msecs = 0;
	else
		sctl_io.max_msecs = (sp->timeout * 1000) + 500;

	errno		= 0;
	sp->error	= SCG_NO_ERROR;
	sp->sense_count	= 0;
	sp->u_scb.cmd_scb[0] = 0;
	sp->resid	= 0;

	ret = ioctl(f, SIOC_IO, &sctl_io);
	if (ret < 0) {
		sp->error = SCG_FATAL;
		sp->ux_errno = errno;
		return (ret);
	}
if (scgp->debug)
error("cdb_status: %X, size: %d xfer: %d\n", sctl_io.cdb_status, sctl_io.data_length, sctl_io.data_xfer);

	if (sctl_io.cdb_status == 0 || sctl_io.cdb_status == 0x02)
		sp->resid = sp->size - sctl_io.data_xfer;

	if (sctl_io.cdb_status & SCTL_SELECT_TIMEOUT ||
			sctl_io.cdb_status & SCTL_INVALID_REQUEST) {
		sp->error = SCG_FATAL;
	} else if (sctl_io.cdb_status & SCTL_INCOMPLETE) {
		sp->error = SCG_TIMEOUT;
	} else if (sctl_io.cdb_status > 0xFF) {
		errmsgno(EX_BAD, "SCSI problems: cdb_status: %X\n", sctl_io.cdb_status);

	} else if ((sctl_io.cdb_status & 0xFF) != 0) {
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = EIO;

		sp->u_scb.cmd_scb[0] = sctl_io.cdb_status & 0xFF;

		sp->sense_count = sctl_io.sense_xfer;
		if (sp->sense_count > SCG_MAX_SENSE)
			sp->sense_count = SCG_MAX_SENSE;

		if (sctl_io.sense_status != S_GOOD) {
			sp->sense_count = 0;
		} else {
			movebytes(sctl_io.sense, sp->u_sense.cmd_sense, sp->sense_count);
		}

	}
	return (ret);
}
#define	sense	u_sense.Sense
