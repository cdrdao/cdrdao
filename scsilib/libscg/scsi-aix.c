/* @(#)scsi-aix.c	1.17 99/09/07 Copyright 1997 J. Schilling */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-aix.c	1.17 99/09/07 Copyright 1997 J. Schilling";
#endif
/*
 *	Interface for the AIX generic SCSI implementation.
 *
 *	This is a hack, that tries to emulate the functionality
 *	of the scg driver.
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

#include <sys/scdisk.h>

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

#define MAX_DMA_AIX (64*1024)

LOCAL	int	do_scsi_cmd	__PR((SCSI *scgp, int f, struct scg_cmd *sp));
LOCAL	int	do_scsi_sense	__PR((SCSI *scgp, int f, struct scg_cmd *sp));

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
	char		devname[32];

	
	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
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

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
		goto openbydev;

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {	

		sprintf(devname, "/dev/rcd%d", tgt);
		f = openx(devname, 0, 0, SC_DIAGNOSTIC);
		if (f < 0) {
			if (scgp->errstr)
				js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'. Specify device number (1 for cd1) as target (1,0)",
					devname);
			return (0);
		}
		scglocal(scgp)->scgfiles[busno][tgt][tlun] = f;
		return 1;
	} else {
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Unable to scan on AIX");
		return (0);
	}
openbydev:
	if (device != NULL && *device != '\0' && busno >= 0 && tgt >= 0 && tlun >= 0) {
		f = openx(device, 0, 0, SC_DIAGNOSTIC);
		if (f < 0) {
			if (scgp->errstr)
				js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'",
					devname);
			return (0);
		}

		scgp->scsibus = busno;
		scgp->target = tgt;
		scgp->lun = tlun;
		scglocal(scgp)->scgfiles[busno][tgt][tlun] = f;

		return (++nopen);
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
	return (MAX_DMA_AIX);
}

#define palign(x, a)	(((char *)(x)) + ((a) - 1 - (((unsigned)((x)-1))%(a))))

EXPORT void *
scsi_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	void	*ret;
	int	pagesize;

#ifdef	_SC_PAGESIZE
	pagesize = sysconf(_SC_PAGESIZE);
#else
	pagesize = getpagesize();
#endif

	if (amt <= 0 || amt > scsi_maxdma(scgp))
		return ((void *)0);
	if (scgp->debug)
		printf("scsi_getbuf: %ld bytes\n", amt);
	/*
	 * Damn AIX is a paged system but has no valloc()
	 */
	scgp->bufbase = ret = malloc((size_t)(amt+pagesize));
	if (ret == NULL)
		return (ret);
	ret = palign(ret, pagesize);
	return (ret);
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

	return (ioctl(f, SCIORESET, IDLUN(scgp->target, scgp->lun)));
}

LOCAL int
do_scsi_cmd(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	struct sc_iocmd req;
	int	ret;

	if (sp->cdb_len > 12)
		comerrno(EX_BAD, "Can't do %d byte command.\n", sp->cdb_len);

	fillbytes(&req, sizeof(req), '\0');

	req.flags = SC_ASYNC;
	if (sp->flags & SCG_RECV_DATA) {
		req.flags |= B_READ;
	} else if (sp->size > 0) {
		req.flags |= B_WRITE;
	}
	req.data_length = sp->size;
	req.buffer = sp->addr;
	req.timeout_value = sp->timeout;
	req.command_length = sp->cdb_len;

	movebytes(&sp->cdb, req.scsi_cdb, 12);
	errno = 0;
	ret = ioctl(f, DKIOCMD, &req);

	if (scgp->debug) {
		printf("ret: %d errno: %d (%s)\n", ret, errno, errmsgstr(errno));
		printf("data_length:     %d\n", req.data_length);
		printf("buffer:          0x%X\n", req.buffer);
		printf("timeout_value:   %d\n", req.timeout_value);
		printf("status_validity: %d\n", req.status_validity);
		printf("scsi_bus_status: 0x%X\n", req.scsi_bus_status);
		printf("adapter_status:  0x%X\n", req.adapter_status);
		printf("adap_q_status:   0x%X\n", req.adap_q_status);
		printf("q_tag_msg:       0x%X\n", req.q_tag_msg);
		printf("flags:           0X%X\n", req.flags);
	}
	if (ret < 0) {
		sp->ux_errno = geterrno();
		/*
		 * Check if SCSI command cound not be send at all.
		 */
		if (sp->ux_errno == ENOTTY || sp->ux_errno == ENXIO ||
		    sp->ux_errno == EINVAL || sp->ux_errno == EACCES) {
			return (-1);
		}
	} else {
		sp->ux_errno = 0;
	}
	ret = 0;
	sp->sense_count = 0;
	sp->resid = 0;		/* AIX is the same rubbish as Linux here */

	fillbytes(&sp->scb, sizeof(sp->scb), '\0');
	fillbytes(&sp->u_sense.cmd_sense, sizeof(sp->u_sense.cmd_sense), '\0');

	if (req.status_validity == 0) {
		sp->error = SCG_NO_ERROR;
		return (0);
	}
	if (req.status_validity & 1) {
		sp->u_scb.cmd_scb[0] = req.scsi_bus_status;
		sp->error = SCG_RETRYABLE;
	}
	if (req.status_validity & 2) {
		if (req.adapter_status & SC_NO_DEVICE_RESPONSE) {
			sp->error = SCG_FATAL;

		} else if (req.adapter_status & SC_CMD_TIMEOUT) {
			sp->error = SCG_TIMEOUT;

		} else if (req.adapter_status != 0) {
			sp->error = SCG_RETRYABLE;
		}
	}

	return (ret);
}

LOCAL int
do_scsi_sense(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	int		ret;
	struct scg_cmd	s_cmd;

	if (sp->sense_len > SCG_MAX_SENSE)
		sp->sense_len = SCG_MAX_SENSE;

	fillbytes((caddr_t)&s_cmd, sizeof(s_cmd), '\0');
	s_cmd.addr = sp->u_sense.cmd_sense;
	s_cmd.size = sp->sense_len;
	s_cmd.flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	s_cmd.cdb_len = SC_G0_CDBLEN;
	s_cmd.sense_len = CCS_SENSE_LEN;
	s_cmd.target = scgp->target;
	s_cmd.cdb.g0_cdb.cmd = SC_REQUEST_SENSE;
	s_cmd.cdb.g0_cdb.lun = sp->cdb.g0_cdb.lun;
	s_cmd.cdb.g0_cdb.count = sp->sense_len;
	ret = do_scsi_cmd(scgp, f, &s_cmd);

	if (ret < 0)
		return (ret);
	if (s_cmd.u_scb.cmd_scb[0] & 02) {
		/* XXX ??? Check condition on request Sense ??? */
	}
	sp->sense_count = sp->sense_len - s_cmd.resid;
	return (ret);
}

LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	int	ret;

	if (f < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}
	ret = do_scsi_cmd(scgp, f, sp);
	if (ret < 0)
		return (ret);
	if (sp->u_scb.cmd_scb[0] & 02)
		ret = do_scsi_sense(scgp, f, sp);
	return (ret);
}
