/* @(#)scsi-wnt.c	1.16 99/09/08 Copyright 1998, 1999 J. Schilling, A.L. Faber */
#ifndef lint
static	char ___sccsid[] =
	"@(#)scsi-wnt.c	1.16 99/09/08 Copyright 1998, 1999 J. Schilling, A.L. Faber";
#endif
/*
 *	Interface for the Win32 ASPI library.
 *		You need wnaspi32.dll and aspi32.sys
 *		Both can be installed from ASPI_ME
 *
 *	Copyright (c) 1998 J. Schilling
 *	Copyright (c) 1999 A.L. Faber for the first implementation
 *			   of this interface.
 *	TODO:
 *	-	DMA resid handling
 *	-	better handling of maxDMA
 *	-	SCSI reset support
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
 * Include for Win32 ASPI AspiRouter
 *
 * NOTE: aspi-win32.h includes Windows.h and Windows.h includes
 *	 Base.h which has a second typedef for BOOL.
 *	 We define BOOL to make all local code use BOOL
 *	 from Windows.h and use the hidden __SBOOL for
 *	 our global interfaces.
 */
#define	BOOL	WBOOL		/* This is the Win BOOL		*/
#define	format	__format
#include <scg/aspi-win32.h>
#undef format

#ifdef	__CYGWIN32__		/* Use dlopen()			*/
#include <dlfcn.h>
#endif

/*
 * Local defines and constants
 */
/*#define DEBUG_WNTASPI*/

#define	MAX_SCG		16	/* Max # of SCSI controllers	*/
#define	MAX_TGT		16	/* Max # of SCSI Targets	*/
#define	MAX_LUN		8	/* Max # of SCSI LUNs		*/

#ifdef DEBUG_WNTASPI
#endif

struct scg_local {
	int	dummy;
};
#define scglocal(p)	((struct scg_local *)((p)->local)) 

/*
 * Local variables
 */
LOCAL	int	busses;
LOCAL	DWORD	(*pfnGetASPI32SupportInfo)(void)		= NULL;
LOCAL	DWORD	(*pfnSendASPI32Command)(LPSRB)			= NULL;
LOCAL	BOOL	(*pfnGetASPI32Buffer)(PASPI32BUFF)		= NULL;
LOCAL	BOOL	(*pfnFreeASPI32Buffer)(PASPI32BUFF)		= NULL;
LOCAL	BOOL	(*pfnTranslateASPI32Address)(PDWORD, PDWORD)	= NULL;

LOCAL	BOOL	AspiLoaded			= FALSE;
LOCAL	HANDLE	hAspiLib			= NULL;	/* Used for Loadlib */

#define	MAX_DMA_WNT	(63L*1024L) /* ASPI-Driver  allows up to 64k ??? */

/*
 * Local function prototypes
 */
LOCAL	void	exit_func	__PR((void));
#ifdef DEBUG_WNTASPI
LOCAL	void	DebugScsiSend	__PR((SRB_ExecSCSICmd s, int bDisplayBuffer));
#endif
LOCAL	void	copy_sensedata	__PR((SRB_ExecSCSICmd *cp, struct scg_cmd *sp));
LOCAL	void	set_error	__PR((SRB_ExecSCSICmd *cp, struct scg_cmd *sp));
LOCAL	BOOL	open_driver	__PR((SCSI *scgp));
LOCAL	BOOL	close_driver	__PR((void));
LOCAL	int	ha_inquiry	__PR((SCSI *scgp, int id, SRB_HAInquiry	*ip));
LOCAL	int	resetSCSIBus	__PR((void));
LOCAL	int	scsiabort	__PR((SCSI *scgp, SRB_ExecSCSICmd *sp));

LOCAL void
exit_func()
{
	if (!close_driver())
		errmsgno(EX_BAD, "Cannot close Win32-ASPI-Driver.\n");
}

EXPORT int
scsi_open(scgp, device, busno, tgt, tlun)
	SCSI	*scgp;
	char	*device;
	int	busno;
	int	tgt;
	int	tlun;
{
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

	/*
	 *  Check if variables are within the range
	 */
	if (tgt >= 0 && tgt >= 0 && tlun >= 0) {
		/*
		 * This is the non -scanbus case.
		 */
		;
	} else if (tgt != -1 || tgt != -1 || tlun != -1) {
		errno = EINVAL;
		return (-1);
	}

	if (scgp->local == NULL) {
		scgp->local = malloc(sizeof(struct scg_local));
		if (scgp->local == NULL)
			return (0);
	}
	/*
	 * Try to open ASPI-Router
	 */
	if (!open_driver(scgp))
		return (-1);

	/*
	 * More than we have ...
	 */
	if (busno >= busses) {
		close_driver();
		return (-1);
	}

	/*
	 * Install Exit Function which closes the ASPI-Router
	 */
	atexit(exit_func);		

	/*
	 * Success after all
	 */
	return (1);
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
	return (MAX_DMA_WNT);
}

EXPORT void *
scsi_getbuf(scgp, amt)
	SCSI	*scgp;
	long	amt;
{
	if (amt <= 0 || amt > scsi_maxdma(scgp)) {
		errmsgno(EX_BAD,
			"scsi_getbuf: buffer out of range; requested size is %ld bytes\n", amt);
		return ((void *)0);
	}
	if (scgp->debug)
		printf("scsi_getbuf: %ld bytes\n", amt);

	scgp->bufbase = malloc((size_t)(amt));
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

EXPORT __SBOOL
scsi_havebus(scgp, busno)
	SCSI	*scgp;
	int	busno;
{
	if (busno < 0 || busno >= busses)
		return (FALSE);

	return (TRUE);
}

EXPORT int
scsi_fileno(scgp, busno, tgt, tlun)
	SCSI	*scgp;
	int	busno;
	int	tgt;
	int	tlun;
{
	if (busno < 0 || busno >= busses ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	/*
	 * Return fake
	 */
	return (1);
}


EXPORT int
scsi_initiator_id(scgp)
	SCSI	*scgp;
{
	SRB_HAInquiry	s;

	if (ha_inquiry(scgp, scgp->scsibus, &s) < 0)
		return (-1);
	return (s.HA_SCSI_ID);
}

EXPORT int
scsi_isatapi(scgp)
	SCSI	*scgp;
{
	return (-1);	/* XXX Need to add real test */
}


/*
 * XXX scsireset not yet tested
 */
EXPORT int
scsireset(scgp)
	SCSI	*scgp;
{

	DWORD			Status = 0;
	DWORD			EventStatus = WAIT_OBJECT_0;
	HANDLE			Event	 = NULL;
	SRB_BusDeviceReset	s;

	if (scgp->debug)
		printf("Attempting to reset SCSI device\n");

	/*
	 * Check if ASPI library is loaded
	 */
	if (AspiLoaded == FALSE) {
		printf("error in scsireset: ASPI driver not loaded !\n");
		return (FALSE);
	}

	memset(&s, 0, sizeof(s));	/* Clear SRB_BesDeviceReset structure */

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	/*
	 * Set structure variables
	 */
	s.SRB_Cmd	= SC_RESET_DEV;			/* ASPI command code = SC_RESET_DEV	*/
	s.SRB_HaId	= scgp->scsibus;		/* ASPI host adapter number		*/
	s.SRB_Flags	= SRB_EVENT_NOTIFY;		/* Flags				*/
	s.SRB_Target	= scgp->target;			/* Target's SCSI ID			*/
	s.SRB_Lun	= scgp->lun;			/* Target's LUN number			*/
	s.SRB_PostProc	= (LPVOID)Event;		/* Post routine				*/
	
	/*
	 * Initiate SCSI command
	 */
	Status = pfnSendASPI32Command((LPSRB)&s);

	/*
	 * Check status
	 */
	if (Status == SS_PENDING) {
		/*
		 * Wait till command completes
		 */
		EventStatus = WaitForSingleObject(Event, INFINITE);
	}


	/**************************************************/
	/* Reset event to non-signaled state.             */
	/**************************************************/

	if (EventStatus == WAIT_OBJECT_0) {
		/*
		 * Clear event
		 */
		ResetEvent(Event);
	}

	/*
	 * Close the event handle
	 */
	CloseHandle(Event);

	/*
	 * Check condition
	 */
	if (s.SRB_Status != SS_COMP) {
		printf("ERROR! 0x%08X\n", s.SRB_Status);

		/*
		 * Indicate that error has occured
		 */
		return (FALSE);
	}

	if (scgp->debug)
		printf("Reset SCSI device completed\n");

	/*
	 * Everything went OK
	 */
	return (TRUE);
}


#ifdef DEBUG_WNTASPI
LOCAL void
DebugScsiSend(s, bDisplayBuffer)
	SRB_ExecSCSICmd	s;
	int		bDisplayBuffer;
{
	int i;

	printf("\n\nDebugScsiSend\n");
	printf("s.SRB_Cmd          = 0x%02x\n", s.SRB_Cmd);
	printf("s.SRB_HaId         = 0x%02x\n", s.SRB_HaId);
	printf("s.SRB_Flags        = 0x%02x\n", s.SRB_Flags); 
	printf("s.SRB_Target       = 0x%02x\n", s.SRB_Target); 
	printf("s.SRB_Lun          = 0x%02x\n", s.SRB_Lun);
	printf("s.SRB_BufLen       = 0x%02x\n", s.SRB_BufLen);
	printf("s.SRB_BufPointer   = %x\n",     s.SRB_BufPointer);
	printf("s.SRB_CDBLen       = 0x%02x\n", s.SRB_CDBLen);
	printf("s.SRB_SenseLen     = 0x%02x\n", s.SRB_SenseLen);
	printf("s.CDBByte          =");
	for (i=0; i < min(s.SRB_CDBLen, 16); i++) {
		printf(" %02X ", s.CDBByte[i]);
	}
	printf("\n");

	/*
	if (bDisplayBuffer != 0 && s.SRB_BufLen >= 8) {
		
		printf("s.SRB_BufPointer   =");
		for (i=0; i < 8; i++) {
			printf(" %02X ", ((char*)s.SRB_BufPointer)[i]);
		}
		printf("\n");
	}
*/
	printf("Debug done\n");
}
#endif

LOCAL void
copy_sensedata(cp, sp)
	SRB_ExecSCSICmd	*cp;
	struct scg_cmd	*sp;
{
	sp->sense_count	= cp->SRB_SenseLen;
	if (sp->sense_count > sp->sense_len)
		sp->sense_count = sp->sense_len;

	memset(&sp->u_sense.Sense, 0x00, sizeof(sp->u_sense.Sense));
	memcpy(&sp->u_sense.Sense, cp->SenseArea, sp->sense_len);

	sp->u_scb.cmd_scb[0] = cp->SRB_TargStat;
}

/*
 * Set error flags
 */
LOCAL void
set_error(cp, sp)
	SRB_ExecSCSICmd	*cp;
	struct scg_cmd	*sp;
{
	switch (cp->SRB_Status) {

	case SS_COMP:			/* 0x01 SRB completed without error  */
		sp->error = SCG_NO_ERROR;
		sp->ux_errno = 0;
		break;

	case SS_PENDING:		/* 0x00 SRB being processed          */
		/*
		 * XXX Could SS_PENDING happen ???
		 */
	case SS_ABORTED:		/* 0x02 SRB aborted                  */
	case SS_ABORT_FAIL:		/* 0x03 Unable to abort SRB          */
	case SS_ERR:			/* 0x04 SRB completed with error     */
	default:
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = EIO;
		break;

	case SS_INVALID_CMD:		/* 0x80 Invalid ASPI command         */
	case SS_INVALID_HA:		/* 0x81 Invalid host adapter number  */
	case SS_NO_DEVICE:		/* 0x82 SCSI device not installed    */

	case SS_INVALID_SRB:		/* 0xE0 Invalid parameter set in SRB */
	case SS_ILLEGAL_MODE:		/* 0xE2 Unsupported Windows mode     */
	case SS_NO_ASPI:		/* 0xE3 No ASPI managers             */
	case SS_FAILED_INIT:		/* 0xE4 ASPI for windows failed init */
	case SS_MISMATCHED_COMPONENTS:	/* 0xE7 The DLLs/EXEs of ASPI don't  */
					/*      version check                */
	case SS_NO_ADAPTERS:		/* 0xE8 No host adapters to manager  */
	
	case SS_ASPI_IS_SHUTDOWN:	/* 0xEA Call came to ASPI after      */
					/*      PROCESS_DETACH               */
	case SS_BAD_INSTALL:		/* 0xEB The DLL or other components  */
					/*      are installed wrong          */
		sp->error = SCG_FATAL;
		sp->ux_errno = EINVAL;
		break;

#ifdef	XXX
	case SS_OLD_MANAGER:		/* 0xE1 ASPI manager doesn't support */
					/*      windows                      */
#endif
	case SS_BUFFER_ALIGN:		/* 0xE1 Buffer not aligned (replaces */
					/*      SS_OLD_MANAGER in Win32)     */
		sp->error = SCG_FATAL;
		sp->ux_errno = EFAULT;
		break;

	case SS_ASPI_IS_BUSY:		/* 0xE5 No resources available to    */
					/*      execute command              */
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = EBUSY;
		break;

#ifdef	XXX
	case SS_BUFFER_TO_BIG:		/* 0xE6 Buffer size too big to handle*/
#endif
	case SS_BUFFER_TOO_BIG:		/* 0xE6 Correct spelling of 'too'    */
	case SS_INSUFFICIENT_RESOURCES:	/* 0xE9 Couldn't allocate resources  */
					/*      needed to init               */
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = ENOMEM;
		break;
	}
}


LOCAL int
scsi_send(scgp, f, sp)
	SCSI		*scgp;
	int		f;
	struct scg_cmd	*sp;
{
	DWORD			Status = 0;
	DWORD			EventStatus = WAIT_OBJECT_0;
	HANDLE			Event	 = NULL;
	SRB_ExecSCSICmd		s;

	/*
	 * Check if ASPI library is loaded
	 */
	if (AspiLoaded == FALSE) {
		errmsgno(EX_BAD, "error in scsi_send: ASPI driver not loaded.\n");
		sp->error = SCG_FATAL;
		return (-1);
	}

	if (f < 0) {
		sp->error = SCG_FATAL;
		return (-1);
	}

	/*
	 * Initialize variables
	 */
	sp->error		= SCG_NO_ERROR;
	sp->sense_count		= 0;
	sp->u_scb.cmd_scb[0]	= 0;
	sp->resid		= 0;

	memset(&s, 0, sizeof(s));	/* Clear SRB structure */

	/*
	 * Check cbd_len > the maximum command pakket that can be handled by ASPI
	 */
	if (sp->cdb_len > 16) {
		sp->error = SCG_FATAL;
		sp->ux_errno = EINVAL;
		printf("sp->cdb_len > sizeof(SRB_ExecSCSICmd.CDBByte). Fatal error in scsi_send, exiting...\n");
		return (-1);
	}
	/*
	 * copy cdrecord command into SRB
	 */
	movebytes(&sp->cdb, &(s.CDBByte), sp->cdb_len);

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	/*
	 * Fill ASPI structure
	 */
	s.SRB_Cmd	= SC_EXEC_SCSI_CMD;		/* SCSI Command			*/
	s.SRB_HaId	= scgp->scsibus;		/* Host adapter number		*/
	s.SRB_Flags	= SRB_EVENT_NOTIFY;		/* Flags			*/
	s.SRB_Target	= scgp->target;			/* Target SCSI ID		*/
	s.SRB_Lun	= scgp->lun;			/* Target SCSI LUN		*/
	s.SRB_BufLen	= sp->size;			/* # of bytes transferred	*/
	s.SRB_BufPointer= sp->addr;			/* pointer to data buffer	*/
	s.SRB_CDBLen	= sp->cdb_len;			/* SCSI command length		*/
	s.SRB_PostProc	= Event;			/* Post proc event		*/
	s.SRB_SenseLen	= SENSE_LEN;			/* Lenght of sense buffer	*/

	/*
	 * Do we receive data from this ASPI command?
	 */
	if (sp->flags & SCG_RECV_DATA) {

		s.SRB_Flags |= SRB_DIR_IN;
	} else {
		/*
		 * Set direction to output
		 */
		if (sp->size > 0) {
			s.SRB_Flags |= SRB_DIR_OUT;
		}
	}

#ifdef DEBUG_WNTASPI
	/*
	 * Dump some debug information when enabled
	 */
	DebugScsiSend(s, TRUE);
/*	DebugScsiSend(s, (s.SRB_Flags&SRB_DIR_OUT) == SRB_DIR_OUT);*/
#endif

	/*
	 * ------------ Send SCSI command --------------------------
	 */

	ResetEvent(Event);			/* Clear event handle	     */
	Status = pfnSendASPI32Command((LPSRB)&s);/* Initiate SCSI command    */

	if (Status == SS_PENDING) {		/* If in progress	     */
		/*
		 * Wait untill command completes, or times out.
		 */
		EventStatus = WaitForSingleObject(Event, sp->timeout*1000L);
/*		EventStatus = WaitForSingleObject(Event, 10L);*/

		if (EventStatus == WAIT_OBJECT_0)
			ResetEvent(Event);	/* Clear event, time out     */

		if (s.SRB_Status == SS_PENDING) {/* Check if we got a timeout*/
			if (scgp->debug)
				printf("Timeout....\n");
			scsiabort(scgp, &s);
			ResetEvent(Event);	/* Clear event, time out     */
			CloseHandle(Event);	/* Close the event handle    */

			sp->error = SCG_TIMEOUT;
			return (1);		/* Return error		     */
		}
	}
	CloseHandle(Event);			/* Close the event handle    */

	/*
	 * Check ASPI command status
	 */
	if (s.SRB_Status != SS_COMP) {
		if (scgp->debug)
			printf("Error in scsi_send: s.SRB_Status is 0x%x\n", s.SRB_Status);

		set_error(&s, sp);		/* Set error flags	     */
		copy_sensedata(&s, sp);		/* Copy sense and status     */

		if (scgp->debug)
			printf("Mapped to: error %d errno: %d\n", sp->error, sp->ux_errno);
		return (1);
	}

	/*
	 * Return success
	 */
	return (0);
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
open_driver(scgp)
	SCSI	*scgp;
{
	DWORD	astatus;
	BYTE	HACount;
	BYTE	ASPIStatus;
	int	i;

#ifdef DEBUG_WNTASPI
	printf("enter open_driver\n");
#endif

	/*
	 * Check if ASPI library is already loaded yet
	 */
	if (AspiLoaded == TRUE)
		return (TRUE);

	/*
	 * Load the ASPI library
	 */
#ifdef	__CYGWIN32__
	hAspiLib = dlopen("WNASPI32", RTLD_NOW);
#else
	hAspiLib = LoadLibrary("WNASPI32");
#endif

	/*
	 * Check if ASPI library is loaded correctly
	 */
	if (hAspiLib == NULL) {
		printf("Can not load ASPI driver! ");
		return (FALSE);
	}
  
	/*
	 * Get a pointer to GetASPI32SupportInfo function
	 * and a pointer to SendASPI32Command function
	 */
#ifdef	__CYGWIN32__
	pfnGetASPI32SupportInfo = (DWORD(*)(void))dlsym(hAspiLib, "GetASPI32SupportInfo");
	pfnSendASPI32Command = (DWORD(*)(LPSRB))dlsym(hAspiLib, "SendASPI32Command");
#else
	pfnGetASPI32SupportInfo = (DWORD(*)(void))GetProcAddress(hAspiLib, "GetASPI32SupportInfo");
	pfnSendASPI32Command = (DWORD(*)(LPSRB))GetProcAddress(hAspiLib, "SendASPI32Command");
#endif

	if ((pfnGetASPI32SupportInfo == NULL) || (pfnSendASPI32Command == NULL)) {
		printf("ASPI function not found in library!");
		return (FALSE);
	}

#ifdef	__CYGWIN32__
	pfnGetASPI32Buffer = (BOOL(*)(PASPI32BUFF))dlsym(hAspiLib, "GetASPI32Buffer");
	pfnFreeASPI32Buffer = (BOOL(*)(PASPI32BUFF))dlsym(hAspiLib, "FreeASPI32Buffer");
	pfnTranslateASPI32Address = (BOOL(*)(PDWORD, PDWORD))dlsym(hAspiLib, "TranslateASPI32Address");
#else
	pfnGetASPI32Buffer = (BOOL(*)(PASPI32BUFF))GetProcAddress(hAspiLib, "GetASPI32Buffer");
	pfnFreeASPI32Buffer = (BOOL(*)(PASPI32BUFF))GetProcAddress(hAspiLib, "FreeASPI32Buffer");
	pfnTranslateASPI32Address = (BOOL(*)(PDWORD, PDWORD))GetProcAddress(hAspiLib, "TranslateASPI32Address");
#endif

	/*
	 * Set AspiLoaded variable
	 */
	AspiLoaded = TRUE;

	astatus = pfnGetASPI32SupportInfo();

	ASPIStatus = HIBYTE(LOWORD(astatus));
	HACount    = LOBYTE(LOWORD(astatus));

	if (scgp->debug)
		printf("open_driver %X HostASPIStatus=0x%x HACount=0x%x\n", astatus, ASPIStatus, HACount);

	if (ASPIStatus != SS_COMP && ASPIStatus != SS_NO_ADAPTERS) {
		printf("Could not find any host adapters\n");
		printf("ASPIStatus == 0x%02X", ASPIStatus);
		return (FALSE);
	}
	busses = HACount;

#ifdef DEBUG_WNTASPI
	printf("open_driver HostASPIStatus=0x%x HACount=0x%x\n", ASPIStatus, HACount);
	printf("leaving open_driver\n");
#endif

	for (i=0; i < busses; i++) {
		SRB_HAInquiry	s;

		ha_inquiry(scgp, i, &s);
	}

	/*
	 * Indicate that library loaded/initialized properly
	 */
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
	/*
	 * If library is loaded
	 */
	if (hAspiLib) {
		/*
		 * Clear all variables
		 */
		AspiLoaded		= FALSE;
		pfnGetASPI32SupportInfo	= NULL;
		pfnSendASPI32Command	= NULL;
		pfnGetASPI32Buffer	= NULL;
		pfnFreeASPI32Buffer	= NULL;
		pfnTranslateASPI32Address = NULL;

		/*
		 * Free ASPI library, we do not need it any longer
		 */
#ifdef	__CYGWIN32__
		dlclose(hAspiLib);
#else
		FreeLibrary(hAspiLib);
#endif
		hAspiLib = NULL;
	}

	/*
	 * Indicate that shutdown has been finished properly
	 */
	return (TRUE);
}

LOCAL int
ha_inquiry(scgp, id, ip)
	SCSI		*scgp;
	int		id;
	SRB_HAInquiry	*ip;
{
	DWORD		Status;
	
	ip->SRB_Cmd	= SC_HA_INQUIRY;
	ip->SRB_HaId	= id;
	ip->SRB_Flags	= 0;
	ip->SRB_Hdr_Rsvd= 0;

	Status = pfnSendASPI32Command((LPSRB)ip);

	if (scgp->debug) {
		printf("Status : %d\n",	Status);
		printf("hacount: %d\n", ip->HA_Count);
		printf("SCSI id: %d\n", ip->HA_SCSI_ID);
		printf("Manager: '%.16s'\n", ip->HA_ManagerId);
		printf("Identif: '%.16s'\n", ip->HA_Identifier);
		scsiprbytes("Unique:", ip->HA_Unique, 16);
	}
	if (ip->SRB_Status != SS_COMP)
		return (-1);
	return (0);
}

LOCAL int
resetSCSIBus(void)
{
	DWORD			Status;
	HANDLE			Event;
	SRB_BusDeviceReset	s;

	printf("Attempting to reset SCSI bus\n");

	Event = CreateEvent(NULL, TRUE, FALSE, NULL);

	memset(&s, 0, sizeof(s));	/* Clear SRB_BesDeviceReset structure */

	/*
	 * Set structure variables
	 */
	s.SRB_Cmd = SC_RESET_DEV;
	s.SRB_PostProc = (LPVOID)Event;

	/*
	 * Clear event
	 */
	ResetEvent(Event);

	/*
	 * Initiate SCSI command
	 */
	Status = pfnSendASPI32Command((LPSRB)&s);

	/*
	 * Check status
	 */
	if (Status == SS_PENDING) {
		/*
		 * Wait till command completes
		 */
		WaitForSingleObject(Event, INFINITE);
	}

	/*
	 * Close the event handle
	 */
	CloseHandle(Event);

	/*
	 * Check condition
	 */
	if (s.SRB_Status != SS_COMP) {
		printf("ERROR  0x%08X\n", s.SRB_Status);

		/*
		 * Indicate that error has occured
		 */
		return (FALSE);
	}

	/*
	 * Everything went OK
	 */
	return (TRUE);
}

LOCAL int
scsiabort(scgp, sp)
	SCSI		*scgp;
	SRB_ExecSCSICmd	*sp;
{
	DWORD			Status = 0;
	SRB_Abort		s;

	if (scgp->debug)
		printf("Attempting to abort SCSI command\n");

	/*
	 * Check if ASPI library is loaded
	 */
	if (AspiLoaded == FALSE) {
		printf("error in scsiabort: ASPI driver not loaded !\n");
		return (FALSE);
	}

	/*
	 * Set structure variables
	 */
	s.SRB_Cmd	= SC_ABORT_SRB;			/* ASPI command code = SC_ABORT_SRB	*/
	s.SRB_HaId	= scgp->scsibus;		/* ASPI host adapter number		*/
	s.SRB_Flags	= 0;				/* Flags				*/
	s.SRB_ToAbort	= (LPSRB)&sp;			/* sp					*/

	/*
	 * Initiate SCSI abort
	 */
	Status = pfnSendASPI32Command((LPSRB)&s);

	/*
	 * Check condition
	 */
	if (s.SRB_Status != SS_COMP) {
		printf("Abort ERROR! 0x%08X\n", s.SRB_Status);

		/*
		 * Indicate that error has occured
		 */
		return (FALSE);
	}

	if (scgp->debug)
		printf("Abort SCSI command completed\n");

	/*
	 * Everything went OK
	 */
	return (TRUE);
}
