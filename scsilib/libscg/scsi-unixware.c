/* @(#)scsi-unixware.c	1.7 99/09/07 Copyright 1998 J. Schilling, Santa Cruz Operation */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-unixware.c	1.7 99/09/07 Copyright 1998 J. Schilling, Santa Cruz Operation";
#endif
/*
 *	Interface for the SCO UnixWare SCSI implementation.
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


#include <sys/scsi.h>
#include <sys/sdi_edt.h>
#include <sys/sdi.h>

#define	MAX_SCG		16		/* Max # of cdrom devices */
#define	MAX_TGT		16		/* Not really needed      */
#define	MAX_LUN		8		/* Not really needed      */

#define	MAX_DMA		(32*1024)	

#define TMP_DIR		"/tmp"

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

/* -------------------------------------------------------------------------
** SCO UnixWare 2.1.x / UnixWare 7 provide a scsi pass-through mechanism, 
** which can be used to access any configured scsi device. 
**
** Devices to use: /dev/rcdrom/cdrom1 ... /dev/rcdrom/cdromx
**
** Using these device special nodes, does work on UnixWare 2 and UnixWare 7
** and helps us getting around a problem with a lacking parameter in 
** cdrecord - the controller numbe. 
**
** We could use the controller number, scsibus, target and lun to 
** open a device special node like /dev/rcdrom/c0b0t5l0, but this would 
** need the controller number as an additional parameter in the device
** specification of cdrecord.
**
** Therefore is more convinient and also consistent to the OpenServer port
** to use the scsibus number as the index into the numbers of configured
** cdrom devices, using device special nodes like /dev/rcdrom/cdrom1.
**
** One difference in this methodology compared to OpenServer is that 
** UnixWare start with number 1 as the first cdrom device, as OpenServer
** starts with 0.
**
** The choosen interfacing hides all info like scsibus, target id and 
** lun from the user, so we really don't need the information about this, 
** just the name of the special device node to open this device for sending
** all SDI_SEND's.
**
** Therefore we can use the scsibus as the index into the special device
** nodes of all CD-ROM devices, as all CD-ROM devices have an additional
** "/dev/rcrom/cdromX" as their device node, with X being the number of 
** the CD-ROM device.
**
** By default, if you installed from a CD-ROM device, you will have 
** a "/dev/rcdrom/cdrom1" device. If you installed the CD-Writer as an 
** additional device, you will need to use the appropiate number to access it.
**
** For example: 
** 
**   busno = 1  means
**
**   /dev/rcdrom/cdrom1 is opened for sending cmds via the SDI_SEND ioctl.
**
**
** The alternative method to specify the target device is to use the
** the full device name instead of the scsibus.
**
** For example:
**
**  cdrecord dev=/dev/rcdrom/cdrom1:5,0 
**
**  will also open /dev/rcdrom/cdrom1 to access the CD-Writer.
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
	int		c, b, t;
	char		devname[256];
	char		*cdr_dev;
	int		fd;
	dev_t		ptdev;
	struct stat	stbuf;

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

		for (c = 0; c < MAX_SCG; c++) {
			for (b = 0; b < MAX_TGT; b++) {
				for (t = 0; t < MAX_LUN ; t++)
					scglocal(scgp)->scgfiles[c][b][t] = (short)-1;
			}
		}
	}

	if (*device != '\0') {
		cdr_dev = device;
	} else {
		sprintf(devname, "/dev/rcdrom/cdrom%d", busno);
		cdr_dev = devname;
	}
	
	if (busno < 0        || tgt <   0      || tlun < 0 || 
	    busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {	
	    printf("cdrecord: dev=%d,%d,%d invalid\n",	busno, tgt, tlun);
	    return -1;
	}

	/* Check for validity of device node */
	if (stat(cdr_dev, &stbuf) < 0) {
		return (-1);
	}
	if (!S_ISCHR(stbuf.st_mode)) {
		return (-1);
	}

	/* Open CD-Writer device */
	if ((fd = open(cdr_dev, O_RDONLY)) < 0) {
		printf("cdrecord: ");
		
		switch (errno) {
			
		case EIO: printf("Check media in %s !\n", cdr_dev);
			  break;
			
		default : perror(cdr_dev);

		}

		return (-1);
	}

	/* Get pass-through interface device number */
	if (ioctl(fd, B_GETDEV, &ptdev) < 0) {
		printf("cdrecord: get pass-through device B_GETDEV ioctl failed: errno = %d\n", errno);
		close(fd);
		return (-1);
	}

	close(fd);

	/* Make pass-through interface device node */
	sprintf(devname, "%s/cdrecord.%x", TMP_DIR, (int) ptdev);

	if (mknod(devname, S_IFCHR | 0700, ptdev) < 0) {
		if (errno == EEXIST) {
			unlink(devname);

			if (mknod(devname, S_IFCHR | 0700, ptdev) < 0) {
				printf("cdrecord: Cannot mknod %s: errno = %d\n",
					devname, errno);
				return (-1);
			}
		}
		else {
			printf("cdrecord: Cannot mknod %s: errno = %d\n",
				devname, errno);
			return (-1);
		}
	}

	/* Open pass-through device node */
	if ((fd = open(devname, O_RDONLY)) < 0) {
		printf("cdrecord: Cannot open pass-through %s: errno = %d\n", devname, errno);
		return (-1);
	}

	scglocal(scgp)->scgfiles[busno][tgt][tlun] = fd;
	return (1);
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

	scgp->bufbase = (void *) valloc((size_t)(amt));

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
	    tgt   < 0 || tgt   >= MAX_TGT ||
	    tlun  < 0 || tlun  >= MAX_LUN)
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
	/* We do not know if the device we opened is either ATAPI or SCSI */
	return (-1);
}

EXPORT
int scsireset(scgp)
	SCSI	*scgp;
{
	return(-1);		
}

LOCAL int
do_scsi_cmd(scgp, fd, sp)
	SCSI		*scgp;
	int		fd;
	struct scg_cmd	*sp;
{
	int			ret;
	int			i;
	struct sb		scsi_cmd;
	struct scb		*scbp;

	memset(&scsi_cmd,  0, sizeof(scsi_cmd));

	scsi_cmd.sb_type = ISCB_TYPE;
	scbp = &scsi_cmd.SCB;

	scbp->sc_cmdpt = (caddr_t) sp->cdb.cmd_cdb;
	scbp->sc_cmdsz = sp->cdb_len;

	scbp->sc_datapt = sp->addr;
	scbp->sc_datasz = sp->size;

	if (!(sp->flags & SCG_RECV_DATA) && (sp->size > 0))
		scbp->sc_mode = SCB_WRITE;
	else
		scbp->sc_mode = SCB_READ;

	scbp->sc_time = sp->timeout;

	errno = 0;
	for (;;) {
		if ((ret = ioctl(fd, SDI_SEND, &scsi_cmd)) < 0) {
			if (errno == EAGAIN){
				sleep(1);
				continue;
			}
			sp->ux_errno = errno;
			if (errno == 0)
				sp->ux_errno = EIO;
			sp->error = SCG_RETRYABLE;

			return (ret);
		}
		break;
	}

	memset(&sp->u_scb.Scb, 0, sizeof(sp->u_scb.Scb));
	sp->u_scb.cmd_scb[0] = scbp->sc_status;
	sp->resid = scbp->sc_resid;
	sp->ux_errno = errno;

	return 0;
}


LOCAL int
do_scsi_sense(scgp, fd, sp)
	SCSI		*scgp;
	int		fd;
	struct scg_cmd	*sp;
{
	int		ret;
	struct scg_cmd	s_cmd;

	if (sp->sense_len > SCG_MAX_SENSE)
		sp->sense_len = SCG_MAX_SENSE;

	memset((caddr_t)&s_cmd, 0, sizeof(s_cmd));
	
	s_cmd.addr      = (caddr_t) sp->u_sense.cmd_sense;
	s_cmd.size      = sp->sense_len;
	s_cmd.flags     = SCG_RECV_DATA|SCG_DISRE_ENA;
	s_cmd.cdb_len   = SC_G0_CDBLEN;
	s_cmd.sense_len = CCS_SENSE_LEN;
	s_cmd.target    = scgp->target;
	
	s_cmd.cdb.g0_cdb.cmd   = SC_REQUEST_SENSE;
	s_cmd.cdb.g0_cdb.lun   = sp->cdb.g0_cdb.lun;
	s_cmd.cdb.g0_cdb.count = sp->sense_len;

	ret = do_scsi_cmd(scgp, fd, &s_cmd);

	if (ret < 0)
		return (ret);
	
	sp->sense_count = sp->sense_len - s_cmd.resid;
	return (ret);
}

LOCAL int
scsi_send(scgp, fd, sp)
	SCSI		*scgp;
	int		fd;
	struct scg_cmd	*sp;
{
	int	ret;

	ret = do_scsi_cmd(scgp, fd, sp);

	if (ret < 0)
		return (ret);

	if (sp->u_scb.cmd_scb[0] & S_CKCON)
		ret = do_scsi_sense(scgp, fd, sp);

	return (ret);
}

#define	sense	u_sense.Sense
