/* @(#)srb_os2.h	1.0 98/10/28 Copyright 1998 D. Dorau, C. Wohlgemuth */
/*
 *	Definitions for ASPI-Router (ASPIROUT.SYS).
 *
 *	Copyright (c) 1998 D. Dorau, C. Wohlgemuth
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

#pragma pack(1)

        /* SRB command */
#define SRB_Inquiry     0x00
#define SRB_Device      0x01
#define SRB_Command     0x02
#define SRB_Abort       0x03
#define SRB_Reset       0x04
#define SRB_Param       0x05

        /* SRB status */
#define SRB_Busy        0x00    /* SCSI request in progress */
#define SRB_Done        0x01    /* SCSI request completed without error */
#define SRB_Aborted     0x02    /* SCSI aborted by host */
#define SRB_BadAbort    0x03    /* Unable to abort SCSI request */
#define SRB_Error       0x04    /* SCSI request completed with error */
#define SRB_BusyPost    0x10    /* SCSI request in progress with POST - Nokia */
#define SRB_InvalidCmd  0x80    /* Invalid SCSI request */
#define SRB_InvalidHA   0x81    /* Invalid Hhost adapter number */
#define SRB_BadDevice   0x82    /* SCSI device not installed */

        /* SRB flags */
#define SRB_Post        0x01    /* Post vector valid */
#define SRB_Link        0x02    /* Link vector valid */
#define SRB_SG          0x04    /* Nokia: scatter/gather */
                                /* S/G: n * (4 bytes length, 4 bytes addr) */
                                /* No of s/g items not limited by HA spec. */
#define SRB_NoCheck     0x00    /* determined by command, not checked  */
#define SRB_Read        0x08    /* target to host, length checked  */
#define SRB_Write       0x10    /* host to target, length checked  */
#define SRB_NoTransfer  0x18    /* no data transfer  */
#define SRB_DirMask     0x18    /* bit mask */

        /* SRB host adapter status */
#define SRB_NoError     0x00    /* No host adapter detected error */
#define SRB_Timeout     0x11    /* Selection timeout */
#define SRB_DataLength  0x12    /* Data over/underrun */
#define SRB_BusFree     0x13    /* Unexpected bus free */
#define SRB_BusSequence 0x14    /* Target bus sequence failure */

        /* SRB target status field */
#define SRB_NoStatus    0x00    /* No target status */
#define SRB_CheckStatus 0x02    /* Check status (sense data valid) */
#define SRB_LUN_Busy    0x08    /* Specified LUN is busy */
#define SRB_Reserved    0x18    /* Reservation conflict */

#define MaxCDBStatus    64      /* max size of CDB + status */


typedef struct SRb {
        unsigned char   cmd,                            /* 00 */
                        status,                         /* 01 */
                        ha_num,                         /* 02 */
                        flags;                          /* 03 */
        unsigned long   res_04_07;                      /* 04..07 */
        union {                                         /* 08 */

        /* SRB_Inquiry */
                struct {
                        unsigned char   num_ha,         /* 08 */
                                        ha_target,      /* 09 */
                                        aspimgr_id[16], /* 0A..19 */
                                        host_id[16],    /* 1A..29 */
                                        unique_id[16];  /* 2A..39 */
                } inq;

        /* SRB_Device */
                struct {
                        unsigned char   target,         /* 08 */
                                        lun,            /* 09 */
                                        devtype;        /* 0A */
                } dev;

        /* SRB_Command */
                struct {
                        unsigned char   target,         /* 08 */
                                        lun;            /* 09 */
                        unsigned long   data_len;       /* 0A..0D */
                        unsigned char   sense_len;      /* 0E */
			unsigned long data_ptr;       /* 0F..12 */
			unsigned long link_ptr;       /* 13..16 */
		//	void * _Seg16     data_ptr;       /* 0F..12 */
                  //      void * _Seg16     link_ptr;       /* 13..16 */
                        unsigned char   cdb_len,        /* 17 */
                                        ha_status,      /* 18 */
                                        target_status;  /* 19 */
			unsigned char   _Seg16postSRB[4];
		//   void    (* _Seg16 post) (SRB *);  /* 1A..1D */
                        unsigned char   res_1E_29[12];  /* 1E..29 */
                        unsigned char   res_2A_3F[22];  /* 2A..3F */
                        unsigned char   cdb_st[64];     /* 40..7F CDB+status */
                        unsigned char   res_80_BF[64];  /* 80..BF */
                } cmd;

        /* SRB_Abort */
                struct {
			unsigned char _Seg16srb[4];
		//     void * _Seg16     srb;            /* 08..0B */
                } abt;

        /* SRB_Reset */
                struct {
                        unsigned char   target,         /* 08 */
                                        lun,            /* 09 */
                                        res_0A_17[14],  /* 0A..17 */
                                        ha_status,      /* 18 */
                                        target_status;  /* 19 */
                } res;

        /* SRB_Param - unused by ASPI4OS2 */
                struct {
                        unsigned char   unique[16];     /* 08..17 */
                } par;

        } u;
} SRB;


// SCSI sense codes
// Note! This list may not be complete. I did this compilation for use with tape drives.

#define Sense_Current   0x70;   // Current Error
#define Sense_Deferred  0x71;   // Deferred Error
#define Sense_Filemark  0x80;   // Filemark detected
#define Sense_EOM       0x40;   // End of medium detected
#define Sense_ILI       0x20;   // Incorrect length indicator

// Sense Keys

#define SK_NoSense      0x00;   // No Sense
#define SK_RcvrdErr     0x01;   // Recovered Error
#define SK_NotReady     0x02;   // Not ready
#define SK_MedErr       0x03;   // Medium Error
#define SK_HWErr        0x04;   // Hardware Error
#define SK_IllReq       0x05;   // Illegal Request
#define SK_UnitAtt      0x06;   // Unit attention
#define SK_DataProt     0x07:   // Data Protect
#define SK_BlankChk     0x08:   // Blank Check
#define SK_VndSpec      0x09;   // Vendor Specific
#define SK_CopyAbort    0x0A;   // Copy Aborted
#define SK_AbtdCmd      0x0B;   // Aborted Command
#define SK_Equal        0x0C;   // Equal
#define SK_VolOvfl      0x0D;   // Volume Overflow
#define SK_MisComp      0x0E;   // Miscompare
#define SK_Reserved     0x0F;   // Reserved
