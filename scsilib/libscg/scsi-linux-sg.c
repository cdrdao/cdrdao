/* @(#)scsi-linux-sg.c	1.37 99/09/07 Copyright 1997 J. Schilling */
#ifndef lint
static	char __sccsid[] =
	"@(#)scsi-linux-sg.c	1.37 99/09/07 Copyright 1997 J. Schilling";
#endif
/*
 *	Interface for Linux generic SCSI implementation (sg).
 *
 *	This is the interface for the broken Linux SCSI generic driver.
 *	This is a hack, that tries to emulate the functionality
 *	of the scg driver.
 *
 *	Design flaws of the sg driver:
 *	-	cannot see if SCSI command could not be send
 *	-	cannot get SCSI status byte
 *	-	cannot get real dma count of tranfer
 *	-	cannot get number of bytes valid in auto sense data
 *	-	to few data in auto sense (CCS/SCSI-2/SCSI-3 needs >= 18)
 *
 *	This code contains support for the sg driver version 2
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

#include <linux/version.h>

#ifndef LINUX_VERSION_CODE	/* Very old kernel? */
#	define LINUX_VERSION_CODE 0
#endif

#if LINUX_VERSION_CODE >= 0x01031a /* <linux/scsi.h> introduced in 1.3.26 */
#if LINUX_VERSION_CODE >= 0x020000 /* <scsi/scsi.h> introduced somewhere. */
/* Need to fine tune the ifdef so we get the transition point right. */
#include <scsi/scsi.h>
#else
#include <linux/scsi.h>
#endif
#else
#define __KERNEL__
#include <linux/fs.h>
#undef __KERNEL__
#include "block/blk.h"
#include "scsi/scsi.h"
#endif

#include "scsi/sg.h"

#ifndef	SCSI_IOCTL_GET_BUS_NUMBER
#define SCSI_IOCTL_GET_BUS_NUMBER 0x5386
#endif

/*
 * XXX There must be a better way than duplicating things from system include
 * XXX files. This is stolen from /usr/src/linux/drivers/scsi/scsi.h
 */
#ifndef	DID_OK
#define DID_OK          0x00 /* NO error                                */
#define DID_NO_CONNECT  0x01 /* Couldn't connect before timeout period  */
#define DID_BUS_BUSY    0x02 /* BUS stayed busy through time out period */
#define DID_TIME_OUT    0x03 /* TIMED OUT for other reason              */
#define DID_BAD_TARGET  0x04 /* BAD target.                             */
#define DID_ABORT       0x05 /* Told to abort for some other reason     */
#define DID_PARITY      0x06 /* Parity error                            */
#define DID_ERROR       0x07 /* Internal error                          */
#define DID_RESET       0x08 /* Reset by somebody.                      */
#define DID_BAD_INTR    0x09 /* Got an interrupt we weren't expecting.  */ 
#endif

/*
 *  These indicate the error that occurred, and what is available.
 */
#ifndef DRIVER_BUSY
#define DRIVER_BUSY         0x01
#define DRIVER_SOFT         0x02
#define DRIVER_MEDIA        0x03
#define DRIVER_ERROR        0x04
 
#define DRIVER_INVALID      0x05
#define DRIVER_TIMEOUT      0x06
#define DRIVER_HARD         0x07
#define DRIVER_SENSE        0x08
#endif

/*
 * XXX Should add extra space in buscookies and scgfiles for a "PP bus"
 * XXX and for two "ATAPI busses".
 */
#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct scg_local {
	int	scgfile;		/* Used for SG_GET_BUFSIZE ioctl()*/
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
	short	buscookies[MAX_SCG];
	int	pgbus;
	int	pack_id;		/* Should be a random number	*/
	char	*SCSIbuf;
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

#ifdef	SG_BIG_BUFF
#define	MAX_DMA_LINUX	SG_BIG_BUFF	/* Defined in include/scsi/sg.h	*/
#else
#define	MAX_DMA_LINUX	(4*1024)	/* Old Linux versions		*/
#endif

#ifndef	SG_MAX_SENSE
#	define	SG_MAX_SENSE	16	/* Too small for CCS / SCSI-2	*/
#endif					/* But cannot be changed	*/

#if	!defined(__i386) && !defined(i386) && !defined(mc68000)
#define	MISALIGN
#endif
/*#define	MISALIGN*/
/*#undef	SG_GET_BUFSIZE*/

#if	defined(USE_PG) && !defined(USE_PG_ONLY)
#include "scsi-linux-pg.c"
#endif

#ifdef	MISALIGN
LOCAL	int	scsi_getint	__PR((int *ip));
#endif
LOCAL	int	scsi_send	__PR((SCSI *scgp, int f, struct scg_cmd *sp));
LOCAL	BOOL	sg_setup	__PR((SCSI *scgp, int f, int busno, int tgt, int tlun));
LOCAL	void	sg_initdev	__PR((SCSI *scgp, int f));
LOCAL	int	sg_mapbus	__PR((SCSI *scgp, int busno, int ino));
LOCAL	BOOL	sg_mapdev	__PR((SCSI *scgp, int f, int *busp, int *tgtp, int *lunp,
							int *chanp, int *inop));
LOCAL	void	sg_settimeout	__PR((int f, int timeout));

EXPORT int
scsi_open(scgp, device, busno, tgt, tlun)
	SCSI	*scgp;
	char	*device;
	int	busno;
	int	tgt;
	int	tlun;
{
	register int	f;
	register int	i;
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

	if (scgp->local == NULL) {
		scgp->local = malloc(sizeof(struct scg_local));
		if (scgp->local == NULL)
			return (0);

		scglocal(scgp)->scgfile = -1;
		scglocal(scgp)->pgbus = -2;
		scglocal(scgp)->SCSIbuf = (char *)-1;
		scglocal(scgp)->pack_id = 5;

		for (b=0; b < MAX_SCG; b++) {
			scglocal(scgp)->buscookies[b] = (short)-1;
			for (t=0; t < MAX_TGT; t++) {
				for (l=0; l < MAX_LUN ; l++)
					scglocal(scgp)->scgfiles[b][t][l] = (short)-1;
			}
		}
	}

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2))
		goto openbydev;

	for (i=0; i < 32; i++) {
		sprintf(devname, "/dev/sg%d", i);
		f = open(devname, 2);
		if (f < 0) {
			if (errno != EACCES && errno != ENOENT && errno != ENXIO && errno != ENODEV) {
				if (scgp->errstr)
					js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
							"Cannot open '%s'", devname);
				return (0);
			}
		} else {
			if (sg_setup(scgp, f, busno, tgt, tlun))
				return (++nopen);
			if (busno < 0 && tgt < 0 && tlun < 0)
				nopen++;
		}
	}
	if (nopen == 0) for (i=0; i <= 25; i++) {
		sprintf(devname, "/dev/sg%c", i+'a');
		f = open(devname, 2);
		if (f < 0) {
			if (errno != ENOENT && errno != ENXIO && errno != ENODEV) {
				if (scgp->errstr)
					js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
						"Cannot open '%s'", devname);
				return (0);
			}
		} else {
			if (sg_setup(scgp, f, busno, tgt, tlun))
				return (++nopen);
			if (busno < 0 && tgt < 0 && tlun < 0)
				nopen++;
		}
	}
openbydev:
	if (device != NULL && *device != '\0') {
		f = open(device, 2);
		if (f < 0 && errno == ENOENT)
			goto openpg;

		if (!sg_mapdev(scgp, f, &busno, &tgt, &tlun, 0, 0)) {
			close(f);
			goto openpg;
		}

		if (scgp->scsibus < 0)
			scgp->scsibus = busno;
		if (scgp->target < 0)
			scgp->target = tgt;
		if (scgp->lun < 0)
			scgp->lun = tlun;

		if (sg_setup(scgp, f, busno, tgt, tlun))
			return (++nopen);
	}
openpg:
#ifdef	USE_PG
	nopen += pg_open(scgp, device, busno, tgt, tlun);
#endif
	if (scgp->debug) for (b=0; b < MAX_SCG; b++) {
		printf("Bus: %d cookie: %X\n", b, scglocal(scgp)->buscookies[b]);
		for (t=0; t < MAX_TGT; t++) {
			for (l=0; l < MAX_LUN ; l++)
				if (scglocal(scgp)->scgfiles[b][t][l] != (short)-1)
					printf("file (%d,%d,%d): %d\n",
						b, t, l, scglocal(scgp)->scgfiles[b][t][l]);
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
		if (b == scglocal(scgp)->pgbus)
			continue;
		scglocal(scgp)->buscookies[b] = (short)-1;
		for (t=0; t < MAX_TGT; t++) {
			for (l=0; l < MAX_LUN ; l++) {
				f = scglocal(scgp)->scgfiles[b][t][l];
				if (f >= 0)
					close(f);
				scglocal(scgp)->scgfiles[b][t][l] = (short)-1;
			}
		}
	}
#ifdef	USE_PG
	pg_close(scgp);
#endif
	return (0);
}

LOCAL BOOL
sg_setup(scgp, f, busno, tgt, tlun)
	SCSI	*scgp;
	int	f;
	int	busno;
	int	tgt;
	int	tlun;
{
	int	n;
	int	Chan;
	int	Ino;
	int	Bus;
	int	Target;
	int	Lun;
	BOOL	onetarget = FALSE;


	if (scgp->scsibus >= 0 && scgp->target >= 0 && scgp->lun >= 0)
		onetarget = TRUE;

	sg_mapdev(scgp, f, &Bus, &Target, &Lun, &Chan, &Ino);

	/*
	 * For old kernels try to make the best guess.
	 */
	Ino |= Chan << 8;
	n = sg_mapbus(scgp, Bus, Ino);
	if (Bus == -1) {
		Bus = n;
		if (scgp->debug)
			printf("SCSI Bus: %d (mapped from %d)\n", Bus, Ino);
	}

	if (Bus < 0 || Bus >= MAX_SCG || Target < 0 || Target >= MAX_TGT ||
						Lun < 0 || Lun >= MAX_LUN) {
		return (FALSE);
	}

	if (scglocal(scgp)->scgfiles[Bus][Target][Lun] == (short)-1)
		scglocal(scgp)->scgfiles[Bus][Target][Lun] = (short)f;

	if (onetarget) {
		if (Bus == busno && Target == tgt && Lun == tlun) {
			sg_initdev(scgp, f);
			scglocal(scgp)->scgfile = f;	/* remember file for ioctl's */
			return (TRUE);
		} else {
			scglocal(scgp)->scgfiles[Bus][Target][Lun] = (short)-1;
			close(f);
		}
	} else {
		sg_initdev(scgp, f);
		if (scglocal(scgp)->scgfile < 0)
			scglocal(scgp)->scgfile = f;	/* remember file for ioctl's */
	}
	return (FALSE);
}

LOCAL void
sg_initdev(scgp, f)
	SCSI	*scgp;
	int	f;
{
	struct sg_rep {
		struct sg_header	hd;
		unsigned char		rbuf[100];
	} sg_rep;
	int	n;

	/* Eat any unwanted garbage from prior use of this device */

	n = fcntl(f, F_GETFL);	/* Be very proper about this */
	fcntl(f, F_SETFL, n|O_NONBLOCK);

	fillbytes((caddr_t)&sg_rep, sizeof(struct sg_header), '\0');
	sg_rep.hd.reply_len = sizeof(struct sg_header);

	while (read(f, &sg_rep, sizeof(sg_rep)) >= 0 || errno != EAGAIN)
		;

	fcntl(f, F_SETFL, n);

	sg_settimeout(f, scgp->deftimeout);
}

LOCAL int
sg_mapbus(scgp, busno, ino)
	SCSI	*scgp;
	int	busno;
	int	ino;
{
	register int	i;

	if (busno >= 0 && busno < MAX_SCG) {
		/*
		 * SCSI_IOCTL_GET_BUS_NUMBER worked.
		 * Now we have the problem that Linux does not properly number
		 * SCSI busses. The Bus number that Linux creates really is
		 * the controller (card) number. I case of multi SCSI bus
		 * cards we are lost.
		 */
		if (scglocal(scgp)->buscookies[busno] == (short)-1) {
			scglocal(scgp)->buscookies[busno] = ino;
			return (busno);
		}
		if (scglocal(scgp)->buscookies[busno] != (short)ino)
			errmsgno(EX_BAD, "Warning Linux Bus mapping botch.\n");
		return (busno);

	} else for (i=0; i < MAX_SCG; i++) {
		if (scglocal(scgp)->buscookies[i] == (short)-1) {
			scglocal(scgp)->buscookies[i] = ino;
			return (i);
		}

		if (scglocal(scgp)->buscookies[i] == ino)
			return (i);
	}
	return (0);
}

LOCAL BOOL
sg_mapdev(scgp, f, busp, tgtp, lunp, chanp, inop)
	SCSI	*scgp;
	int	f;
	int	*busp;
	int	*tgtp;
	int	*lunp;
	int	*chanp;
	int	*inop;
{
	struct	sg_id {
		long	l1; /* target | lun << 8 | channel << 16 | low_ino << 24 */
		long	l2; /* Unique id */
	} sg_id;
	int	Chan;
	int	Ino;
	int	Bus;
	int	Target;
	int	Lun;

	if (ioctl(f, SCSI_IOCTL_GET_IDLUN, &sg_id))
		return (FALSE);
	if (scgp->debug)
		printf("l1: 0x%lX l2: 0x%lX\n", sg_id.l1, sg_id.l2);
	if (ioctl(f, SCSI_IOCTL_GET_BUS_NUMBER, &Bus) < 0) {
		Bus = -1;
	}

	Target	= sg_id.l1 & 0xFF;
	Lun	= (sg_id.l1 >> 8) & 0xFF;
	Chan	= (sg_id.l1 >> 16) & 0xFF;
	Ino	= (sg_id.l1 >> 24) & 0xFF;
	if (scgp->debug) {
		printf("Bus: %d Target: %d Lun: %d Chan: %d Ino: %d\n",
				Bus, Target, Lun, Chan, Ino);
	}
	*busp = Bus;
	*tgtp = Target;
	*lunp = Lun;
	if (chanp)
		*chanp = Chan;
	if (inop)
		*inop = Ino;
	return (TRUE);
}

LOCAL long
scsi_maxdma(scgp)
	SCSI	*scgp;
{
	long maxdma = MAX_DMA_LINUX;

#ifdef	SG_GET_BUFSIZE
	/*
	 * We assume that all /dev/sg instances use the same
	 * maximum buffer size.
	 */
	if ((maxdma = ioctl(scglocal(scgp)->scgfile, SG_GET_BUFSIZE, 0)) < 0) {
		if (scglocal(scgp)->scgfile >= 0) {
			maxdma = MAX_DMA_LINUX;
		}
	}
#endif
#ifdef	USE_PG
	if (scgp->scsibus == scglocal(scgp)->pgbus)
		return (pg_maxdma(scgp));
	if ((scgp->scsibus < 0) && (pg_maxdma(scgp) < maxdma))
		return (pg_maxdma(scgp));
#endif
	return (maxdma);
}

EXPORT void *
scsi_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	char	*ret;

	if (amt <= 0 || amt > scsi_maxdma(scgp))
		return ((void *)0);
	if (scgp->debug)
		printf("scsi_getbuf: %ld bytes\n", amt);
	/*
	 * For performance reason, we allocate pagesize()
	 * bytes before the SCSI buffer to avoid
	 * copying the whole buffer contents when
	 * setting up the /dev/sg data structures.
	 */
	ret = valloc((size_t)(amt+getpagesize()));
	if (ret == NULL)
		return (ret);
	scgp->bufbase = ret;
	ret += getpagesize();
	scglocal(scgp)->SCSIbuf = ret;
	return ((void *)ret);
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
#ifdef	USE_PG
	if (scgp->scsibus == scglocal(scgp)->pgbus)
		return (pg_initiator_id(scgp));
#endif
	return (-1);
}

EXPORT
int scsi_isatapi(scgp)
	SCSI	*scgp;
{
#ifdef	USE_PG
	if (scgp->scsibus == scglocal(scgp)->pgbus)
		return (pg_isatapi(scgp));
#endif

#ifdef	SG_EMULATED_HOST
	{
	int	emulated = FALSE;
	int	f = scsi_fileno(scgp, scgp->scsibus, scgp->target, scgp->lun);

	if (ioctl(f, SG_EMULATED_HOST, &emulated) >= 0)
		return (emulated != 0);
	}
#endif
	return (-1);
}

EXPORT
int scsireset(scgp)
	SCSI	*scgp;
{
#ifdef	USE_PG
	if (scgp->scsibus == scglocal(scgp)->pgbus)
		return (pg_reset(scgp));
#endif
	/*
	 * Do we have a SCSI reset in the Linux sg driver?
	 */
	return (-1);
}

LOCAL void
sg_settimeout(f, tmo)
	int	f;
	int	tmo;
{
	tmo *= HZ;
	if (tmo)
		tmo += HZ/2;

	if (ioctl(f, SG_SET_TIMEOUT, &tmo) < 0)
		comerr("Cannot set SG_SET_TIMEOUT.\n");
}

/*
 * Get misaligned int.
 * Needed for all recent processors (sparc/ppc/alpha)
 * because the /dev/sg design forces us to do misaligned
 * reads of integers.
 */
#ifdef	MISALIGN
LOCAL int
scsi_getint(ip)
	int	*ip;
{
		 int	ret;
	register char	*cp = (char *)ip;
	register char	*tp = (char *)&ret;
	register int	i;

	for (i = sizeof(int); --i >= 0; )
		*tp++ = *cp++;
	
	return (ret);
}
#define	GETINT(a)	scsi_getint(&(a))
#else
#define	GETINT(a)	(a)
#endif

LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	struct sg_rq	*sgp;
	struct sg_rq	*sgp2;
	int	i;
	int	pack_len;
	int	reply_len;
	int	amt = sp->cdb_len;
	struct sg_rq {
		struct sg_header	hd;
		unsigned char		buf[MAX_DMA_LINUX+SCG_MAX_CMD];
	} sg_rq;
#ifdef	SG_GET_BUFSIZE		/* We may use a 'sg' version 2 driver	*/
	char	driver_byte;
	char	host_byte;
	char	msg_byte;
	char	status_byte;
#endif

	if (f < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}
#ifdef	USE_PG
	if (scgp->scsibus == scglocal(scgp)->pgbus)
		return (pg_send(scgp, f, sp));
#endif
	if (sp->timeout != scgp->deftimeout)
		sg_settimeout(f, sp->timeout);


	sgp2 = sgp = &sg_rq;
	if (sp->addr == scglocal(scgp)->SCSIbuf) {
		sgp = (struct sg_rq *)
			(scglocal(scgp)->SCSIbuf - (sizeof(struct sg_header) + amt));
		sgp2 = (struct sg_rq *)
			(scglocal(scgp)->SCSIbuf - (sizeof(struct sg_header)));
	} else {
		if (scgp->debug) {
			printf("DMA addr: 0x%8.8lX size: %d - using copy buffer\n",
				(long)sp->addr, sp->size);
		}
		if (sp->size > (int)(sizeof(sg_rq.buf) - SCG_MAX_CMD)) {
			errno = ENOMEM;
			return (-1);
		}
	}

	/*
	 * This is done to avoid misaligned access of sgp->some_int
	 */
	pack_len = sizeof(struct sg_header) + amt;
	reply_len = sizeof(struct sg_header);
	if (sp->flags & SCG_RECV_DATA) {
		reply_len += sp->size;
	} else {
		pack_len += sp->size;
	}

#ifdef	MISALIGN
	/*
	 * sgp->some_int may be misaligned if (sp->addr == SCSIbuf)
	 * This is no problem on Intel porocessors, however
	 * all other processors don't like it.
	 * sizeof(struct sg_header) + amt is usually not a multiple of
	 * sizeof(int). For this reason, we fill in the values into sg_rq
	 * which is always corectly aligned and then copy it to the real
	 * location if this location differs from sg_rq.
	 * Never read/write directly to sgp->some_int !!!!!
	 */
	fillbytes((caddr_t)&sg_rq, sizeof(struct sg_header), '\0');

	sg_rq.hd.pack_len = pack_len;
	sg_rq.hd.reply_len = reply_len;
	sg_rq.hd.pack_id = scglocal(scgp)->pack_id++;
/*	sg_rq.hd.result = 0;	not needed because of fillbytes() */

	if ((caddr_t)&sg_rq != (caddr_t)sgp)
		movebytes((caddr_t)&sg_rq, (caddr_t)sgp, sizeof(struct sg_header));
#else
	fillbytes((caddr_t)sgp, sizeof(struct sg_header), '\0');

	sgp->hd.pack_len = pack_len;
	sgp->hd.reply_len = reply_len;
	sgp->hd.pack_id = scglocal(scgp)->pack_id++;
/*	sgp->hd.result = 0;	not needed because of fillbytes() */
#endif
	if (amt == 12)
		sgp->hd.twelve_byte = 1;


	for (i = 0; i < amt; i++ ) {
		sgp->buf[i] = sp->cdb.cmd_cdb[i];;
	}
	if (!(sp->flags & SCG_RECV_DATA)) {
		if ((void *)sp->addr != (void *)&sgp->buf[amt])
			movebytes(sp->addr, &sgp->buf[amt], sp->size);
		amt += sp->size;
	}
#ifdef	SG_GET_BUFSIZE
	sgp->hd.want_new  = 1;			/* Order new behaviour	*/
	sgp->hd.cdb_len	  = sp->cdb_len;	/* Set CDB length	*/
	if (sp->sense_len > SG_MAX_SENSE)
		sgp->hd.sense_len = SG_MAX_SENSE;
	else
		sgp->hd.sense_len = sp->sense_len;
#endif
	i = sizeof(struct sg_header) + amt;
	if ((amt = write(f, sgp, i)) < 0) {			/* write */
		sg_settimeout(f, scgp->deftimeout);
		return (-1);
	} else if (amt != i) {
		errmsg("scsi_send(%s) wrote %d bytes (expected %d).\n",
						scgp->cmdname, amt, i);
	}

	if (sp->addr == scglocal(scgp)->SCSIbuf) {
		movebytes(sgp, sgp2, sizeof(struct sg_header));
		sgp = sgp2;
	}
	sgp->hd.sense_buffer[0] = 0;
	if ((amt = read(f, sgp, reply_len)) < 0) {		/* read */
		sg_settimeout(f, scgp->deftimeout);
		return (-1);
	}

	if (sp->flags & SCG_RECV_DATA && ((void *)sgp->buf != (void *)sp->addr)) {
		movebytes(sgp->buf, sp->addr, sp->size);
	}
	sp->ux_errno = GETINT(sgp->hd.result);		/* Unaligned read */
	sp->error = SCG_NO_ERROR;

#ifdef	SG_GET_BUFSIZE
	if (sgp->hd.grant_new) {
		sp->sense_count = sgp->hd.sense_len;
		pack_len    = GETINT(sgp->hd.sg_cmd_status);	/* Unaligned read */
		driver_byte = (pack_len  >> 24) & 0xFF;
		host_byte   = (pack_len  >> 16) & 0xFF;
		msg_byte    = (pack_len  >> 8) & 0xFF;
		status_byte =  pack_len  & 0xFF;

		switch (host_byte) {

		case DID_OK:
				if ((driver_byte & DRIVER_SENSE ||
				    sgp->hd.sense_buffer[0] != 0)
							&& status_byte == 0) {
					/*
					 * The Linux SCSI system up to 2.1.xx
					 * trashes the status byte in the
					 * kernel. This is true at least for
					 * ide-scsi emulation. Until this gets
					 * fixed, we need this hack.
					 */
					sp->error = SCG_RETRYABLE;
					status_byte = ST_CHK_COND;
					if (sgp->hd.sense_len == 0)
						sgp->hd.sense_len = SG_MAX_SENSE;
				}
				break;

		case DID_NO_CONNECT:	/* Arbitration won, retry NO_CONNECT? */
		case DID_BAD_TARGET:
				sp->error = SCG_FATAL;
				break;
	
		case DID_TIME_OUT:
				sp->error = SCG_TIMEOUT;
				break;

		default:
				sp->error = SCG_RETRYABLE;

				if ((driver_byte & DRIVER_SENSE ||
				    sgp->hd.sense_buffer[0] != 0)
							&& status_byte == 0)
					status_byte = ST_CHK_COND;
				if (status_byte != 0 && sgp->hd.sense_len == 0)
					sgp->hd.sense_len = SG_MAX_SENSE;
				break;

		}
		if ((host_byte != DID_OK || status_byte != 0) && sp->ux_errno == 0)
			sp->ux_errno = EIO;
		sp->u_scb.cmd_scb[0] = status_byte;
		if (status_byte & ST_CHK_COND) {
			sp->sense_count = sgp->hd.sense_len;
			movebytes(sgp->hd.sense_buffer, sp->u_sense.cmd_sense, sp->sense_count);
		}
	} else
#endif
	{
		if (GETINT(sgp->hd.result) == EBUSY) {	/* Unaligned read */
			struct timeval to;

			to.tv_sec = sp->timeout;
			to.tv_usec = 500000;
			scsitimes(scgp);

			if (scgp->cmdstop->tv_sec < to.tv_sec ||
			    (scgp->cmdstop->tv_sec == to.tv_sec &&
				scgp->cmdstop->tv_usec < to.tv_usec)) {

				sp->ux_errno = 0;
				sp->error = SCG_TIMEOUT;	/* a timeout */
			} else {
				sp->error = SCG_RETRYABLE;	/* may be BUS_BUSY */
			}
		}

		if (sp->flags & SCG_RECV_DATA)
			sp->resid = (sp->size + sizeof(struct sg_header)) - amt;
		else
			sp->resid = 0;	/* sg version1 cannot return DMA resid count */

		if (sgp->hd.sense_buffer[0] != 0) {
			sp->error = SCG_RETRYABLE;
			sp->scb.chk = 1;
			sp->sense_count = SG_MAX_SENSE;
			movebytes(sgp->hd.sense_buffer, sp->u_sense.cmd_sense, sp->sense_count);
			if (sp->ux_errno == 0)
				sp->ux_errno = EIO;
		}
	}

	if (scgp->verbose > 0 && scgp->debug) {
#ifdef	SG_GET_BUFSIZE
		printf("status: 0x%08X pack_len: %d, reply_len: %d pack_id: %d result: %d wn: %d gn: %d cdb_len: %d sense_len: %d sense[0]: %02X\n",
				GETINT(sgp->hd.sg_cmd_status),
				GETINT(sgp->hd.pack_len),
				GETINT(sgp->hd.reply_len),
				GETINT(sgp->hd.pack_id),
				GETINT(sgp->hd.result),
				sgp->hd.want_new,
				sgp->hd.grant_new,
				sgp->hd.cdb_len,
				sgp->hd.sense_len,	
				sgp->hd.sense_buffer[0]);
#else
		printf("pack_len: %d, reply_len: %d pack_id: %d result: %d sense[0]: %02X\n",
				GETINT(sgp->hd.pack_len),
				GETINT(sgp->hd.reply_len),
				GETINT(sgp->hd.pack_id),
				GETINT(sgp->hd.result),
				sgp->hd.sense_buffer[0]);
#endif
#ifdef	DEBUG
		printf("sense: ");
		for (i=0; i < 16; i++)
			printf("%02X ", sgp->hd.sense_buffer[i]);
		printf("\n");
#endif
	}

	if (sp->timeout != scgp->deftimeout)
		sg_settimeout(f, scgp->deftimeout);
	return 0;
}
