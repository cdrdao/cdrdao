/* @(#)scsi-sco.c	1.12 99/09/07 Copyright 1998 J. Schilling, Santa Cruz Operation */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-sco.c	1.12 99/09/07 Copyright 1998 J. Schilling, Santa Cruz Operation";
#endif
/*
 *	Interface for the SCO SCSI implementation.
 *
 *	Copyright (c) 1998 J. Schilling, Santa Cruz Operation
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

#include <sys/scsicmd.h>

#define	MAX_SCG		16		/* Max # of cdrom devices */
#define	MAX_TGT		16		/* Not really needed      */
#define	MAX_LUN		8		/* Not really needed      */

#define	MAX_DMA		(32*1024)	/* Maybe even more ???    */

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

LOCAL	void	cp_scg2sco	__PR((struct scsicmd2 *sco, struct scg_cmd *scg));
LOCAL	void	cp_sco2scg	__PR((struct scsicmd2 *sco, struct scg_cmd *scg));

/* -------------------------------------------------------------------------
** SCO OpenServer does not have a generic scsi device driver, which can
** be used to access any configured scsi device. 
**
** But we can use the "Srom" scsi peripherial driver (for CD-ROM devices) 
** to send SCSIUSERCMD's to any target device controlled by the Srom driver,
** as we only want to communicate with a CD(-Writer) device, not with disks
** tapes etc.
**
** The Srom driver hides all info like scsibus, target id and lun from the
** user, so we really don't need the information about this, just the
** name of the special device node to open this device for sending
** all SCSIUSERCMD's.
**
** Therefore we can use the scsibus as the index into the special device
** nodes of all CD-ROM devices, as all CD-ROM devices have "/dev/rcdX" as 
** their device node, with X being the number of the CD-ROM device.
**
** By default, if you installed from a CD-ROM device, you will have 
** a "/dev/rcd0" device. If you installed the CD-Writer as an additional
** device, you will need to use the appropiate number to access it.
**
** The scsibus variable is used as the number of the /dev/rcd device to be 
** used for sending the userland commands to the cdrecorder device, as the 
** scsibus itself is hidden by the Srom driver (same as with target and lun).
**
** Actually we would not need target and lun at all, as it is all hidden by 
** the Srom driver, but for God's sake we will take it to set the according
** scgfiles element.
**
** For example: 
** 
**   busno = 0  means
**
**   /dev/rcd0 is opened for sending cmds via the SCSIUSERCMD2 ioctl.
**
**
** The alternative method to specify the target device is to use the
** the full device name instead of the scsibus.
**
** For example:
**
**  cdrecord dev=/dev/rcd0:5,0 
**
**  will also open /dev/rcd0 to access the CD-Writer.
**
**
*/

EXPORT int
scsi_open(scgp, device, busno, tgt, tlun)
	SCSI	*scgp;
	char	*device;
	int	busno;
	int	tgt;
	int	tlun;
{
	int	f, b, t, l;
	int	nopen = 0;
	char	devname[64];

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

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN ; l++)
					scglocal(scgp)->scgfiles[b][t][l] = (short)-1;
			}
		}
	}

	sprintf(devname, "/dev/rcd%d", busno);
	device = devname;

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {	

		f = open(device, O_RDONLY);

		if (f < 0) {
		
			switch (errno) {
				
			case ENXIO:
				if (scgp->errstr)
					js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
						"No media in %s !\n", device);
			            break;
			
			default   :
				if (scgp->errstr)
					js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
						"Cannot open '%s'.\n", device);
			}
			return(-1);

		}
		
		scglocal(scgp)->scgfiles[busno][tgt][tlun] = f;
		nopen++;
		return(1);
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
	return (MAX_DMA);
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
	return (-1);		/* There may be ATAPI support */
}

EXPORT
int scsireset(scgp)
	SCSI	*scgp;
{
	return(-1);		/* no scsi reset available */
}

LOCAL void
cp_scg2sco(sco, scg)
	struct scsicmd2	*sco;
	struct scg_cmd	*scg;
{
	sco->cmd.data_ptr = (char *) scg->addr;
	sco->cmd.data_len = scg->size;
	sco->cmd.cdb_len  = scg->cdb_len;

	sco->sense_len    = scg->sense_len;
	sco->sense_ptr    = scg->u_sense.cmd_sense;
    
	if (!(scg->flags & SCG_RECV_DATA) && (scg->size > 0))
		sco->cmd.is_write = 1;

	if (scg->cdb_len == SC_G0_CDBLEN )
		memcpy(sco->cmd.cdb, &scg->cdb.g0_cdb, scg->cdb_len);

	if (scg->cdb_len == SC_G1_CDBLEN )
		memcpy(sco->cmd.cdb, &scg->cdb.g1_cdb, scg->cdb_len);

	if (scg->cdb_len == SC_G5_CDBLEN )
		memcpy(sco->cmd.cdb, &scg->cdb.g5_cdb, scg->cdb_len);
}


LOCAL void
cp_sco2scg(sco, scg)
	struct scsicmd2	*sco;
	struct scg_cmd	*scg;
{
	scg->size      = sco->cmd.data_len;

	memset(&scg->scb,               0, sizeof(scg->scb));

	if (sco->sense_len > SCG_MAX_SENSE)
		scg->sense_count = SCG_MAX_SENSE;
	else
		scg->sense_count = sco->sense_len;

	scg->resid = 0;
	scg->u_scb.cmd_scb[0] = sco->cmd.target_sts;

}


LOCAL int
scsi_send(scgp, fd, sp)
	SCSI		*scgp;
	int		fd;
	struct scg_cmd	*sp;
{
	struct scsicmd2	scsi_cmd;

	int	i;

	u_char		sense_buf[SCG_MAX_SENSE];

	memset(&scsi_cmd, 0, sizeof(scsi_cmd));
	memset(sense_buf,  0, sizeof(sense_buf));
	scsi_cmd.sense_ptr = sense_buf;
	scsi_cmd.sense_len = sizeof(sense_buf);
	cp_scg2sco(&scsi_cmd, sp);

	errno = 0;
	for (;;) {
		int	ioctlStatus;

		if ((ioctlStatus = ioctl(fd, SCSIUSERCMD2, &scsi_cmd)) < 0) {
			if (errno == EINTR)
				continue;

			cp_sco2scg(&scsi_cmd, sp);
			sp->ux_errno = errno;
			if (errno == 0)
				sp->ux_errno = EIO;
			sp->error    = SCG_RETRYABLE;

			return (0);
		}
		break;
	}

	cp_sco2scg(&scsi_cmd, sp);
	sp->ux_errno = errno;
	if (scsi_cmd.cmd.target_sts & 0x02) {
		if (errno == 0)
			sp->ux_errno = EIO;
		sp->error    = SCG_RETRYABLE;
	} else {
		sp->error    = SCG_NO_ERROR;
	}

	return 0;
}
#define	sense	u_sense.Sense
