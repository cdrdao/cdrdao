/* @(#)scsi-linux-pg.c	1.18 99/09/07 Copyright 1997 J. Schilling */
#ifndef lint
static	char ___sccsid[] =
	"@(#)scsi-linux-pg.c	1.18 99/09/07 Copyright 1997 J. Schilling";
#endif
/*
 *	Interface for the Linux PARIDE implementation.
 *
 *	We emulate the functionality of the scg driver, via the pg driver.
 *
 *	Copyright (c) 1997  J. Schilling
 *	Copyright (c) 1998  Grant R. Guenther	<grant@torque.net>
 *			    Under the terms of the GNU public license.
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

#include <linux/pg.h>

#ifdef	USE_PG_ONLY

#define	MAX_SCG		1	/* Max # of SCSI controllers */
#define	MAX_TGT		8
#define	MAX_LUN		8

struct scg_local {
	short	scgfiles[MAX_SCG][MAX_TGT][MAX_LUN];
	short	buscookies[MAX_SCG];
	int	pgbus;
	char	*SCSIbuf;
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

#else

#define	scsi_open	pg_open
#define	scsi_close	pg_close
#define	scsi_send	pg_send
#define	scsi_maxdma	pg_maxdma
#define	scsi_initiator_id pg_initiator_id
#define	scsi_isatapi	pg_isatapi
#define	scsireset	pg_reset

LOCAL	int	pg_open		__PR((SCSI *scgp, char *device, int busno, int tgt, int tlun));
LOCAL	int	pg_close	__PR((SCSI *scgp));
LOCAL	long	pg_maxdma	__PR((SCSI *scgp));
LOCAL	int 	pg_initiator_id	__PR((SCSI *scgp));
LOCAL	int 	pg_isatapi	__PR((SCSI *scgp));
LOCAL	int	pg_reset	__PR((SCSI *scgp));
LOCAL	int	pg_send		__PR((SCSI *scgp, int f, struct scg_cmd	*sp));

#endif

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
#ifdef	USE_PG_ONLY
	register int	t;
	register int	l;
#endif
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

#ifndef	USE_PG_ONLY
	/*
	 * We need to find a fake bus number for the parallel port interface.
	 * Unfortunatly, the buscookie array may contain holes if
	 * SCSI_IOCTL_GET_BUS_NUMBER works, so we are searching backwards
	 * for some place for us.
	 * XXX Should add extra space in buscookies for a "PP bus".
	 */

	if (scglocal(scgp)->buscookies[MAX_SCG-1] != (short)-1)
		return (0);			/* No space for pgbus */

	for (b=MAX_SCG-1; b >= 0; b--) {
		if (scglocal(scgp)->buscookies[b] != (short)-1) {
			scglocal(scgp)->pgbus = ++b;
			break;
		}
	}
	if (scgp->debug)
		printf("PP Bus: %d\n", scglocal(scgp)->pgbus);
#else
	if (scgp->local == NULL) {
		scgp->local = malloc(sizeof(struct scg_local));
		if (scgp->local == NULL)
			return (0);

		scglocal(scgp)->pgbus = -2;
		scglocal(scgp)->SCSIbuf = (char *)-1;

		for (b=0; b < MAX_SCG; b++) {
			for (t=0; t < MAX_TGT; t++) {
				for (l=0; l < MAX_LUN ; l++)
					scglocal(scgp)->scgfiles[b][t][l] = (short)-1;
			}
		}
	}
#endif
	if (scglocal(scgp)->pgbus < 0)
		scglocal(scgp)->pgbus = 0;

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2))
		goto openbydev;

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {	
#ifndef	USE_PG_ONLY
		if (scglocal(scgp)->pgbus != busno)
			return (0);
#endif
		sprintf(devname, "/dev/pg%d", tgt);
		f = open(devname, 2);
		if (f < 0) {
			if (scgp->errstr)
				js_snprintf(scgp->errstr, SCSI_ERRSTR_SIZE,
						"Cannot open '%s'", devname);
			return (0);
		}
		scglocal(scgp)->scgfiles[busno][tgt][tlun] = f;
		return 1;
	} else {
/*		scgp->scsibus = 0; scgp->lun = 0;*/
		tlun = 0;
		for(tgt=0;tgt<MAX_TGT;tgt++) {
			sprintf(devname, "/dev/pg%d", tgt);
			f = open(devname, 2);
			if (f >= 0) {
				scglocal(scgp)->scgfiles[scglocal(scgp)->pgbus][tgt][tlun] = f;
				nopen++;
			}
		}
	}
openbydev:
	if (device != NULL && *device != '\0') {
		char	*p;

		if (tlun < 0)
			return (0);
		f = open(device, 2);
		if (f < 0 && errno == ENOENT)
			return (0);

		p = device + strlen(device) -1;
		tgt = *p - '0';
		if (tgt < 0 || tgt > 9)
			return (0);
		scgp->scsibus = scglocal(scgp)->pgbus;
		scgp->target = tgt;
		scgp->lun = tlun;
		scglocal(scgp)->scgfiles[scglocal(scgp)->pgbus][tgt][tlun] = f;
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
	if (scglocal(scgp)->pgbus < 0)
		return (0);
	b = scglocal(scgp)->pgbus;
	scglocal(scgp)->buscookies[b] = (short)-1;

	for (t=0; t < MAX_TGT; t++) {
		for (l=0; l < MAX_LUN ; l++) {
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
	return (PG_MAX_DATA);
}

#ifdef	USE_PG_ONLY

EXPORT void *
scsi_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
        char    *ret;

	if (amt <= 0 || amt > scsi_maxdma(scgp))
                return ((void *)0);
        if (scgp->debug)
                printf("scsi_getbuf: %ld bytes\n", amt);

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
#endif	/* USE_PG_ONLY */

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
	return (TRUE);
}

EXPORT
int scsireset(scgp)
	SCSI	*scgp;
{
	int	f = scsi_fileno(scgp, scgp->scsibus, scgp->target, scgp->lun);

	struct pg_write_hdr hdr = {PG_MAGIC, PG_RESET, 0};

	return (write(f, (char *)&hdr, sizeof(hdr)));

}

#ifndef MAX
#define MAX(a,b)	((a)>(b)?(a):(b))
#endif

#define	RHSIZE	sizeof(struct pg_read_hdr)
#define WHSIZE  sizeof(struct pg_write_hdr)
#define LEAD	MAX(RHSIZE,WHSIZE)

LOCAL int
do_scsi_cmd(scgp, f, sp)
	SCSI	*scgp;
	int		f;
	struct scg_cmd	*sp;
{

	char	local[LEAD+PG_MAX_DATA];
	int	use_local, i, r;
	int	inward = (sp->flags & SCG_RECV_DATA);

	struct pg_write_hdr *whp;
	struct pg_read_hdr  *rhp;
	char		    *dbp;

	if (sp->cdb_len > 12)
		comerrno(EX_BAD, "Can't do %d byte command.\n", sp->cdb_len);

	if (sp->addr == scglocal(scgp)->SCSIbuf) {
		use_local = 0; 
		dbp = sp->addr;
	} else {
		use_local = 1;
		dbp = &local[LEAD];
		if (!inward) 
			movebytes(sp->addr, dbp, sp->size);
	}

	whp = (struct pg_write_hdr *)(dbp - WHSIZE);
	rhp = (struct pg_read_hdr *)(dbp - RHSIZE);

	whp->magic   = PG_MAGIC;
	whp->func    = PG_COMMAND;
	whp->dlen    = sp->size;
	whp->timeout = sp->timeout;

	for(i=0; i<12; i++) {
		if (i < sp->cdb_len)
			whp->packet[i] = sp->cdb.cmd_cdb[i];
		else
			whp->packet[i] = 0;
	}

	i = WHSIZE;
	if (!inward) 
		i += sp->size;

	r = write(f, (char *)whp, i);

	if (r < 0) {				/* command was not sent */
		sp->ux_errno = geterrno();
		if (sp->ux_errno == ETIME) {
			/*
			 * If the previous command timed out, we cannot send
			 * any further command until the command in the drive
			 * is ready. So we behave as if the drive did not
			 * respond to the command.
			 */
			sp->error = SCG_FATAL;
			return 0;
		}
		return -1;
	}

	if (r != i)
		errmsg("scsi_send(%s) wrote %d bytes (expected %d).\n",
			scgp->cmdname, r, i);

	sp->ux_errno = 0;
	sp->sense_count = 0;

	r = read(f, (char *)rhp, RHSIZE+sp->size);

	if (r < 0) {
		sp->ux_errno = geterrno();
		if (sp->ux_errno == ETIME) {
			sp->error = SCG_TIMEOUT;
			return 0;
		}
		sp->error = SCG_FATAL;
		return -1;
	}
		
	i = rhp->dlen;
	if (i > sp->size) {
		/*
		 * "DMA overrun" should be handled in the kernel.
		 * However this may happen with flaky PP adapters.
		 */
		errmsgno(EX_BAD,
			"DMA (read) overrun by %d bytes (requested %d bytes).\n",
			i - sp->size, sp->size);
		i = sp->size;
	}

	if (use_local && inward)
		movebytes(dbp, sp->addr, i);

	sp->resid = sp->size - i;

	fillbytes(&sp->scb, sizeof(sp->scb), '\0');
        fillbytes(&sp->u_sense.cmd_sense, sizeof(sp->u_sense.cmd_sense), '\0');

	sp->error = SCG_NO_ERROR;
	i = rhp->scsi?2:0;
	sp->u_scb.cmd_scb[0] = i;
	if (i) {
		if (sp->ux_errno == 0)
			sp->ux_errno = EIO;
		sp->error = SCG_RETRYABLE;
	}

	return 0;

}

LOCAL int
do_scsi_sense(scgp, f, sp)
	SCSI	*scgp;
	int		f;
	struct scg_cmd	*sp;
{
        int             ret;
        struct scg_cmd  s_cmd;

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
	if (sp->u_scb.cmd_scb[0])
		ret = do_scsi_sense(scgp, f, sp);
	return (ret);
}

/* end of scsi-linux-pg.c */

#ifndef	USE_PG_ONLY

#undef	scsi_open
#undef	scsi_close
#undef	scsi_send
#undef	scsi_maxdma
#undef	scsi_initiator_id
#undef	scsi_isatapi
#undef	scsireset

#endif
