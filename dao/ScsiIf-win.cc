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
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "winaspi.h"
#include "ScsiIf.h"
#include "log.h"
#include "util.h"

#include "decodeSense.cc"


typedef DWORD (*GETSUPPORTINFO) (void);
typedef DWORD (*GETDLLVERSION)  (void);
typedef DWORD (*SENDCOMMAND)    (char *);

#define BUF_SIZE  (32*1024)

class ScsiIfImpl 
{
public:
  char *dev_;
  
  int maxSendLen_;

  SRB_ExecSCSICmd6    cmd6_;
  SRB_ExecSCSICmd10   cmd10_;
  SRB_ExecSCSICmd12   cmd12_;

  unsigned char senseBuffer_[SENSE_LEN];

  char haid_;
  char lun_;
  char scsi_id_;

  HINSTANCE   hinstlib;
  SENDCOMMAND Sendcommand;
};

ScsiIf::ScsiIf(const char *dev)
{
  char *p, *q;
 
  impl_ = new ScsiIfImpl;

  impl_->dev_ = strdupCC(dev);

  maxDataLen_ = BUF_SIZE;

  impl_->haid_ = 0;
  impl_->lun_ = 0;
  impl_->scsi_id_ = 0;

  if (p = strchr (impl_->dev_, ':'))
  {
     *p = 0;
     impl_->haid_ = atoi (impl_->dev_);
     q = ++p;

     if (p = strchr (q, ':'))
     {
        *p = 0;
        impl_->lun_ = atoi (q);
        q = ++p;
        impl_->scsi_id_ = atoi (q);
     }
  }

  impl_->hinstlib = 0;

  vendor_[0] = 0;
  product_[0] = 0;
  revision_[0] = 0;
}

ScsiIf::~ScsiIf()
{
  delete[] impl_->dev_;
  impl_->dev_ = NULL;

  if (impl_->hinstlib)
     FreeLibrary (impl_->hinstlib);

  delete impl_;
}

// opens and flushes scsi device
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed

int ScsiIf::init()
{
  impl_->hinstlib = LoadLibrary ("WNASPI32.DLL");

  if (!impl_->hinstlib)
  {
     log_message(-2, "Can't load WNASPI32.DLL");
     return 1;
  }
  else {
    impl_->Sendcommand = (SENDCOMMAND)GetProcAddress (impl_->hinstlib, "SendASPI32Command");
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

int ScsiIf::sendCmd (const unsigned char *cmd,     int cmdLen, 
		     const unsigned char *dataOut, int dataOutLen,
		     unsigned char *dataIn,  int dataInLen,
		     int showMessage)
{
  int status, i = 10;

  switch (cmdLen)
  {
      case 6: memset (&impl_->cmd6_, 0, sizeof (SRB_ExecSCSICmd6));
              memcpy (impl_->cmd6_.CDBByte, cmd, 6);

              impl_->cmd6_.SRB_Cmd        = SC_EXEC_SCSI_CMD;
              impl_->cmd6_.SRB_HaId       = impl_->haid_;
              impl_->cmd6_.SRB_Target     = impl_->scsi_id_;
              impl_->cmd6_.SRB_Lun        = impl_->lun_;
              impl_->cmd6_.SRB_SenseLen   = SENSE_LEN;
              impl_->cmd6_.SRB_CDBLen     = 6;

              if (dataOut && dataOutLen)
              {
                 impl_->cmd6_.SRB_Flags      = SRBF_WRITE;
                 impl_->cmd6_.SRB_BufPointer = (BYTE*)dataOut;
                 impl_->cmd6_.SRB_BufLen     = dataOutLen;
              }
              else
              {
                 impl_->cmd6_.SRB_Flags      = SRBF_READ;
                 impl_->cmd6_.SRB_BufPointer = dataIn;
                 impl_->cmd6_.SRB_BufLen     = dataInLen;
              }


              (impl_->Sendcommand) ((char *) &impl_->cmd6_);

              while (i-- && impl_->cmd6_.SRB_Status != SS_COMP)
              {
                   while (!impl_->cmd6_.SRB_Status)
                        Sleep (10);
              }

	      // AM: Try to get sense data
	      if (impl_->cmd6_.SRB_Status == SS_ERR &&
		  impl_->cmd6_.SRB_TargStat == STATUS_CHKCOND) {
		memcpy(impl_->senseBuffer_, impl_->cmd6_.SenseArea, SENSE_LEN);

		if (showMessage)
		  printError();

		return 2;
	      }

              if (impl_->cmd6_.SRB_Status != SS_COMP)
                 return (1);
              break; 

     case 10: memset (&impl_->cmd10_, 0, sizeof (SRB_ExecSCSICmd10));
              memcpy (impl_->cmd10_.CDBByte, cmd, 10);

              impl_->cmd10_.SRB_Cmd        = SC_EXEC_SCSI_CMD;
              impl_->cmd10_.SRB_HaId       = impl_->haid_;
              impl_->cmd10_.SRB_Target     = impl_->scsi_id_;
              impl_->cmd10_.SRB_Lun        = impl_->lun_;
              impl_->cmd10_.SRB_SenseLen   = SENSE_LEN;
              impl_->cmd10_.SRB_CDBLen     = 10;

              if (dataOut && dataOutLen)
              {
                 impl_->cmd10_.SRB_Flags      = SRBF_WRITE;
                 impl_->cmd10_.SRB_BufPointer = (BYTE*)dataOut;
                 impl_->cmd10_.SRB_BufLen     = dataOutLen;
              }
              else
              {
                 impl_->cmd10_.SRB_Flags      = SRBF_READ;
                 impl_->cmd10_.SRB_BufPointer = dataIn;
                 impl_->cmd10_.SRB_BufLen     = dataInLen;
              }

              
              (impl_->Sendcommand) ((char *) &impl_->cmd10_);

              while (i-- && impl_->cmd10_.SRB_Status != SS_COMP)
              {
                      while (!impl_->cmd10_.SRB_Status)
                          Sleep (10);
              }

	      // AM: Try to get sense data
	      if (impl_->cmd10_.SRB_Status == SS_ERR &&
		  impl_->cmd10_.SRB_TargStat == STATUS_CHKCOND) {
		memcpy(impl_->senseBuffer_, impl_->cmd10_.SenseArea10,
		       SENSE_LEN);

		if (showMessage)
		  printError();

		return 2;
	      }

              if (impl_->cmd10_.SRB_Status != SS_COMP)
                 return (1);

              break; 

     case 12: memset (&impl_->cmd12_, 0, sizeof (SRB_ExecSCSICmd12));
              memcpy (impl_->cmd12_.CDBByte, cmd, 12);

              impl_->cmd12_.SRB_Cmd        = SC_EXEC_SCSI_CMD;
              impl_->cmd12_.SRB_HaId       = impl_->haid_;
              impl_->cmd12_.SRB_Target     = impl_->scsi_id_;
              impl_->cmd12_.SRB_Lun        = impl_->lun_;
              impl_->cmd12_.SRB_SenseLen   = SENSE_LEN;
              impl_->cmd12_.SRB_CDBLen     = 12;

              if (dataOut && dataOutLen)
              {
                 impl_->cmd12_.SRB_Flags      = SRBF_WRITE;
                 impl_->cmd12_.SRB_BufPointer = (BYTE*)dataOut;
                 impl_->cmd12_.SRB_BufLen     = dataOutLen;
              }
              else
              {
                 impl_->cmd12_.SRB_Flags      = SRBF_READ;
                 impl_->cmd12_.SRB_BufPointer = dataIn;
                 impl_->cmd12_.SRB_BufLen     = dataInLen;
              }


              
              (impl_->Sendcommand) ((char *) &impl_->cmd12_);

              while (i-- && impl_->cmd12_.SRB_Status != SS_COMP)
              {
                   while (!impl_->cmd12_.SRB_Status)
                         Sleep (10);
              }

	      // AM: Try to get sense data
	      if (impl_->cmd12_.SRB_Status == SS_ERR &&
		  impl_->cmd12_.SRB_TargStat == STATUS_CHKCOND) {
		memcpy(impl_->senseBuffer_, impl_->cmd12_.SenseArea12,
		       SENSE_LEN);

		if (showMessage)
		  printError();

		return 2;
	      }

              if (impl_->cmd12_.SRB_Status != SS_COMP)
                 return (1);

              break; 
  }

  return 0;
}

const unsigned char *ScsiIf::getSense(int &len) const
{
  len = SENSE_LEN;
  return impl_->senseBuffer_;
}

void ScsiIf::printError()
{
  decodeSense(impl_->senseBuffer_, SENSE_LEN);
}

int ScsiIf::inquiry()
{
  unsigned char cmd[6];
  unsigned char result[0x2c];
  int i;


  cmd[0] = 0x12; // INQUIRY
  cmd[1] = cmd[2] = cmd[3] = 0;
  cmd[4] = 0x2c;
  cmd[5] = 0;

  if (sendCmd (cmd, 6, NULL, 0, result, 0x2c, 1) != 0) {
    log_message(-2, "Inquiry command failed on '%s': ", impl_->dev_);
    return 1;
  }

  strncpy(vendor_, (char *)(result + 0x08), 8);
  vendor_[8] = 0;

  strncpy(product_, (char *)(result + 0x10), 16);
  product_[16] = 0;

  strncpy(revision_, (char *)(result + 0x20), 4);
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
