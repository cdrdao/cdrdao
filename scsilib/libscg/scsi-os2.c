/* @(#)scsi-os2.c	1.8 99/09/07 Copyright 1998 J. Schilling, C. Wohlgemuth */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-os2.c	1.8 99/09/07 Copyright 1998 J. Schilling, C. Wohlgemuth";
#endif
/*
 *	Interface for the OS/2 ASPI-Router ASPIROUT.SYS ((c) D. Dorau).
 *		This additional driver is a prerequisite for using cdrecord.
 *		Get it from HOBBES or LEO.
 *
 *	Copyright (c) 1998 J. Schilling
 *	Copyright (c) 1998 C. Wohlgemuth for this interface.
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

/*#define DEBUG*/
/*#define DEBUG2*/
/*#define SCANDEBUG*/

/* For AspiRouter */
#include "scg/srb_os2.h"

#define FILE_OPEN			0x0001
#define OPEN_SHARE_DENYREADWRITE	0x0010
#define OPEN_ACCESS_READWRITE		0x0002
#define DC_SEM_SHARED			0x01
#define OBJ_TILE			0x0040
#define PAG_READ			0x0001
#define PAG_WRITE			0x0002
#define PAG_COMMIT			0x0010

typedef unsigned long LHANDLE;
typedef unsigned long ULONG;
typedef unsigned char *PSZ;
typedef unsigned short USHORT;
typedef unsigned char UCHAR;

typedef LHANDLE	HFILE;
typedef ULONG	HEV;

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

#define	MAX_DMA_OS2	(63*1024) /* ASPI-Router allows up to 64k */

LOCAL	void	*buffer		= NULL;
LOCAL	HFILE	driver_handle	= 0;
LOCAL	HEV	postSema	= 0;
#ifdef	XXX
LOCAL	int	fileHandle;
#endif

LOCAL	BOOL	open_driver	__PR((void));
LOCAL	BOOL	close_driver	__PR((void));
LOCAL	ULONG	wait_post	__PR((ULONG ulTimeOut));
LOCAL	BOOL 	init_buffer	__PR((void* mem));
EXPORT	void	exit_func	__PR((void));
#ifdef	SCANDEBUG
LOCAL	ULONG test_unit_ready_aspi __PR(( UCHAR busno, UCHAR tgt, UCHAR tlun));
#endif


EXPORT void
exit_func()
{
	if (!close_driver())
		fprintf(stderr, "Cannot close OS/2-ASPI-Router!\n");
}

EXPORT int
scsi_open(scgp, device, busno, tgt, tlun)
	SCSI	*scgp;
	char	*device;
	int	busno;
	int	tgt;
	int	tlun;
{
	register int b;
	register int t;
	register int l;
	register int f;
	register int nopen = 0;

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

	if (!open_driver())	/* Try to open ASPI-Router */
		return (-1);
	atexit(exit_func);	/* Install Exit Function which closes the ASPI-Router */
  
	if (busno >= 0 && tgt >= 0 && tlun >= 0) {

		scglocal(scgp)->scgfiles[busno][tgt][tlun] = 1;	/* We do not need it anywhere */
#ifdef	XXX
		fileHandle = open("test.wav", O_BINARY|O_RDWR|O_CREAT|O_SYNC, S_IREAD|S_IWRITE);
		if (fileHandle == -1) {
			fileHandle = 0;
			printf("Cannot open testfile.\n");
		}
#endif
		return (1);
	} else {
		for (b=0; b < MAX_SCG; b++) {
			for (t=0; t < MAX_TGT; t++) {
				for (l=0; l < MAX_LUN ; l++) {
#ifdef	SCANDEBUG
					f = test_unit_ready_aspi(b, t, l);
					if (f == 0) {
						scglocal(scgp)->scgfiles[b][t][l] = (short)f;
						scglocal(scgp)->scgfiles[b][t][l] = (short)1;
						nopen++;
					} else if (scgp->debug) {
						errmsg("open '0x%x 0x%x 0x%x'\n",
									b, t, l);
					}
#else
					scglocal(scgp)->scgfiles[b][t][l] = (short)1;
					nopen++;
#endif
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
	exit_func();
	return (0);
}

LOCAL long
scsi_maxdma(scgp)
	SCSI	*scgp;
{
	long maxdma = MAX_DMA_OS2;
	return (maxdma);
}

EXPORT void *
scsi_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	ULONG rc;

	if (amt <= 0 || amt > scsi_maxdma(scgp))
		return ((void *)0);
#ifdef DEBUG
	printf("scsi_getbuf: %ld bytes\n", amt);
#endif
	rc = DosAllocMem(&buffer, amt, OBJ_TILE | PAG_READ | PAG_WRITE | PAG_COMMIT);

	if (rc) {
		fprintf(stderr, "Cannot allocate buffer.\n");
		return ((void *)0);
	}
	scgp->bufbase = buffer;

#ifdef DEBUG
	printf("Buffer allocated at: 0x%x\n", scgp->bufbase);
#endif

	/* Lock memory */
	if (init_buffer(scgp->bufbase))
		return (scgp->bufbase);

	fprintf(stderr, "Cannot lock memory buffer.\n");
	return ((void *)0); /* Error */
}

EXPORT void
scsi_freebuf(scgp)
	SCSI	*scgp;
{
	if (scgp->bufbase && DosFreeMem(scgp->bufbase))
		fprintf(stderr, "Cannot free buffer memory for ASPI-Router!\n"); /* Free our memory buffer if not already done */
	if (buffer == scgp->bufbase)
		buffer = NULL;
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

EXPORT int
scsi_isatapi(scgp)
	SCSI	*scgp;
{
	return (FALSE);
}


EXPORT
int scsireset(scgp)
	SCSI	*scgp;
{
	ULONG	rc;				/* return value */
	Ulong	cbreturn;
	Ulong	cbParam;
	BOOL	success;
	SRB	SRBlock;

	SRBlock.cmd		= SRB_Reset;	/* reset device */
	SRBlock.ha_num		= scgp->scsibus;/* host adapter number	*/
	SRBlock.flags		= SRB_Post;	/* posting enabled	*/
	SRBlock.u.res.target	= scgp->target;	/* target id		*/
	SRBlock.u.res.lun	= scgp->lun;	/* target LUN		*/

	rc = DosDevIOCtl(driver_handle, 0x92, 0x02, (void*) &SRBlock, sizeof(SRB), &cbParam,
			(void*) &SRBlock, sizeof(SRB), &cbreturn);
	if (rc) {
		fprintf(stderr, "DosDevIOCtl() failed in resetDevice.\n");
		return (1);			/* DosDevIOCtl failed */
	} else {
		success = wait_post(40000);	/** wait for SRB being processed */
		if (success)
			return (2);
	}
	if (SRBlock.status != SRB_Done)
		return (3);
#ifdef DEBUG
	printf("resetDevice of host: %d target: %d lun: %d successful.\n", scgp->scsibus, scgp->target, scgp->lun);
	printf("SRBlock.ha_status: 0x%x, SRBlock.target_status: 0x%x, SRBlock.satus: 0x%x\n",
				 SRBlock.u.cmd.ha_status, SRBlock.u.cmd.target_status, SRBlock.status);
#endif
	return (0);
}

LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	ULONG	rc;				/* return value */
	SRB	SRBlock;
	Ulong	cbreturn;
	Ulong	cbParam;
	UCHAR*	ptr;
	static int done = 0;

	if (!f) {				/* Set in scsi_open() */
		sp->error = SCG_FATAL;
		return (-1);
	}

	if (sp->cdb_len > sizeof(SRBlock.u.cmd.cdb_st)) { /* commandsize too big */
		sp->error = SCG_FATAL;
		fprintf(stderr, "sp->cdb_len > SRBlock.u.cmd.cdb_st. Fatal error in scsi_send, exiting...\n");
		return (-1);
	}

	/* clear command block */
	fillbytes((caddr_t)&SRBlock.u.cmd.cdb_st, sizeof(SRBlock.u.cmd.cdb_st), '\0');
	/* copy cdrecord command into SRB */
	movebytes(&sp->cdb, &SRBlock.u.cmd.cdb_st, sp->cdb_len);

	/* execute SCSI		command */
	SRBlock.cmd = SRB_Command;
	SRBlock.ha_num = scgp->scsibus;		/* host adapter number */
	SRBlock.flags = 0;

	SRBlock.flags = SRB_Post;		/* flags */

	SRBlock.u.cmd.target	= scgp->target;	/* Target SCSI ID */
	SRBlock.u.cmd.lun	= scgp->lun;	/* Target SCSI LUN */
	SRBlock.u.cmd.data_len	= sp->size;	/* # of bytes transferred */
	SRBlock.u.cmd.data_ptr	= 0;		/* pointer to data buffer */
	SRBlock.u.cmd.sense_len	= sp->sense_len;/* length of sense buffer */

	SRBlock.u.cmd.link_ptr	= 0;		/* pointer to next SRB */
	SRBlock.u.cmd.cdb_len	= sp->cdb_len;	/* SCSI command length */

	if (sp->flags & SCG_RECV_DATA) {
		SRBlock.flags |= SRB_Read;
	} else {
		if (sp->size > 0) {
			SRBlock.flags |= SRB_Write;
			if (scgp->bufbase != sp->addr) {/* Copy only if data not in ASPI-Mem */
				movebytes(sp->addr, scgp->bufbase, sp->size);
			} else {
/*				SRBlock.flags |= SRB_NoTransfer;*/
			}

#ifdef DEBUG2
			printf("data_len=0d%d, sp->size=0d%d, sp->addr0x%x, buffer0x%x\n",
						SRBlock.u.cmd.data_len, sp->size, sp->addr, scgp->bufbase);
			printf("Input cdb[](0x): %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
				SRBlock.u.cmd.cdb_st[0], SRBlock.u.cmd.cdb_st[1],
				SRBlock.u.cmd.cdb_st[2], SRBlock.u.cmd.cdb_st[3], SRBlock.u.cmd.cdb_st[4], SRBlock.u.cmd.cdb_st[5],
				SRBlock.u.cmd.cdb_st[6], SRBlock.u.cmd.cdb_st[7], SRBlock.u.cmd.cdb_st[8], SRBlock.u.cmd.cdb_st[9],
				SRBlock.u.cmd.cdb_st[10], SRBlock.u.cmd.cdb_st[11], SRBlock.u.cmd.cdb_st[12], SRBlock.u.cmd.cdb_st[13],
				SRBlock.u.cmd.cdb_st[14], SRBlock.u.cmd.cdb_st[15], SRBlock.u.cmd.cdb_st[16], SRBlock.u.cmd.cdb_st[17],
				SRBlock.u.cmd.cdb_st[18], SRBlock.u.cmd.cdb_st[19], SRBlock.u.cmd.cdb_st[20], SRBlock.u.cmd.cdb_st[21]);
#endif
#ifdef	XXX
				if (fileHandle)
					write(fileHandle, sp->addr, sp->size);
#endif
		}
	}
	sp->error	= SCG_NO_ERROR;
	sp->sense_count	= 0;
	sp->u_scb.cmd_scb[0] = 0;
	sp->resid	= 0;
#ifdef DEBUG2
	printf("Input cdb[](0x): %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			 SRBlock.u.cmd.cdb_st[0], SRBlock.u.cmd.cdb_st[1],
			 SRBlock.u.cmd.cdb_st[2], SRBlock.u.cmd.cdb_st[3], SRBlock.u.cmd.cdb_st[4], SRBlock.u.cmd.cdb_st[5],
			 SRBlock.u.cmd.cdb_st[6], SRBlock.u.cmd.cdb_st[7], SRBlock.u.cmd.cdb_st[8], SRBlock.u.cmd.cdb_st[9],
			 SRBlock.u.cmd.cdb_st[10], SRBlock.u.cmd.cdb_st[11], SRBlock.u.cmd.cdb_st[12], SRBlock.u.cmd.cdb_st[13],
			 SRBlock.u.cmd.cdb_st[14], SRBlock.u.cmd.cdb_st[15], SRBlock.u.cmd.cdb_st[16], SRBlock.u.cmd.cdb_st[17],
			 SRBlock.u.cmd.cdb_st[18], SRBlock.u.cmd.cdb_st[19], SRBlock.u.cmd.cdb_st[20], SRBlock.u.cmd.cdb_st[21]);
	printf("data_len=0x%x, sense_len=0x%x, cdb_len=0x%x\n",
			 SRBlock.u.cmd.data_len, SRBlock.u.cmd.sense_len, SRBlock.u.cmd.cdb_len);
	ptr = (UCHAR*)&sp->cdb;
	printf("buffer(0x): %x %x %x %x %x %x, addr=%x\n",
			 *(ptr), *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4),
			 *(ptr+5), sp->addr);
#endif
	done++;

	rc = DosDevIOCtl(driver_handle, 0x92, 0x02,
			 (void*) &SRBlock, sizeof(SRB), &cbParam,
			 (void*) &SRBlock, sizeof(SRB), &cbreturn);

	if (rc) {
		fprintf(stderr, "DosDevIOCtl() in sendCommand failed.\n");
		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;	/* Später vielleicht errno einsetzen */
		return (rc);
	} else {
		rc = wait_post(sp->timeout*1000);
		if (rc) {
			if (rc == 640) {
				sp->error = SCG_TIMEOUT;
				fprintf(stderr, "Timeout during SCSI-Command.\n");
				return (1);
			}
			sp->error = SCG_FATAL;
			fprintf(stderr, "Fatal Error during DosWaitEventSem().\n");
			return (1);
		}
#ifdef DEBUG2
		printf("wait_post() returned rc=%x\n", rc);
		printf("Done cdb[](0x): %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			 SRBlock.u.cmd.cdb_st[0], SRBlock.u.cmd.cdb_st[1],
			 SRBlock.u.cmd.cdb_st[2], SRBlock.u.cmd.cdb_st[3], SRBlock.u.cmd.cdb_st[4], SRBlock.u.cmd.cdb_st[5],
			 SRBlock.u.cmd.cdb_st[6], SRBlock.u.cmd.cdb_st[7], SRBlock.u.cmd.cdb_st[8], SRBlock.u.cmd.cdb_st[9],
			 SRBlock.u.cmd.cdb_st[10], SRBlock.u.cmd.cdb_st[11], SRBlock.u.cmd.cdb_st[12], SRBlock.u.cmd.cdb_st[13],
			 SRBlock.u.cmd.cdb_st[14], SRBlock.u.cmd.cdb_st[15], SRBlock.u.cmd.cdb_st[16], SRBlock.u.cmd.cdb_st[17],
			 SRBlock.u.cmd.cdb_st[18], SRBlock.u.cmd.cdb_st[19], SRBlock.u.cmd.cdb_st[20], SRBlock.u.cmd.cdb_st[21],
			 SRBlock.u.cmd.cdb_st[22], SRBlock.u.cmd.cdb_st[23], SRBlock.u.cmd.cdb_st[24], SRBlock.u.cmd.cdb_st[25],
			 SRBlock.u.cmd.cdb_st[26], SRBlock.u.cmd.cdb_st[27]);
		printf("data_len=0x%x, sense_len=0x%x, cdb_len=0x%x, SRBlock.status=0x%x, size=0x%x, target_status=0x%x\n",
			 SRBlock.u.cmd.data_len, SRBlock.u.cmd.sense_len, SRBlock.u.cmd.cdb_len, SRBlock.status,
			 sp->size, SRBlock.u.cmd.target_status);
		ptr = (UCHAR*)scgp->bufbase;
		printf("buffer(0x): %x %x %x %x %x %x, buffer=%x\n",
			 *(ptr), *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4),
			 *(ptr+5), scgp->bufbase);
#endif
		if (SRBlock.status == SRB_Done) {	/* succesful completion */
#ifdef DEBUG
			printf("Command successful finished. SRBlock.status=0x%x\n\n", SRBlock.status);
#endif
			sp->sense_count = 0;
			sp->resid = 0;
			if (sp->flags & SCG_RECV_DATA) {
				if (sp->addr && sp->size) {
					if (scgp->bufbase != sp->addr)	/* Copy only if data not in ASPI-Mem */
						movebytes(scgp->bufbase, sp->addr, SRBlock.u.cmd.data_len);
					ptr = (UCHAR*)sp->addr;
					sp->resid = sp->size - SRBlock.u.cmd.data_len;/*nicht übertragene bytes. Korrekt berechnet???*/
				}
			}	/* end of if (sp->flags & SCG_RECV_DATA) */
			if (SRBlock.u.cmd.target_status == SRB_CheckStatus) { /* Sense data valid */
				sp->sense_count	= (int)SRBlock.u.cmd.sense_len;
				if (sp->sense_count > sp->sense_len)
					sp->sense_count = sp->sense_len;

				ptr = (UCHAR*)&SRBlock.u.cmd.cdb_st;
				ptr += SRBlock.u.cmd.cdb_len;

				fillbytes(&sp->u_sense.Sense, sizeof(sp->u_sense.Sense), '\0');
				movebytes(ptr, &sp->u_sense.Sense, sp->sense_len);

				sp->u_scb.cmd_scb[0] = SRBlock.u.cmd.target_status;
				sp->ux_errno = EIO;	/* Später differenzieren? */
			}
			return (0);
		}
		/* SCSI-Error occured */
		sp->error = SCG_RETRYABLE;
#ifdef DEBUG2
		printf("Done cdb[](0x): %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			 SRBlock.u.cmd.cdb_st[0], SRBlock.u.cmd.cdb_st[1],
			 SRBlock.u.cmd.cdb_st[2], SRBlock.u.cmd.cdb_st[3], SRBlock.u.cmd.cdb_st[4], SRBlock.u.cmd.cdb_st[5],
			 SRBlock.u.cmd.cdb_st[6], SRBlock.u.cmd.cdb_st[7], SRBlock.u.cmd.cdb_st[8], SRBlock.u.cmd.cdb_st[9],
			 SRBlock.u.cmd.cdb_st[10], SRBlock.u.cmd.cdb_st[11], SRBlock.u.cmd.cdb_st[12], SRBlock.u.cmd.cdb_st[13],
			 SRBlock.u.cmd.cdb_st[14], SRBlock.u.cmd.cdb_st[15], SRBlock.u.cmd.cdb_st[16], SRBlock.u.cmd.cdb_st[17],
			 SRBlock.u.cmd.cdb_st[18], SRBlock.u.cmd.cdb_st[19], SRBlock.u.cmd.cdb_st[20], SRBlock.u.cmd.cdb_st[21],
			 SRBlock.u.cmd.cdb_st[22], SRBlock.u.cmd.cdb_st[23], SRBlock.u.cmd.cdb_st[24], SRBlock.u.cmd.cdb_st[25],
			 SRBlock.u.cmd.cdb_st[26], SRBlock.u.cmd.cdb_st[27]);
		printf("data_len=0x%x, cdb_len=0x%x, SRBlock.status=0x%x, flags=0x%x, ha_status=0x%x, target_status=0x%x\n",
			 SRBlock.u.cmd.data_len, SRBlock.u.cmd.cdb_len, SRBlock.status,
			 sp->flags, SRBlock.u.cmd.ha_status, SRBlock.u.cmd.target_status);
		ptr = (UCHAR*)scgp->bufbase;
		printf("buffer(0x): %x %x %x %x %x %x, buffer=%x\n",
			 *(ptr), *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4),
			 *(ptr+5), scgp->bufbase);
#endif
		if (SRBlock.u.cmd.target_status == SRB_CheckStatus) {	/* Sense data valid */
			sp->sense_count	= (int)SRBlock.u.cmd.sense_len;
			if (sp->sense_count > sp->sense_len)
				sp->sense_count = sp->sense_len;

			ptr = (UCHAR*)&SRBlock.u.cmd.cdb_st;
			ptr += SRBlock.u.cmd.cdb_len;

			fillbytes(&sp->u_sense.Sense, sizeof(sp->u_sense.Sense), '\0');
			movebytes(ptr, &sp->u_sense.Sense, sp->sense_len);

			sp->u_scb.cmd_scb[0] = SRBlock.u.cmd.target_status;
			sp->ux_errno = EIO;
		}
		if (sp->flags & SCG_RECV_DATA) {
			if (sp->addr && sp->size) {
				if (scgp->bufbase != sp->addr)	/* Copy only if data not in ASPI-Mem */
					movebytes(scgp->bufbase, sp->addr, SRBlock.u.cmd.data_len);
			}
		}
#ifdef	really
		sp->resid	= SRBlock.u.cmd.data_len;/* XXXXX Got no Data ????? */
#else
		sp->resid	= sp->size - SRBlock.u.cmd.data_len;
#endif
		return (0);
	}
}

/***************************************************************************
 *                                                                         *
 *  BOOL open_driver()                                                     *
 *                                                                         *
 *  Opens the ASPI Router device driver and sets device_handle.            *
 *  Returns:                                                               *
 *    TRUE - Success                                                       *
 *    FALSE - Unsuccessful opening of device driver                        *
 *                                                                         *
 *  Preconditions: ASPI Router driver has be loaded                        *
 *                                                                         *
 ***************************************************************************/
LOCAL BOOL
open_driver()
{
	ULONG	rc;			/* return value */
	ULONG	ActionTaken;		/* return value	*/
	USHORT	openSemaReturn;		/* return value	*/
	Ulong	cbreturn;
	Ulong	cbParam;

	if (driver_handle)		/* ASPI-Router already opened	*/
		return (TRUE);

	rc = DosOpen((PSZ) "aspirou$",	/* open driver*/
			&driver_handle,
			&ActionTaken,
			0,
			0,
			FILE_OPEN,
			OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
			NULL);
	if (rc) {
		fprintf(stderr, "Cannot open ASPI-Router!\n");
		return (FALSE);		/* opening failed -> return false*/
	}

	/* Init semaphore */
	if (DosCreateEventSem(NULL, &postSema,	/* create event semaphore */
				 DC_SEM_SHARED, 0)) {
		DosClose(driver_handle);
		fprintf(stderr, "Cannot create event semaphore!\n");
		return (FALSE);
	}
	rc = DosDevIOCtl(driver_handle, 0x92, 0x03,	/* pass semaphore handle */
			(void*) &postSema, sizeof(HEV),	/* to driver		 */
			&cbParam, (void*) &openSemaReturn,
			sizeof(USHORT), &cbreturn);

	if (rc||openSemaReturn) {			/* Error */
		DosCloseEventSem(postSema);
		DosClose(driver_handle);
		return (FALSE);
	}
	return (TRUE);
}

/***************************************************************************
 *                                                                         *
 *  BOOL close_driver()                                                    *
 *                                                                         *
 *  Closes the device driver                                               *
 *  Returns:                                                               *
 *    TRUE - Success                                                       *
 *    FALSE - Unsuccessful closing of device driver                        *
 *                                                                         *
 *  Preconditions: ASPI Router driver has be opened with open_driver       *
 *                                                                         *
 ***************************************************************************/
LOCAL BOOL
close_driver()
{
	ULONG rc;				/* return value */

	if (driver_handle) {
		rc = DosClose(driver_handle);
		if (rc)
			return (FALSE);		/* closing failed -> return false */
		driver_handle = 0;
		if (DosCloseEventSem(postSema))
			fprintf(stderr, "Cannot close event semaphore!\n");
		if (buffer && DosFreeMem(buffer))
			fprintf(stderr, "Cannot free buffer memory for ASPI-Router!\n"); /* Free our memory buffer if not already done */
		buffer = NULL;
	}
	return (TRUE);
}


#ifdef	SCANDEBUG
/***************************************************************************/
/*                                                                         */
/*  ULONG test_unit_ready_aspi(UCHAR busno, UCHAR tgt, UCHAR tlun)         */
/*                                                                         */
/*  Sends a SRB containing a test unit ready command                       */
/*  Returns:                                                               */
/*    0  - Success                                                         */
/*    1  - DevIOCtl failed                                                 */
/*    2  - Semaphore access failure                                        */
/*    3  - SCSI command failed                                             */
/*                                                                         */
/*  Preconditions: init() has to be called successfully before             */
/*                                                                         */
/***************************************************************************/
ULONG test_unit_ready_aspi( UCHAR busno, UCHAR tgt, UCHAR tlun)
{
	ULONG	rc;					/* return value */
	BOOL 	success;				/* return value */
	Ulong	cbreturn;
	Ulong	cbParam;
	SRB	SRBlock;
  
	SRBlock.cmd = SRB_Command;			/* execute SCSI cmd */
	SRBlock.ha_num = busno;				/* host adapter number */
	SRBlock.flags = SRB_NoTransfer | SRB_Post;	/* no data transfer, posting enabled */
	SRBlock.u.cmd.target = tgt;			/* Target SCSI ID */
	SRBlock.u.cmd.lun = tlun;			/* Target SCSI LUN */
	SRBlock.u.cmd.data_len = 0;			/* # of bytes transferred */
	SRBlock.u.cmd.sense_len = 32;			/* length of sense buffer */
	SRBlock.u.cmd.data_ptr = 0;			/* pointer to data buffer */
	SRBlock.u.cmd.link_ptr = 0;			/* pointer to next SRB */
	SRBlock.u.cmd.cdb_len = 6;			/* SCSI command length */
	SRBlock.u.cmd.cdb_st[0] = 0x0;			/* test unit ready command */
	SRBlock.u.cmd.cdb_st[1] = (tlun << 5);		/* lun */
	SRBlock.u.cmd.cdb_st[2] = 0;
	SRBlock.u.cmd.cdb_st[3] = 0;
	SRBlock.u.cmd.cdb_st[4] = 0;
	SRBlock.u.cmd.cdb_st[5] = 0;

	rc = DosDevIOCtl(driver_handle, 0x92, 0x02, (void*) &SRBlock, sizeof(SRB), &cbParam,
		   (void*) &SRBlock, sizeof(SRB), &cbreturn);
	if (rc) {
		return 1;				/* DosDevIOCtl failed */
	} else {
		success = wait_post(40000);		/* wait for SRB being processed */
		if (success) return 2;			/* semaphore could not be accessed */
	}
	if (SRBlock.status != SRB_Done) return 3;
	if (SRBlock.u.cmd.ha_status != SRB_NoError) return 3;
	if (SRBlock.u.cmd.target_status != SRB_NoStatus) return 3;

	return (0);
}
#endif	/* SCANDEBUG */

LOCAL ULONG
wait_post(ULONG ulTimeOut)
{
	ULONG count = 0;
	ULONG rc;					/* return value */

/*	rc = DosWaitEventSem(postSema, -1);*/		/* wait forever*/
	rc = DosWaitEventSem(postSema, ulTimeOut);
	if (!rc)
		DosResetEventSem(postSema, &count);	/* ?????????? */
	return (rc);
}

LOCAL BOOL 
init_buffer(mem)
	void	*mem;
{
	ULONG	rc;					/* return value */
	USHORT lockSegmentReturn;			/* return value */
	Ulong	cbreturn;
	Ulong	cbParam;

	rc = DosDevIOCtl(driver_handle, 0x92, 0x04,	/* pass buffers pointer */
			(void*) mem, sizeof(void*),	/* to driver */
			&cbParam, (void*) &lockSegmentReturn,
			sizeof(USHORT), &cbreturn);
	if (rc)
		return (FALSE);				/* DosDevIOCtl failed */
	if (lockSegmentReturn)
		return (FALSE);				/* Driver could not lock segment */
	return (TRUE);
}
#define	sense	u_sense.Sense
