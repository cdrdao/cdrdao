/* @(#)scsiopen.c	1.79 99/09/07 Copyright 1995 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)scsiopen.c	1.79 99/09/07 Copyright 1995 J. Schilling";
#endif
/*
 *	SCSI command functions for cdrecord
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

/*
 * NOTICE:	The Philips CDD 521 has several firmware bugs.
 *		One of them is not to respond to a SCSI selection
 *		within 200ms if the general load on the
 *		SCSI bus is high. To deal with this problem
 *		most of the SCSI commands are send with the
 *		SCG_CMD_RETRY flag enabled.
 */
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
#include <timedefs.h>

#include <utypes.h>
#include <btorder.h>
#include <scg/scgcmd.h>
#include <scg/scsidefs.h>
#include <scg/scsireg.h>
#include <scg/scsitransp.h>

#define	strbeg(s1,s2)	(strstr((s2), (s1)) == (s2))

extern	int	lverbose;

EXPORT	SCSI	*open_scsi	__PR((char *scsidev, char *errs, int slen, int debug,
								int be_verbose));
LOCAL	int	scsi_scandev	__PR((char *devp, char *errs, int slen, 
							int *busp, int *tgtp, int *lunp));
EXPORT	int	close_scsi	__PR((SCSI * scgp));
EXPORT	void	scsi_settimeout	__PR((SCSI * scgp, int timeout));

EXPORT	SCSI	*scsi_smalloc	__PR((void));
EXPORT	void	scsi_sfree	__PR((SCSI *scgp));

/*
 * Open a SCSI device.
 *
 * Possible syntax is:
 *
 * Preferred:
 *	dev=target,lun / dev=scsibus,target,lun
 *
 * Needed on some systems:
 *	dev=devicename:target,lun / dev=devicename:scsibus,target,lun
 *
 * On systems that don't support SCSI Bus scanning this syntax helps:
 *	dev=devicename:@ / dev=devicename:@,lun
 * or	dev=devicename (undocumented)
 *
 * NOTE: As the 'lun' is part of the SCSI command descriptor block, it
 * 	 must always be known. If the OS cannot map it, it must be
 *	 specified on command line.
 */
EXPORT SCSI *
open_scsi(scsidev, errs, slen, debug, be_verbose)
	char	*scsidev;
	char	*errs;
	int	slen;
	int	debug;
	int	be_verbose;
{
	char	devname[256];
	char	*devp = NULL;
	int	x1, x2, x3;
	int	n = 0;
	SCSI	*scgp;

	if (errs)
		errs[0] = '\0';
	scgp = scsi_smalloc();
	if (scgp == NULL) {
		if (errs)
			js_snprintf(errs, slen, "No memory for SCSI structure");
		return ((SCSI *)0);
	}
	scgp->debug = debug;

	devname[0] = '\0';
	if (scsidev != NULL) {
		if ((devp = strchr(scsidev, ':')) == NULL) {
			if (strchr(scsidev, ',') == NULL) {
				/* Notation form: 'devname' (undocumented)   */
				/* Fetch bus/tgt/lun values from OS	     */
				n = -1;
				scgp->lun = -2;	/* Lun must be known	     */
				strncpy(devname, scsidev, sizeof(devname)-1);
				devname[sizeof(devname)-1] = '\0';
			} else {
				/* Basic notation form: 'bus,tgt,lun'	     */
				devp = scsidev;
			}
		} else {
			/* Notation form: 'devname:bus,tgt,lun'/'devname:@'  */
			x1 = devp - scsidev;
			if (x1 >= (int)sizeof(devname))
				x1 = sizeof(devname)-1;
			strncpy(devname, scsidev, x1);
			devname[x1] = '\0';
			devp++;
			/* Check for a notation in the form 'devname:@'	     */
			if (devp[0] == '@') {
				if (devp[1] == '\0') {
					scgp->lun = -2;
				} else if (devp[1] == ',') {
					if (*astoi(&devp[2], &scgp->lun) != '\0') {
						errno = EINVAL;
						if (errs)
							js_snprintf(errs, slen,
								"Invalid lun specifier '%s'",
										&devp[2]);
						return ((SCSI *)0);
					}
				}
				n = -1;
				devp = NULL;
			}
		}
	}
	if (devp != NULL) {
		n = scsi_scandev(devp, errs, slen, &x1, &x2, &x3);
		if (n < 0) {
			errno = EINVAL;
			return ((SCSI *)0);
		}
	}
	if (n == 3 || n == 2) {	/* Got bus,target,lun or target,lun	*/
		scgp->scsibus = x1;
		scgp->target = x2;
		scgp->lun = x3;
	} else if (n == -1) {	/* Got device:@, fetch bus/lun from OS	*/
		scgp->scsibus = scgp->target = -2;
	} else if (devp != NULL) {
		printf("WARNING: device not valid, trying to use default target...\n");
		scgp->scsibus = 0;
		scgp->target = 6;
		scgp->lun = 0;
	}
	if (be_verbose && scsidev != NULL) {
		printf("scsidev: '%s'\n", scsidev);
		if (devname[0] != '\0')
			printf("devname: '%s'\n", devname);
		printf("scsibus: %d target: %d lun: %d\n",
					scgp->scsibus, scgp->target, scgp->lun);
	}
	if (scsi_open(scgp, devname, scgp->scsibus, scgp->target, scgp->lun) <= 0) {
		if (errs && scgp->errstr)
			js_snprintf(errs, slen, scgp->errstr);
		scsi_sfree(scgp);
		return ((SCSI *)0);
	}
	return (scgp);
}

/*
 * Convert scgp->target,lun or scsibus,target,lun syntax.
 * Check for bad syntax and invalid values.
 * This is definitely better than using scanf() as it checks for syntax errors.
 */
LOCAL int
scsi_scandev(devp, errs, slen, busp, tgtp, lunp)
	char	*devp;
	char	*errs;
	int	slen;
	int	*busp;
	int	*tgtp;
	int	*lunp;
{
	int	x1, x2, x3;
	int	n = 0;
	char	*p = devp;

	x1 = x2 = x3 = 0;
	*busp = *tgtp = *lunp = 0;

	if (*p != '\0') {
		p = astoi(p, &x1);
		if (*p == ',') {
			p++;
			n++;
		} else {
			if (errs)
				js_snprintf(errs, slen, "Invalid bus or target specifier in '%s'", devp);
			return (-1);
		}
	}
	if (*p != '\0') {
		p = astoi(p, &x2);
		if (*p == ',' || *p == '\0') {
			if (*p != '\0')
				p++;
			n++;
		} else {
			if (errs)
				js_snprintf(errs, slen, "Invalid target or lun specifier in '%s'", devp);
			return (-1);
		}
	}
	if (*p != '\0') {
		p = astoi(p, &x3);
		if (*p == '\0') {
			n++;
		} else {
			if (errs)
				js_snprintf(errs, slen, "Invalid lun specifier in '%s'", devp);
			return (-1);
		}
	}
	if (n == 3) {
		*busp = x1;
		*tgtp = x2;
		*lunp = x3;
	}
	if (n == 2) {
		*tgtp = x1;
		*lunp = x2;
	}
	if (n == 1) {
		*tgtp = x1;
	}

	if (x1 < 0 || x2 < 0 || x3 < 0) {
		if (errs)
			js_snprintf(errs, slen, "Invalid value for bus, target or lun (%d,%d,%d)",
				*busp, *tgtp, *lunp);
		return (-1);
	}
	return (n);
}

EXPORT int
close_scsi(scgp)
	SCSI	*scgp;
{
	scsi_close(scgp);
	scsi_sfree(scgp);
	return (0);
}

EXPORT void
scsi_settimeout(scgp, timeout)
	SCSI	*scgp;
	int	timeout;
{
#ifdef	nonono
	if (timeout >= 0)
		scgp->deftimeout = timeout;
#else
	scgp->deftimeout = timeout;
#endif
}

EXPORT SCSI *
scsi_smalloc()
{
	SCSI	*scgp;

	scgp = malloc(sizeof(*scgp));
	if (scgp == NULL)
		return ((SCSI *)0);

	fillbytes(scgp, sizeof(*scgp), 0);
	scgp->scsibus	= -1;
	scgp->target	= -1;
	scgp->lun	= -1;
	scgp->deftimeout = 20;
	scgp->running	= FALSE;

	scgp->cmdstart = malloc(sizeof(struct timeval));
	if (scgp->cmdstart == NULL)
		goto err;
	scgp->cmdstop = malloc(sizeof(struct timeval));
	if (scgp->cmdstop == NULL)
		goto err;
	scgp->scmd = malloc(sizeof(struct scg_cmd));
	if (scgp->scmd == NULL)
		goto err;
	scgp->errstr = malloc(SCSI_ERRSTR_SIZE);
	if (scgp->errstr == NULL)
		goto err;
	scgp->errptr = scgp->errstr;
	scgp->errstr[0] = '\0';
	scgp->inq = malloc(sizeof(struct scsi_inquiry));
	if (scgp->inq == NULL)
		goto err;
	scgp->cap = malloc(sizeof(struct scsi_capacity));
	if (scgp->cap == NULL)
		goto err;

	return (scgp);
err:
	scsi_sfree(scgp);
	return ((SCSI *)0);
}

EXPORT void
scsi_sfree(scgp)
	SCSI	*scgp;
{
	if (scgp->cmdstart)
		free(scgp->cmdstart);
	if (scgp->cmdstop)
		free(scgp->cmdstop);
	if (scgp->scmd)
		free(scgp->scmd);
	if (scgp->inq)
		free(scgp->inq);
	if (scgp->cap)
		free(scgp->cap);
	if (scgp->local)
		free(scgp->local);
	scsi_freebuf(scgp);
	if (scgp->errstr)
		free(scgp->errstr);
	free(scgp);
}
