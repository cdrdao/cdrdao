/*------------------------------------------------------------------------------
 winaspi.h
------------------------------------------------------------------------------*/

#define SS_PENDING         0x00  // SCSI request is in progress.
#define SS_COMP            0x01  // SCSI Request Completed Without Error
#define SS_ABORTED         0x02  // SCSI command has been aborted.
#define SS_ERR             0x04  //  SCSI command has completed with an error.
#define SS_INVALID_SRB     0xE0  // One or more parameters in the SCSI Request Block
                                 // (SRB) are set incorrectly.
#define SS_OLD_MANAGER     0xE1  // One or more ASPI for DOS managers are loaded which
                                 // do not support ASPI for Win16.
#define SS_ILLEGAL_MODE    0xE2  // This ASPI manager does not support this mode of
                                 // Windows. You typically see this error code when
                                 // running Windows in Real Mode.
#define SS_NO_ASPI         0xE3  //  No ASPI managers are loaded. This typically occurs
                                 //  when a DOS ASPI manager is not resident in memory.
#define SS_FAILED_INIT     0xE4  //  For some reason, other than SS_OLD_MANAGER,
                                 //  SS_ILLEGAL_MODE or SS_NO_ASPI, ASPI for Win16
                                 //  could not properly initialize itself. This may be
                                 //  caused by a lack of system resources.
#define SS_ASPI_IS_BUSY    0xE5  //  The ASPI manager cannot handle the request at this
                                 //  time. This error generally occurs if the ASPI
                                 //  manager is already using up all of his resources
                                 //  to execute other requests. Try resending the
                                 //  command later.
#define SS_BUFFER_TO_BIG   0xE6  //  The ASPI manager cannot handle the given transfer
#define SS_INVALID_HA      0x81  //  Invalid Host Adapter Number
#define SS_NO_DEVICE       0x82  //  SCSI Device Not Installed

#define HASTAT_OK          0x00  //  Host adapter did not detect an error
#define HASTAT_SEL_TO      0x11  //  Selection time-out
#define HASTAT_DO_DU       0x12  //  Data overrun/underrun
#define HASTAT_BUS_FREE    0x13  //  Unexpected Bus Free
#define HASTAT_PHASE_ERR   0x14  //  Target Bus phase sequence failure

#define STATUS_GOOD        0x00  //  No target status
#define STATUS_CHKCOND     0x02  //  Check status (sense data is in SenseArea)
#define STATUS_BUSY        0x08  //  Specified Target/LUN is busy
#define STATUS_RESCONF     0x18  //  Reservation conflict

#define SENSE_LEN          14


#define SC_HA_INQUIRY      0x00
#define SC_GET_DEV_TYPE    0x01
#define SC_EXEC_SCSI_CMD   0x02
#define SC_ABORT_SRB       0x03
#define SC_RESET_DEV       0x04

#define SRBF_POSTING       0x01
#define SRBF_LINKED        0x02
#define SRBF_NOLENCHECK    0x00  // direction ctrled by SCSI cmd
#define SRBF_READ          0x08  // xfer to host, length checked
#define SRBF_WRITE         0x10  // xfer to target, length checked
#define SRBF_NOXFER        0x18  // no data transfer
#define SRB_EVENT_NOTIFY   0x40



/*
typedef unsigned char BYTE;
typedef unsigned int  WORD;
typedef unsigned long DWORD;
*/

typedef unsigned char * LPSRB;


typedef struct
{
    BYTE        SRB_Cmd;            // ASPI command code = SC_HA_INQUIRY
    BYTE        SRB_Status;         // ASPI command status byte
    BYTE        SRB_HaId;           // ASPI host adapter number
    BYTE        SRB_Flags;          // ASPI request flags
    WORD        SRB_55AASignature;  // Extended signature bytes
    WORD        SRB_ExtBufferSize;  // Extended buffer length
    BYTE        HA_Count;           // Number of host adapters present
    BYTE        HA_SCSI_ID;         // SCSI ID of host adapter
    BYTE        HA_ManagerId[16];   // String describing the manager
    BYTE        HA_Identifier[16];  // String describing the host adapter
    BYTE        HA_Unique[16];      // Host Adapter Unique parameters
    WORD        HA_ExtBuffer;       // Extended buffer area
} SRB_HAInquiry;

typedef struct
{
    BYTE        SRB_Cmd;            // ASPI command code = SC_GET_DEV_TYPE
    BYTE        SRB_Status;         // ASPI command status byte
    BYTE        SRB_HaId;           // ASPI host adapter number
    BYTE        SRB_Flags;          // ASPI request flags
    DWORD       SRB_Hdr_Rsvd;       // Reserved, MUST = 0
    BYTE        SRB_Target;         // Target's SCSI ID
    BYTE        SRB_Lun;            // Target's LUN number
    BYTE        SRB_DeviceType;     // Target's peripheral device type
    BYTE        SRB_Rsvd1;          // Reserved for alignment
} SRB_GDEVBlock;

typedef struct
{                                       // Structure for 6-byte CDBs
    BYTE        SRB_Cmd;                // ASPI command code = SC_EXEC_SCSI_CMD
    BYTE        SRB_Status;             // ASPI command status byte
    BYTE        SRB_HaId;               // ASPI host adapter number
    BYTE        SRB_Flags;              // ASPI request flags
    DWORD       SRB_Hdr_Rsvd;           // Reserved, MUST = 0
    BYTE        SRB_Target;             // Target's SCSI ID
    BYTE        SRB_Lun;                // Target's LUN number
    WORD        SRB_Rsvd1;              // Reserved for Alignment
    DWORD       SRB_BufLen;             // Data Allocation LengthPG
    BYTE        *SRB_BufPointer;        // Data Buffer Pointer
    BYTE        SRB_SenseLen;           // Sense Allocation Length
    BYTE        SRB_CDBLen;             // CDB Length = 6
    BYTE        SRB_HaStat;             // Host Adapter Status
    BYTE        SRB_TargStat;           // Target Status
    void        (*SRB_PostProc)();      // Post routine
    void        *SRB_Rsvd2;             // Reserved
    BYTE        SRB_Rsvd3[16];          // Reserved for expansion
    BYTE        CDBByte[16];            // SCSI CDB
    BYTE        SenseArea[SENSE_LEN+2]; // Request Sense buffer
} SRB_ExecSCSICmd6;

typedef struct
{                                       // Structure for 10-byte CDBs
    BYTE        SRB_Cmd;                // ASPI command code = SC_EXEC_SCSI_CMD
    BYTE        SRB_Status;             // ASPI command status byte
    BYTE        SRB_HaId;               // ASPI host adapter number
    BYTE        SRB_Flags;              // ASPI request flags
    DWORD       SRB_Hdr_Rsvd;           // Reserved, MUST = 0
    BYTE        SRB_Target;             // Target's SCSI ID
    BYTE        SRB_Lun;                // Target's LUN number
    WORD        SRB_Rsvd1;              // Reserved, MUST = 0
    DWORD       SRB_BufLen;             // Data Allocation Length
    BYTE        *SRB_BufPointer;        // Data Buffer Pointer
    BYTE        SRB_SenseLen;           // Sense Allocation Length
    BYTE        SRB_CDBLen;             // CDB Length = 10
    BYTE        SRB_HaStat;             // Host Adapter Status
    BYTE        SRB_TargStat;           // Target Status
    void        (*SRB_PostProc)();      // Post routine
    void        *SRB_Rsvd2;             // Reserved
    BYTE        SRB_Rsvd3[16];          // Reserved for expansion
    BYTE        CDBByte[16];            // SCSI CDB
    BYTE        SenseArea10[SENSE_LEN+2]; // Request Sense buffer
} SRB_ExecSCSICmd10;

typedef struct
{
    BYTE        SRB_Cmd;                // ASPI command code = SC_EXEC_SCSI_CMD
    BYTE        SRB_Status;             // ASPI command status byte
    BYTE        SRB_HaId;               // ASPI host adapter number
    BYTE        SRB_Flags;              // ASPI request flags
    DWORD       SRB_Hdr_Rsvd;           // Reserved, MUST = 0
    BYTE        SRB_Target;             // Target's SCSI ID
    BYTE        SRB_Lun;                // Target's LUN number
    WORD        SRB_Rsvd1;              // Reserved, MUST = 0
    DWORD       SRB_BufLen;             // Data Allocation Length
    BYTE        *SRB_BufPointer;        // Data Buffer Pointer
    BYTE        SRB_SenseLen;           // Sense Allocation Length
    BYTE        SRB_CDBLen;             // CDB Length = 10
    BYTE        SRB_HaStat;             // Host Adapter Status
    BYTE        SRB_TargStat;           // Target Status
    void        (*SRB_PostProc)();      // Post routine
    void        *SRB_Rsvd2;             // Reserved
    BYTE        SRB_Rsvd3[16];          // Reserved for expansion
    BYTE        CDBByte[16];            // SCSI CDB
    BYTE        SenseArea12[SENSE_LEN+2]; // Request Sense buffer
} SRB_ExecSCSICmd12;


typedef struct
{
    BYTE        SRB_Cmd;            // ASPI command code = SC_ABORT_SRB
    BYTE        SRB_Status;         // ASPI command status byte
    BYTE        SRB_HaId;           // ASPI host adapter number
    BYTE        SRB_Flags;          // ASPI request flags
    DWORD       SRB_Hdr_Rsvd;       // Reserved, MUST = 0
    void        *SRB_ToAbort;       // Pointer to SRB to abort
} SRB_Abort;

typedef struct
{
    BYTE        SRB_Cmd;            // ASPI command code = SC_RESET_DEV
    BYTE        SRB_Status;         // ASPI command status byte
    BYTE        SRB_HaId;           // ASPI host adapter number
    BYTE        SRB_Flags;          // ASPI request flags
    DWORD       SRB_Hdr_Rsvd;       // Reserved, MUST = 0
    BYTE        SRB_Target;         // Target's SCSI ID
    BYTE        SRB_Lun;            // Target's LUN number
    BYTE        SRB_ResetRsvd1[12]; // Reserved, MUST = 0
    BYTE        SRB_HaStat;         // Host Adapter Status
    BYTE        SRB_TargStat;       // Target Status
    void        *SRB_PostProc;      // Post routine
    void        *SRB_Rsvd2;         // Reserved
    BYTE        SRB_Rsvd3[32];      // Reserved
} SRB_BusDeviceReset;


typedef struct
{
  BYTE  res0;
  BYTE  TRACK_adr_contrl;
  BYTE  TRACK_nr;
  BYTE  res1;
  DWORD TRACK_abs_adr;
} TRACK;

typedef struct
{
    WORD        TOC_len; 
    BYTE        TOC_first;
    BYTE        TOC_last;
    TRACK       track[99];
} TOC;



void ASPIPostProc6  (SRB_ExecSCSICmd6  *DoneSRB);
void ASPIPostProc10 (SRB_ExecSCSICmd10 *DoneSRB);
void ASPIPostProc12 (SRB_ExecSCSICmd12 *DoneSRB);

