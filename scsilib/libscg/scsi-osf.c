/* @(#)scsi-osf.c	1.10 99/09/07 Copyright 1998 J. Schilling */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-osf.c	1.10 99/09/07 Copyright 1998 J. Schilling";
#endif
/*
 *	Interface for Digital UNIX (OSF/1 generic SCSI implementation (/dev/cam).
 *
 *	Created out of the hacks from:
 *		Stefan Traby <stefan@sime.com> and
 *		Bruno Achauer <bruno@tk.uni-linz.ac.at>
 *
 *	Copyright (c) 1998 J. Schilling
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

#include <sys/types.h>
#include <io/common/iotypes.h>
#include <io/cam/cam.h>
#include <io/cam/uagt.h> 

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct scg_local {
	int	scgfile = -1;	/* Used for ioctl()	*/
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

LOCAL	int	scgfile = -1;	/* Used for ioctl()	*/

LOCAL	BOOL	scsi_checktgt	__PR((SCSI *scgp, int f, int busno, int tgt, int tlun));

/*
 * I don't have any documentation about CAM
 */
#define	MAX_DMA_OSF_CAM	(64*1024)

#ifndef	AUTO_SENSE_LEN
#	define	AUTO_SENSE_LEN	32	/* SCG_MAX_SENSE */
#endif

EXPORT int
scsi_open(scgp, device, busno, tgt, tlun)
	SCSI	*scgp;
	char	*device;
	int	busno;
	int	tgt;
	int	tlun;
{
	register int	b;
	register int	t;
	register int	l;

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
		scglocal(scgp)->scgfile = -1;

		for (b=0; b < MAX_SCG; b++) {
			for (t=0; t < MAX_TGT; t++) {
				for (l=0; l < MAX_LUN ; l++)
					scglocal(scgp)->scgfiles[b][t][l] = 0;
			}
		}
	}

	if (scglocal(scgp)->scgfile != -1)	/* multiple opens ??? */
		return (1);			/* not yet ready .... */

	if ((scglocal(scgp)->scgfile = open("/dev/cam", O_RDWR, 0)) < 0) {
		if (scgp->errstr)
			js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
				"Cannot open '/dev/cam');
		return (-1);
	}

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {
		/* scsi_checktgt() ??? */
		for (l=0; l < MAX_LUN ; l++)
			scglocal(scgp)->scgfiles[b][t][l] = 1;
		return (1);
	}
	/*
	 * There seems to be no clean way to check whether
	 * a SCSI bus is present in the current system.
	 * scsi_checktgt() is used as a workaround for this problem.
	 */
	for (b=0; b < MAX_SCG; b++) {
		for (t=0; t < MAX_TGT; t++) {
			if (scsi_checktgt(scgp, scglocal(scgp)->scgfile, b, t, 0)) {
				for (l=0; l < MAX_LUN ; l++)
					scglocal(scgp)->scgfiles[b][t][l] = 1;
				/*
				 * Found a target on this bus.
				 * Comment the 'break' for a complete scan.
				 */
				break;
			}
		}
	}
	return (1);
}

EXPORT int
scsi_close(scgp)
	SCSI	*scgp;
{
	if (scgp->local == NULL)
		return (-1);

	if (scglocal(scgp)->scgfile >= 0)
		close(scglocal(scgp)->scgfile);
	scglocal(scgp)->scgfile = -1;
	return (0);
}

/*
 * We send a test unit ready command to the target to check whether the
 * OS is considering this target to be valid.
 */
LOCAL BOOL
scsi_checktgt(scgp, f, busno, tgt, tlun)
	SCSI	*scgp;
	int	f;
	int	busno;
	int	tgt;
	int	tlun;
{
	struct scg_cmd	sc;
	int	obus = scgp->scsibus;
	int	otgt = scgp->target;
	int	olun = scgp->lun;

	scgp->scsibus = busno;
	scgp->target  = tgt;
	scgp->lun     = tlun;
	
	fillbytes((caddr_t)&sc, sizeof(sc), '\0');
	sc.addr = (caddr_t)0;
	sc.size = 0;
	sc.flags = SCG_DISRE_ENA | SCG_SILENT;
	sc.cdb_len = SC_G0_CDBLEN;
	sc.sense_len = CCS_SENSE_LEN;
	sc.target = scgp->target;
	sc.cdb.g0_cdb.cmd = SC_TEST_UNIT_READY;
	sc.cdb.g0_cdb.lun = scgp->lun;

	scsi_send(scgp, f, &sc);
	scgp->scsibus = obus;
	scgp->target  = otgt;
	scgp->lun     = olun;

	if (sc.error != SCG_FATAL)
		return (TRUE);
	return (sc.ux_errno != EINVAL);
}


LOCAL long
scsi_maxdma(scgp)
	SCSI	*scgp;
{
	long maxdma = MAX_DMA_OSF_CAM;

	return (maxdma);
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

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (scgp->local == NULL)
		return (FALSE);

	for (t=0; t < MAX_TGT; t++) {
		if (scglocal(scgp)->scgfiles[busno][t][0] != 0)
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
	if (scgp->local == NULL)
		return (-1);

	return (busno < 0 || busno >= MAX_SCG) ? -1 : scglocal(scgp)->scgfile;
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
#ifdef	XXX
	int	f = scsi_fileno(scgp, scgp->scsibus, scgp->target, scgp->lun);

	return (ioctl(f, SCGIORESET, 0));
#else
	return (-1);
#endif
}

LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	CCB_SCSIIO ccb;
	UAGT_CAM_CCB ua;
	unsigned char  *cdb;
	CCB_RELSIM relsim;
	UAGT_CAM_CCB relua;
	int             i;

	if (f < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	fillbytes(&ua, sizeof(UAGT_CAM_CCB), 0);
	fillbytes(&ccb, sizeof(CCB_SCSIIO), 0);

	ua.uagt_ccb = (CCB_HEADER *) &ccb;
	ua.uagt_ccblen = sizeof(CCB_SCSIIO);
	ccb.cam_ch.my_addr = (CCB_HEADER *) &ccb;
	ccb.cam_ch.cam_ccb_len = sizeof(CCB_SCSIIO);

	ua.uagt_snsbuf = ccb.cam_sense_ptr = sp->u_sense.cmd_sense;
	ua.uagt_snslen = ccb.cam_sense_len = AUTO_SENSE_LEN;

	cdb = (unsigned char *) ccb.cam_cdb_io.cam_cdb_bytes;

	ccb.cam_timeout = sp->timeout;

	ccb.cam_data_ptr = ua.uagt_buffer = (u_char *) sp->addr;
	ccb.cam_dxfer_len = ua.uagt_buflen = sp->size;
	ccb.cam_ch.cam_func_code = XPT_SCSI_IO;
	ccb.cam_ch.cam_flags = 0;	/* CAM_DIS_CALLBACK; */

	if (sp->size == 0) {
		ccb.cam_data_ptr = ua.uagt_buffer = (u_char *) NULL;
		ccb.cam_ch.cam_flags |= CAM_DIR_NONE;
	} else {
		if (sp->flags & SCG_RECV_DATA) {
			ccb.cam_ch.cam_flags |= CAM_DIR_IN;
		} else {
			ccb.cam_ch.cam_flags |= CAM_DIR_OUT;
		}
	}

	ccb.cam_cdb_len = sp->cdb_len;
	for (i = 0; i < sp->cdb_len; i++)
		cdb[i] = sp->cdb.cmd_cdb[i];

	ccb.cam_ch.cam_path_id	  = scgp->scsibus;
	ccb.cam_ch.cam_target_id  = scgp->target;
	ccb.cam_ch.cam_target_lun = scgp->lun;

	sp->sense_count = 0;
	sp->ux_errno = 0;
	sp->error = SCG_NO_ERROR;


	if (ioctl(f, UAGT_CAM_IO, (caddr_t) &ua) < 0) {
		sp->ux_errno = geterrno();
		sp->error = SCG_FATAL;
		if (scgp->debug) {
			errmsg("ioctl(f, UAGT_CAM_IO, dev=%d,%d,%d) failed.\n",
					scgp->scsibus, scgp->target, scgp->lun);
		}
		return (0);
	}
	if (scgp->debug) {
		errmsgno(EX_BAD, "cam_status = 0x%.2X dev=%d,%d,%d\n",
					ccb.cam_ch.cam_status,
					scgp->scsibus, scgp->target, scgp->lun);
		fflush(stderr);
	}
	switch (ccb.cam_ch.cam_status & CAM_STATUS_MASK) {

	case CAM_REQ_CMP:	break;

	case CAM_SEL_TIMEOUT:	sp->error = SCG_FATAL;
				sp->ux_errno = EIO;
				break;

	case CAM_CMD_TIMEOUT:	sp->error = SCG_TIMEOUT;
				sp->ux_errno = EIO;
				break;

	default:		sp->error = SCG_RETRYABLE;
				sp->ux_errno = EIO;
				break;
	}

	sp->u_scb.cmd_scb[0] = ccb.cam_scsi_status;

	if (ccb.cam_ch.cam_status & CAM_AUTOSNS_VALID) {
		sp->sense_count = MIN(ccb.cam_sense_len - ccb.cam_sense_resid,
			SCG_MAX_SENSE);
		sp->sense_count = MIN(sp->sense_count, sp->sense_len);
		if (sp->sense_len < 0)
			sp->sense_count = 0;
	}
	sp->resid = ccb.cam_resid;


	/*
	 * this is perfectly wrong.
	 * But without this, we hang...
	 */
	if (ccb.cam_ch.cam_status & CAM_SIM_QFRZN) {
		fillbytes(&relsim, sizeof(CCB_RELSIM), 0);
		relsim.cam_ch.cam_ccb_len = sizeof(CCB_SCSIIO);
		relsim.cam_ch.cam_func_code = XPT_REL_SIMQ;
		relsim.cam_ch.cam_flags = CAM_DIR_IN | CAM_DIS_CALLBACK;
		relsim.cam_ch.cam_path_id	= scgp->scsibus;
		relsim.cam_ch.cam_target_id	= scgp->target;
		relsim.cam_ch.cam_target_lun	= scgp->lun;

		relua.uagt_ccb = (struct ccb_header *) & relsim;	/* wrong cast */
		relua.uagt_ccblen = sizeof(relsim);
		relua.uagt_buffer = NULL;
		relua.uagt_buflen = 0;

		if (ioctl(f, UAGT_CAM_IO, (caddr_t) & relua) < 0)
			errmsg("DEC CAM -> LMA\n");
	}
	return 0;
}
