/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "ScsiIf.h"
#include "log.h"

#include "ntddcdrm.h"

#include "decodeSense.cc"

//
// SCSI Definitionen.
//

#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_GET_CAPABILITIES     CTL_CODE(IOCTL_SCSI_BASE, 0x0404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_ADDRESS          CTL_CODE(IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS)


#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2

#pragma pack(4)

typedef struct _IO_SCSI_CAPABILITIES 
{
    ULONG Length;
    ULONG MaximumTransferLength;
    ULONG MaximumPhysicalPages;
    ULONG SupportedAsynchronousEvents;
    ULONG AlignmentMask;
    BOOLEAN TaggedQueuing;
    BOOLEAN AdapterScansDown;
    BOOLEAN AdapterUsesPio;
} IO_SCSI_CAPABILITIES, *PIO_SCSI_CAPABILITIES;

typedef struct _SCSI_PASS_THROUGH_DIRECT 
{
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;


typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER 
{
  SCSI_PASS_THROUGH_DIRECT sptd;
  ULONG Filler;      // realign buffer to double word boundary
  UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

typedef struct _SCSI_INQUIRY_DEVICE 
{
  UCHAR Type;
  UCHAR TypeModifier;
  UCHAR Version;
  UCHAR Format;
  UCHAR AddLength; // n-4
  UCHAR Reserved[2];
  UCHAR Flags;
  char  VendorId[8];
  char  ProductId[16];
  char  ProductRevLevel[4];
  char  ProductRevDate[8];
} SCSI_INQUIRY_DEVICE, *PSCSI_INQUIRY_DEVICE;


typedef struct _SCSI_ADDRESS {
    ULONG Length;
    UCHAR PortNumber;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
}SCSI_ADDRESS, *PSCSI_ADDRESS;


#pragma pack(1)

//
// SCSI CDB operation codes
//

#define SCSIOP_INQUIRY             0x12
#define SCSIOP_MODE_SELECT         0x15
#define SCSIOP_MODE_SENSE          0x1A

#define AUDIO_BLOCK_LEN 2352
#define BUF_SIZE  (63*1024)

class ScsiIfImpl 
{
public:
  char *dev_;

  HANDLE hCD;
  SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sb;
  
  unsigned char senseBuffer_[32];

  char haid_;
  char lun_;
  char scsi_id_;
};

ScsiIf::ScsiIf(const char *dev)
{
  impl_ = new ScsiIfImpl;

  impl_->dev_ = strdup3CC("\\\\.\\", dev, NULL);

  impl_->hCD = INVALID_HANDLE_VALUE;

  vendor_[0] = 0;
  product_[0] = 0;
  revision_[0] = 0;
}

ScsiIf::~ScsiIf()
{
  if (impl_->hCD != INVALID_HANDLE_VALUE)
     CloseHandle (impl_->hCD);

  delete impl_;
}

// opens scsi device
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed

int ScsiIf::init()
{
  int i = 0;
  DWORD ol;

  SCSI_ADDRESS sa;
  IO_SCSI_CAPABILITIES ca;

  while (i++ < 3 && (impl_->hCD == INVALID_HANDLE_VALUE))
  {
    impl_->hCD = CreateFile (impl_->dev_, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  }

  if (impl_->hCD == INVALID_HANDLE_VALUE) {
    return 1;
  }

  if (DeviceIoControl (impl_->hCD, IOCTL_SCSI_GET_ADDRESS, NULL, 0, &sa, sizeof(SCSI_ADDRESS), &ol, NULL)) 
  { 
    impl_->haid_    = sa.PortNumber;
    impl_->lun_     = sa.Lun;
    impl_->scsi_id_ = sa.TargetId;
  }
  else
  {
    CloseHandle (impl_->hCD);
    impl_->hCD = INVALID_HANDLE_VALUE;
    return 1;
  }

  if (DeviceIoControl (impl_->hCD, IOCTL_SCSI_GET_CAPABILITIES, NULL, 0, &ca, sizeof(IO_SCSI_CAPABILITIES), &ol, NULL)) 
  {
    maxDataLen_ = ca.MaximumTransferLength;
  }
  else
  {
    CloseHandle (impl_->hCD);
    impl_->hCD = INVALID_HANDLE_VALUE;
    return 1;
  }

  if (inquiry() != 0) 
    return 2;

  return 0;
}

// Sets given timeout value in seconds and returns old timeout.
// return: old timeout

int ScsiIf::timeout (int t)
{
  return 0;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)

int ScsiIf::sendCmd (unsigned char *cmd,     int cmdLen, 
		     unsigned char *dataOut, int dataOutLen,
		     unsigned char *dataIn,  int dataInLen,
		     int showMessage)
{
  int i = 10;

  DWORD er, il, ol;

  ZeroMemory (&impl_->sb, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));

  impl_->sb.sptd.Length             = sizeof(SCSI_PASS_THROUGH_DIRECT);
  impl_->sb.sptd.PathId             = 0;
  impl_->sb.sptd.TargetId           = impl_->scsi_id_;
  impl_->sb.sptd.Lun                = impl_->lun_;
  impl_->sb.sptd.CdbLength          = cmdLen;
  impl_->sb.sptd.SenseInfoLength    = 32;
  impl_->sb.sptd.TimeOutValue       = 4;
  impl_->sb.sptd.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
  memcpy (impl_->sb.sptd.Cdb, cmd, cmdLen);

  if (dataOut && dataOutLen)
  {
     impl_->sb.sptd.DataIn              = SCSI_IOCTL_DATA_OUT;
     impl_->sb.sptd.DataBuffer          = dataOut;
     impl_->sb.sptd.DataTransferLength  = dataOutLen;
  }
  else
  {
     impl_->sb.sptd.DataIn              = SCSI_IOCTL_DATA_IN;
     impl_->sb.sptd.DataBuffer          = dataIn;
     impl_->sb.sptd.DataTransferLength  = dataInLen;
  }

  il = sizeof (SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);

  // AM: what about sense data?
  if (DeviceIoControl (impl_->hCD, IOCTL_SCSI_PASS_THROUGH_DIRECT, &impl_->sb, il, &impl_->sb, il, &ol, NULL)) 
  {
      er = impl_->sb.sptd.ScsiStatus ? impl_->sb.sptd.ScsiStatus | 0x20000000 : 0;

      if (!er)
         return (0); 
  }
  else 
     return (1);
  
  return 0;
}

const unsigned char *ScsiIf::getSense(int &len) const
{
  len = 32;
  return impl_->sb.ucSenseBuf;
}

void ScsiIf::printError()
{
  decodeSense(impl_->sb.ucSenseBuf, 32);
}

int ScsiIf::inquiry()
{
  unsigned char cmd[6];
  int i;

  SCSI_INQUIRY_DEVICE NTinqbuf;

  ZeroMemory (&NTinqbuf, sizeof(SCSI_INQUIRY_DEVICE));

  cmd[0] = 0x12; // INQUIRY
  cmd[1] = cmd[2] = cmd[3] = 0;
  cmd[4] = sizeof (NTinqbuf);
  cmd[5] = 0;

  if (sendCmd (cmd, 6, NULL, 0,
	       (unsigned char *) &NTinqbuf, sizeof (NTinqbuf), 1) != 0) {
    log_message(-2, "Inquiry command failed on '%s': ", impl_->dev_);
    return 1;
  }

  strncpy(vendor_, (char *)(NTinqbuf.VendorId), 8);
  vendor_[8] = 0;

  strncpy(product_, (char *)(NTinqbuf.ProductId), 16);
  product_[16] = 0;

  strncpy(revision_, (char *)(NTinqbuf.ProductRevLevel), 4);
  revision_[4] = 0;

  for (i = 7; i >= 0 && vendor_[i] == ' '; i--) 
  {
     vendor_[i] = 0;
  }

  for (i = 15; i >= 0 && product_[i] == ' '; i--) 
  {
     product_[i] = 0;
  }

  for (i = 3; i >= 0 && revision_[i] == ' '; i--) 
  {
     revision_[i] = 0;
  }

  return 0;
}


