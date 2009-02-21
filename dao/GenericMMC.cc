/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002 Andreas Mueller <andreas@daneb.de>
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

#include <config.h>

#include <string.h>
#include <time.h>
#include <assert.h>

#include "GenericMMC.h"

#include "port.h"
#include "Toc.h"
#include "log.h"
#include "PQSubChannel16.h"
#include "PWSubChannel96.h"
#include "CdTextEncoder.h"



// Variants for cue sheet generation

// do not set ctl flags for ISRC cue sheet entries
#define CUE_VAR_ISRC_NO_CTL 0x1

// do not set start of lead-in time in lead-in cue sheet entry
#define CUE_VAR_CDTEXT_NO_TIME 0x2

#define CUE_VAR_MAX 0x3 


// Variants for write parameters mode page

//do not set data block type to block size 2448 if a CD-TEXT lead-in is written
#define WMP_VAR_CDTEXT_NO_DATA_BLOCK_TYPE 0x1

#define WMP_VAR_MAX 0x1


GenericMMC::GenericMMC(ScsiIf *scsiIf, unsigned long options)
  : CdrDriver(scsiIf, options)
{
  int i;
  driverName_ = "Generic SCSI-3/MMC - Version 2.0";
  
  speed_ = 0;
  rspeed_ = 0;
  simulate_ = true;
  encodingMode_ = 1;

  scsiTimeout_ = 0;
  
  cdTextEncoder_ = NULL;

  driveInfo_ = NULL;

  memset(&diskInfo_, 0, sizeof(DiskInfo));

  for (i = 0; i < maxScannedSubChannels_; i++) {
    if (options_ & OPT_MMC_USE_PQ) 
      scannedSubChannels_[i] = new PQSubChannel16;
    else
      scannedSubChannels_[i] = new PWSubChannel96;
  }

  // MMC drives usually return little endian samples
  audioDataByteOrder_ = 0;
}

GenericMMC::~GenericMMC()
{
  int i;

  for (i = 0; i < maxScannedSubChannels_; i++) {
    delete scannedSubChannels_[i];
    scannedSubChannels_[i] = NULL;
  }

  delete cdTextEncoder_;
  cdTextEncoder_ = NULL;

  delete driveInfo_;
  driveInfo_ = NULL;
}

// static constructor
CdrDriver *GenericMMC::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new GenericMMC(scsiIf, options);
}


int GenericMMC::checkToc(const Toc *toc)
{
  int err = CdrDriver::checkToc(toc);
  int e;

  if (options_ & OPT_MMC_CD_TEXT) {
    if ((e = toc->checkCdTextData()) > err)
      err = e;
  }

  return err;
}

int GenericMMC::subChannelEncodingMode(TrackData::SubChannelMode sm) const
{
  int ret = 0;

  switch (sm) {
  case TrackData::SUBCHAN_NONE:
    ret = 0;
    break;

  case TrackData::SUBCHAN_RW:
#if 0
    if (options_ & OPT_MMC_NO_RW_PACKED) 
      ret = 1; // have to encode the R-W sub-channel data
    else 
      ret = 0;
#endif
    ret = 0;
    break;

  case TrackData::SUBCHAN_RW_RAW:
    // raw R-W sub-channel writing is assumed to be always supported
    ret = 1;
    break;
  }

  return ret;
}

// sets speed
// return: 0: OK
//         1: illegal speed
int GenericMMC::speed(int s)
{
  speed_ = s;

  if (selectSpeed() != 0)
    return 1;
  
  return 0;
}

int GenericMMC::speed()
{
  const DriveInfo *di;

  delete driveInfo_;
  driveInfo_ = NULL;

  if ((di = driveInfo(true)) == NULL) {
    return 0;
  }

  return speed2Mult(di->currentWriteSpeed);

}

// sets fspeed
// return: true: OK
//         false: illegal speed
bool GenericMMC::rspeed(int s)
{
  rspeed_ = s;

  if (selectSpeed() != 0)
    return false;
  
  return true;
}

int GenericMMC::rspeed()
{
  const DriveInfo *di;

  delete driveInfo_;
  driveInfo_ = NULL;

  if ((di = driveInfo(true)) == NULL) {
    return 0;
  }

  return speed2Mult(di->currentReadSpeed);
}


// loads ('unload' == 0) or ejects ('unload' == 1) tray
// return: 0: OK
//         1: scsi command failed
int GenericMMC::loadUnload(int unload) const
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x1b; // START/STOP UNIT
  if (unload) {
    cmd[4] = 0x02; // LoUnlo=1, Start=0
  }
  else {
    cmd[4] = 0x03; // LoUnlo=1, Start=1
  }
  
  if (sendCmd(cmd, 6, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot load/unload medium.");
    return 1;
  }

  return 0;
}

// Checks for ready status of the drive after a write operation
// Return: 0: drive ready
//         1: error occured
//         2: drive not ready

int GenericMMC::checkDriveReady() const
{
  unsigned char cmd[10];
  unsigned char data[4];
  int ret;

  ret = testUnitReady(0);

  if (ret == 0) {
    // testUnitReady reports ready status but this might actually not
    // be the truth -> additionally check the READ DISK INFO command 
    
    memset(cmd, 0, 10);

    cmd[0] = 0x51; // READ DISK INFORMATION
    cmd[8] = 4;
    
    ret = sendCmd(cmd, 10, NULL, 0, data, 4, 0);
    
    if (ret == 2) {
      const unsigned char *sense;
      int senseLen;
      
      ret = 0; // indicates ready status

      sense = scsiIf_->getSense(senseLen);
      
      if (senseLen >= 14 && (sense[2] & 0x0f) == 0x2 && sense[7] >= 6 &&
          sense[12] == 0x4 && 
          (sense[13] == 0x8 || sense[13] == 0x7)) {
        // Not Ready, long write in progress
        ret = 2; // indicates not ready status
      }
    }
  }

  return ret;
}

// Performs complete blanking of a CD-RW.
// return: 0: OK
//         1: scsi command failed
int GenericMMC::blankDisk(BlankingMode mode)
{
  unsigned char cmd[12];
  int ret, progress;
  time_t startTime, endTime;

  setSimulationMode(0);


  memset(cmd, 0, 12);

  cmd[0] = 0xa1; // BLANK

  switch (mode) {
  case BLANK_FULL:
    cmd[1] = 0x0; // erase complete disk
    break;
  case BLANK_MINIMAL:
    cmd[1] = 0x1; // erase PMA, lead-in and 1st track's pre-gap
    break;
  }

  cmd[1] |= 1 << 4; // immediate return

  sendBlankCdProgressMsg(0);

  if (sendCmd(cmd, 12, NULL, 0, NULL, 0, 1) != 0) {
    log_message(-2, "Cannot erase CD-RW.");
    return 1;
  }

  time(&startTime);
  progress = 0;

  do {
    mSleep(2000);

    ret = checkDriveReady();

    if (ret == 1) {
      log_message(-2, "Test Unit Ready command failed.");
    }

    progress += 10;
    sendBlankCdProgressMsg(progress);

    if (progress >= 1000)
      progress = 0;

  } while (ret == 2);

  if (ret == 0)
    sendBlankCdProgressMsg(1000);

  time(&endTime);

  log_message(2, "Blanking time: %ld seconds", endTime - startTime);

  return ret;
}

// sets read/write speed and simulation mode
// return: 0: OK
//         1: scsi command failed
int GenericMMC::selectSpeed()
{
  unsigned char cmd[12];
  int spd;

  memset(cmd, 0, 12);

  cmd[0] = 0xbb; // SET CD SPEED

  // select maximum read speed
  if (rspeed_ == 0) {
    cmd[2] = 0xff;
    cmd[3] = 0xff;
  }
  else {
    spd = mult2Speed(rspeed_);
    cmd[2] = spd >> 8;
    cmd[3] = spd;
  }

  // select maximum write speed
  if (speed_ == 0) {
    cmd[4] = 0xff;
    cmd[5] = 0xff;
  }
  else {
    spd = mult2Speed(speed_);
    cmd[4] = spd >> 8;
    cmd[5] = spd;
  }

  if ((options_ & OPT_MMC_YAMAHA_FORCE_SPEED) != 0 &&
      writeSpeedControl())
    cmd[11] = 0x80; // enable Yamaha's force speed

  if (sendCmd(cmd, 12, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot set cd speed.");
    return 1;
  }

  return 0;
}

// Determins start and length of lead-in and length of lead-out.
// return: 0: OK
//         1: SCSI command failed
int GenericMMC::getSessionInfo()
{
  unsigned char cmd[10];
  unsigned long dataLen = 34;
  unsigned char data[34];

  memset(cmd, 0, 10);
  memset(data, 0, dataLen);

  cmd[0] = 0x51; // READ DISK INFORMATION
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot retrieve disk information.");
    return 1;
  }

  leadInStart_ = Msf(data[17], data[18], data[19]);

  if (leadInStart_.lba() >= Msf(80, 0, 0).lba()) {
    leadInLen_ = 450000 - leadInStart_.lba();

    if (fullBurn_) {
    	leadOutLen_ = (userCapacity_ ? Msf(userCapacity_, 0, 0).lba() : diskInfo_.capacity) + Msf(1, 30, 0).lba() - toc_->length().lba() - diskInfo_.thisSessionLba - 150; // Fill all rest space <vladux>
	if (leadOutLen_ < Msf(1, 30, 0).lba()) {
		leadOutLen_ = Msf(1, 30, 0).lba(); // 90 seconds lead-out
	}
    } else {
    	leadOutLen_ = Msf(1, 30, 0).lba(); // 90 seconds lead-out
    }
  }
  else {
    leadInLen_ = Msf(1, 0, 0).lba();
    leadOutLen_ = Msf(0, 30, 0).lba();
  }


  log_message(4, "Lead-in start: %s length: %ld", leadInStart_.str(),
	  leadInLen_);
  log_message(4, "Lead-out length: %ld", leadOutLen_);

  return 0;
}

bool GenericMMC::readBufferCapacity(long *capacity, long *available)
{
  unsigned char cmd[10];
  unsigned char data[12];
  long bufsize;

  memset(cmd, 0, 10);
  memset(data, 0, 12);

  cmd[0] = 0x5c; // READ BUFFER CAPACITY
  cmd[8] = 12;

  if (sendCmd(cmd, 10, NULL, 0, data, 12) != 0) {
    log_message(-2, "Read buffer capacity failed.");
    return false;
  }

  *capacity = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
  *available = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];

  return true;
}

int GenericMMC::performPowerCalibration()
{
  unsigned char cmd[10];
  int ret;
  long old_timeout;

  memset(cmd, 0, 10);

  cmd[0] = 0x54; // SEND OPC INFORMATION
  cmd[1] = 1;

  log_message(2, "Executing power calibration...");

  old_timeout = scsiIf_->timeout(30);
  ret = sendCmd(cmd, 10, NULL, 0, NULL, 0);
  (void)scsiIf_->timeout(old_timeout);

  if (ret == 0) {
    log_message(2, "Power calibration successful.");
    return 0;
  }
  if (ret == 2) {
    const unsigned char *sense;
    int senseLen;

    sense = scsiIf_->getSense(senseLen);

    if (senseLen >= 14 && (sense[2] & 0x0f) == 0x5 && sense[7] >= 6 &&
      sense[12] == 0x20 && sense[13] == 0x0) {
      log_message(2, "Power calibration not supported.");
      return 0;
    } /* else fall trough */
  }
  log_message(-2, "Power calibration failed.");
  return 1;
}

// Sets write parameters via mode page 0x05.
// return: 0: OK
//         1: scsi command failed
int GenericMMC::setWriteParameters(unsigned long variant)
{
  unsigned char mp[0x38];
  unsigned char mpHeader[8];
  unsigned char blockDesc[8];

  if (getModePage(5/*write parameters mode page*/, mp, 0x38,
		  mpHeader, blockDesc, 1) != 0) {
    log_message(-2, "Cannot retrieve write parameters mode page.");
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag

  mp[2] &= 0xe0;
  mp[2] |= 0x02; // write type: Session-at-once
  if (simulate_) {
    mp[2] |= 1 << 4; // test write
  }

  const DriveInfo *di;
  if ((di = driveInfo(true)) != NULL) {
    if (di->burnProof) {
      // This drive has BURN-Proof function.
      // Enable it unless explicitly disabled.
      if (bufferUnderRunProtection()) {
	log_message(2, "Turning BURN-Proof on");
	mp[2] |= 0x40;
      }
      else {
	log_message(2, "Turning BURN-Proof off");
	mp[2] &= ~0x40;
      }
    }

    RicohSetWriteOptions(di);
  }

  mp[3] &= 0x3f; // Multi-session: No B0 pointer, next session not allowed

  if (multiSession_)
    mp[3] |= 0x03 << 6; // open next session
  else if (!diskInfo_.empty)
    mp[3] |= 0x01 << 6; // use B0=FF:FF:FF when closing last session of a
                        // multi session CD-R

  log_message(4, "Multi session mode: %d", mp[3] >> 6);

  mp[4] &= 0xf0; // Data Block Type: raw data, block size: 2352 (I think not
                 // used for session at once writing)

  if (cdTextEncoder_ != NULL) {
    if ((variant & WMP_VAR_CDTEXT_NO_DATA_BLOCK_TYPE) == 0)
      mp[4] |= 3; /* Data Block Type: raw data with raw P-W sub-channel data,
		     block size 2448, required for CD-TEXT lead-in writing
		     according to the SCSI/MMC-3 manual
		  */
  }

  log_message(4, "Data block type: %u",  mp[4] & 0x0f);

  mp[8] = sessionFormat();

  if (!diskInfo_.empty) {
    // use the toc type of the already recorded sessions
    switch (diskInfo_.diskTocType) {
    case 0x00:
    case 0x10:
    case 0x20:
      mp[8] = diskInfo_.diskTocType;
      break;
    }
  }

  log_message(4, "Toc type: 0x%x", mp[8]);

  if (setModePage(mp, mpHeader, NULL, 0) != 0) {
    log_message(-2, "Cannot set write parameters mode page.");
    return 1;
  }

  return 0;
}

// Sets simulation mode via mode page 0x05.
// return: 0: OK
//         1: scsi command failed
int GenericMMC::setSimulationMode(int showMessage)
{
  unsigned char mp[0x38];
  unsigned char mpHeader[8];

  if (getModePage(5/*write parameters mode page*/, mp, 0x38,
		  mpHeader, NULL, showMessage) != 0) {
    if (showMessage)
      log_message(-2, "Cannot retrieve write parameters mode page.");
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag

  if (simulate_)
    mp[2] |= 1 << 4; // test write
  else
    mp[2] &= ~(1 << 4);

  if (setModePage(mp, mpHeader, NULL, showMessage) != 0) {
    if (showMessage)
      log_message(-2, "Cannot set write parameters mode page.");
    return 1;
  }

  return 0;
}

int GenericMMC::getNWA(long *nwa)
{
  unsigned char cmd[10];
  int infoblocklen = 16;
  unsigned char info[16];
  long lba = 0;

  cmd[0] = 0x52; // READ TRACK INFORMATION
  cmd[1] = 0x01; // track instead of lba designation
  cmd[2] = 0x00;
  cmd[3] = 0x00;
  cmd[4] = 0x00;
  cmd[5] = 0xff; // invisible track
  cmd[6] = 0x00; // reserved
  cmd[7] = infoblocklen << 8;
  cmd[8] = infoblocklen; // alloc length
  cmd[9] = 0x00; // Control Byte

  if (sendCmd(cmd, 10, NULL, 0, info, infoblocklen) != 0) {
    log_message(-2, "Cannot get Track Information Block.");
    return 1;
  }

#if 0
  log_message(3,"Track Information Block");
  for (int i=0;i<infoblocklen;i++) log_message(3,"byte %02x : %02x",i,info[i]);
#endif

  if ((info[6] & 0x40) && (info[7] & 0x01) && !(info[6] & 0xb0))
  {
      log_message(4,"Track is Blank, Next Writable Address is valid");
      lba |= info[12] << 24; // MSB of LBA
      lba |= info[13] << 16;
      lba |= info[14] << 8;
      lba |= info[15];       // LSB of LBA
  }

  log_message(4, "NWA: %ld", lba);

  if (nwa != NULL) 
    *nwa = lba;

  return 0;
}

// Determines first writable address of data area of current empty session. 
// lba: set to start of data area
// return: 0: OK
//         1: error occured
int GenericMMC::getStartOfSession(long *lba)
{
  unsigned char mp[0x38];
  unsigned char mpHeader[8];

  // first set the writing mode because it influences which address is
  // returned with 'READ TRACK INFORMATION'

  if (getModePage(5/*write parameters mode page*/, mp, 0x38,
		  mpHeader, NULL, 0) != 0) {
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag

  mp[2] &= 0xe0;
  mp[2] |= 0x02; // write type: Session-at-once

  if (setModePage(mp, mpHeader, NULL, 1) != 0) {
    log_message(-2, "Cannot set write parameters mode page.");
    return 1;
  }

  return getNWA(lba);
}

static unsigned char leadInOutDataMode(TrackData::Mode mode)
{
  unsigned char ret = 0;

  switch (mode) {
  case TrackData::AUDIO:
    ret = 0x01;
    break;

  case TrackData::MODE0: // should not happen
  case TrackData::MODE1:
  case TrackData::MODE1_RAW:
    ret = 0x14;
    break;

  case TrackData::MODE2_FORM1:
  case TrackData::MODE2_FORM2:
  case TrackData::MODE2_FORM_MIX:
  case TrackData::MODE2_RAW: // assume it contains XA sectors
    ret = 0x24;
    break;

  case TrackData::MODE2:
    ret = 0x34;
    break;
  }

  return ret;
}


unsigned char GenericMMC::subChannelDataForm(TrackData::SubChannelMode sm,
					     int encodingMode)
{
  unsigned char ret = 0;

  switch (sm) {
  case TrackData::SUBCHAN_NONE:
    break;

  case TrackData::SUBCHAN_RW:
  case TrackData::SUBCHAN_RW_RAW:
    if (encodingMode == 0)
      ret = 0xc0;
    else
      ret = 0x40;
    break;
  }

  return ret;
}
		  

// Creates cue sheet for current toc.
// cueSheetLen: filled with length of cue sheet in bytes
// return: newly allocated cue sheet buffer or 'NULL' on error
unsigned char *GenericMMC::createCueSheet(unsigned long variant,
					  long *cueSheetLen)
{
  const Track *t;
  int trackNr;
  Msf start, end, index;
  unsigned char *cueSheet;
  long len = 3; // entries for lead-in, gap, lead-out
  long n; // index into cue sheet
  unsigned char ctl; // control nibbles of cue sheet entry CTL/ADR
  long i;
  unsigned char dataMode;
  int trackOffset;
  long lbaOffset;

  if (!diskInfo_.empty && diskInfo_.append) {
    if (toc_->firstTrackNo() != 0 && toc_->firstTrackNo() != diskInfo_.lastTrackNr + 1) {
      log_message(-2, "Number of first track doesn't match");
      return NULL;
    }
#if 0
    /* for progress message */
    toc_->firstTrackNo(diskInfo_.lastTrackNr + 1);
#endif
    trackOffset = diskInfo_.lastTrackNr;
    lbaOffset = diskInfo_.thisSessionLba;
  } else {
    trackOffset = toc_->firstTrackNo() == 0 ? 0 : toc_->firstTrackNo() - 1;
    lbaOffset = 0;
  }
  if (trackOffset + toc_->nofTracks() > 99) {
    log_message(-2, "Track numbers too large");
    return NULL;
  }

  TrackIterator itr(toc_);

  if (itr.first(start, end) == NULL) {
    return NULL;
  }

  if (toc_->catalogValid()) {
    len += 2;
  }

  for (t = itr.first(start, end), trackNr = 1;
       t != NULL; t = itr.next(start, end), trackNr++) {
    len += 1; // entry for track
    if (t->start().lba() != 0 && trackNr > 1) {
      len += 1; // entry for pre-gap
    }
    if (t->type() == TrackData::AUDIO && t->isrcValid()) {
      len += 2; // entries for ISRC code
    }
    len += t->nofIndices(); // entry for each index increment
  }

  cueSheet = new unsigned char[len * 8];
  n = 0;

  if (toc_->leadInMode() == TrackData::AUDIO) {
    ctl = 0;
  }
  else {
    ctl = 0x40;
  }

  if (toc_->catalogValid()) {
    // fill catalog number entry
    cueSheet[n*8] = 0x02 | ctl;
    cueSheet[(n+1)*8] = 0x02 | ctl;
    for (i = 1; i <= 13; i++) {
      if (i < 8) {
	cueSheet[n*8 + i] = toc_->catalog(i-1) + '0';
      }
      else {
	cueSheet[(n+1)*8 + i - 7] = toc_->catalog(i-1) + '0';
      }
    }
    cueSheet[(n+1)*8+7] = 0;
    n += 2;
  }

  // entry for lead-in
  cueSheet[n*8] = 0x01 | ctl; // CTL/ADR
  cueSheet[n*8+1] = 0;    // Track number
  cueSheet[n*8+2] = 0;    // Index

  if (cdTextEncoder_ != NULL) {
    cueSheet[n*8+3] = 0x41; // Data Form: CD-DA with P-W sub-channels,
                            // main channel data generated by device
  }
  else {
    cueSheet[n*8+3] = leadInOutDataMode(toc_->leadInMode());
  }

  cueSheet[n*8+4] = 0;    // Serial Copy Management System

  if (cdTextEncoder_ != NULL &&
      (variant & CUE_VAR_CDTEXT_NO_TIME) == 0) {
    cueSheet[n*8+5] = leadInStart_.min();
    cueSheet[n*8+6] = leadInStart_.sec();
    cueSheet[n*8+7] = leadInStart_.frac();
  }
  else {
    cueSheet[n*8+5] = 0;    // MIN
    cueSheet[n*8+6] = 0;    // SEC
    cueSheet[n*8+7] = 0;    // FRAME
  }

  n++;

  int firstTrack = 1;

  for (t = itr.first(start, end), trackNr = trackOffset + 1;
       t != NULL;
       t = itr.next(start, end), trackNr++) {
    if (encodingMode_ == 0) {
      // just used for some experiments with raw writing
      dataMode = 0;
    }
    else {
      switch (t->type()) {
      case TrackData::AUDIO:
	dataMode = 0;
	break;
      case TrackData::MODE1:
      case TrackData::MODE1_RAW:
	dataMode = 0x10;
	break;
      case TrackData::MODE2:
	dataMode = 0x30;
	break;
      case TrackData::MODE2_RAW: // assume it contains XA sectors
      case TrackData::MODE2_FORM1:
      case TrackData::MODE2_FORM2:
      case TrackData::MODE2_FORM_MIX:
	dataMode = 0x20;
	break;
      default:
	dataMode = 0;
	break;
      }
    }

    // add mode for sub-channel writing
    dataMode |= subChannelDataForm(t->subChannelType(),
				   subChannelEncodingMode(t->subChannelType()));

    ctl = 0;
    if (t->copyPermitted()) {
      ctl |= 0x20;
    }

    if (t->type() == TrackData::AUDIO) {
      // audio track
      if (t->preEmphasis()) {
	ctl |= 0x10;
      }
      if (t->audioType() == 1) {
	ctl |= 0x80;
      }

      if (t->isrcValid()) {
	if ((variant & CUE_VAR_ISRC_NO_CTL) == 0)
	  cueSheet[n*8] = ctl | 0x03;
	else
	  cueSheet[n*8] = 0x03;

	cueSheet[n*8+1] = trackNr;
	cueSheet[n*8+2] = t->isrcCountry(0);
	cueSheet[n*8+3] = t->isrcCountry(1);
	cueSheet[n*8+4] = t->isrcOwner(0);  
	cueSheet[n*8+5] = t->isrcOwner(1);   
	cueSheet[n*8+6] = t->isrcOwner(2);  
	cueSheet[n*8+7] = t->isrcYear(0) + '0';
	n++;

	if ((variant & CUE_VAR_ISRC_NO_CTL) == 0)
	  cueSheet[n*8] = ctl | 0x03;
	else
	  cueSheet[n*8] = 0x03;

	cueSheet[n*8+1] = trackNr;
	cueSheet[n*8+2] = t->isrcYear(1) + '0';
	cueSheet[n*8+3] = t->isrcSerial(0) + '0';
	cueSheet[n*8+4] = t->isrcSerial(1) + '0';
	cueSheet[n*8+5] = t->isrcSerial(2) + '0';
	cueSheet[n*8+6] = t->isrcSerial(3) + '0';
	cueSheet[n*8+7] = t->isrcSerial(4) + '0';
	n++;
      }
    }
    else {
      // data track
      ctl |= 0x40;
    }
	 

    if (firstTrack) {
      Msf sessionStart(lbaOffset);

      // entry for gap before first track
      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = trackNr;
      cueSheet[n*8+2] = 0;    // Index 0
      cueSheet[n*8+3] = dataMode;    // Data Form
      cueSheet[n*8+4] = 0;    // Serial Copy Management System
      cueSheet[n*8+5] = sessionStart.min();
      cueSheet[n*8+6] = sessionStart.sec();
      cueSheet[n*8+7] = sessionStart.frac();
      n++;
    }
    else if (t->start().lba() != 0) {
      // entry for pre-gap
      Msf pstart(lbaOffset + start.lba() - t->start().lba() + 150);

      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = trackNr;
      cueSheet[n*8+2] = 0; // Index 0 indicates pre-gap
      cueSheet[n*8+3] = dataMode; // Data Form
      cueSheet[n*8+4] = 0; // no alternate copy bit
      cueSheet[n*8+5] = pstart.min();
      cueSheet[n*8+6] = pstart.sec();
      cueSheet[n*8+7] = pstart.frac();
      n++;
    }

    Msf tstart(lbaOffset + start.lba() + 150);
    
    cueSheet[n*8]   = ctl | 0x01;
    cueSheet[n*8+1] = trackNr;
    cueSheet[n*8+2] = 1; // Index 1
    cueSheet[n*8+3] = dataMode; // Data Form
    cueSheet[n*8+4] = 0; // no alternate copy bit
    cueSheet[n*8+5] = tstart.min();
    cueSheet[n*8+6] = tstart.sec();
    cueSheet[n*8+7] = tstart.frac();
    n++;

    for (i = 0; i < t->nofIndices(); i++) {
      index = tstart + t->getIndex(i);
      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = trackNr;
      cueSheet[n*8+2] = i+2; // Index
      cueSheet[n*8+3] = dataMode; // Data Form
      cueSheet[n*8+4] = 0; // no alternate copy bit
      cueSheet[n*8+5] = index.min();
      cueSheet[n*8+6] = index.sec();
      cueSheet[n*8+7] = index.frac();
      n++;
    }

    firstTrack = 0;
  }

  assert(n == len - 1);

  // entry for lead out
  Msf lostart(lbaOffset + toc_->length().lba() + 150);
  ctl = toc_->leadOutMode() == TrackData::AUDIO ? 0 : 0x40;
    
  cueSheet[n*8]   = ctl | 0x01;
  cueSheet[n*8+1] = 0xaa;
  cueSheet[n*8+2] = 1; // Index 1
  cueSheet[n*8+3] = leadInOutDataMode(toc_->leadOutMode());
  cueSheet[n*8+4] = 0; // no alternate copy bit
  cueSheet[n*8+5] = lostart.min();
  cueSheet[n*8+6] = lostart.sec();
  cueSheet[n*8+7] = lostart.frac();

  log_message(3, "\nCue Sheet (variant %lx):", variant);
  log_message(3, "CTL/  TNO  INDEX  DATA  SCMS  MIN  SEC  FRAME");
  log_message(3, "ADR               FORM");
  for (n = 0; n < len; n++) {
    log_message(3, "%02x    %02x    %02x     %02x    %02x   %02d   %02d   %02d",
	   cueSheet[n*8],
	   cueSheet[n*8+1], cueSheet[n*8+2], cueSheet[n*8+3], cueSheet[n*8+4],
	   cueSheet[n*8+5], cueSheet[n*8+6], cueSheet[n*8+7]);
  }

  *cueSheetLen = len * 8;
  return cueSheet;
}

int GenericMMC::sendCueSheet()
{
  unsigned char cmd[10];
  long cueSheetLen;
  unsigned long variant;
  unsigned char *cueSheet;
  int cueSheetSent = 0;

  for (variant = 0; variant <= CUE_VAR_MAX; variant++) {

    if (cdTextEncoder_ == NULL &&
	(variant & CUE_VAR_CDTEXT_NO_TIME) != 0) {
      // skip CD-TEXT variants if no CD-TEXT has to be written
      continue;
    }

    cueSheet = createCueSheet(variant, &cueSheetLen);

    if (cueSheet != NULL) {
      memset(cmd, 0, 10);

      cmd[0] = 0x5d; // SEND CUE SHEET

      cmd[6] = cueSheetLen >> 16;
      cmd[7] = cueSheetLen >> 8;
      cmd[8] = cueSheetLen;

      if (sendCmd(cmd, 10, cueSheet, cueSheetLen, NULL, 0, 0) != 0) {
	delete[] cueSheet;
      }
      else {
	log_message(3, "Drive accepted cue sheet variant %lx.", variant);
	delete[] cueSheet;
	cueSheetSent = 1;
	break;
      }
    }
  }

  if (cueSheetSent) {
    return 0;
  }
  else {
    log_message(-2,
	    "Drive does not accept any cue sheet variant - please report.");
    return 1;
  }
}

int GenericMMC::initDao(const Toc *toc)
{
  long n;
  blockLength_ = AUDIO_BLOCK_LEN + MAX_SUBCHANNEL_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

  toc_ = toc;

  if (options_ & OPT_MMC_CD_TEXT) {
    delete cdTextEncoder_;
    cdTextEncoder_ = new CdTextEncoder(toc_);
    if (cdTextEncoder_->encode() != 0) {
      log_message(-2, "CD-TEXT encoding failed.");
      return 1;
    }

    if (cdTextEncoder_->getSubChannels(&n) == NULL || n == 0) {
      delete cdTextEncoder_;
      cdTextEncoder_ = NULL;
    }
    
    //return 1;
  }

  diskInfo();

  if (!diskInfo_.valid.empty || !diskInfo_.valid.append) {
    log_message(-2, "Cannot determine status of inserted medium.");
    return 1;
  }

  if (!diskInfo_.append) {
    log_message(-2, "Inserted medium is not appendable.");
    return 1;
  }

  if ((!diskInfo_.empty && diskInfo_.append) && toc_->firstTrackNo() != 0 ) {
    log_message(-1, "Cannot choose number of first track in append mode.");
    return 1;
  }

  if (selectSpeed() != 0 ||
      getSessionInfo() != 0) {
    return 1;
  }


  // allocate buffer for writing zeros
  n = blocksPerWrite_ * blockLength_;
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);

  return 0;
}

int GenericMMC::startDao()
{
  unsigned long variant;
  int writeParametersSet = 0;

  scsiTimeout_ = scsiIf_->timeout(3 * 60);

  for (variant = 0; variant <= WMP_VAR_MAX; variant++) {
    if (cdTextEncoder_ == NULL &&
	(variant & WMP_VAR_CDTEXT_NO_DATA_BLOCK_TYPE) != 0) {
      // skip CD-TEXT variants if no CD-TEXT has to be written
      continue;
    }

    if (setWriteParameters(variant) == 0) {
      log_message(3, "Drive accepted write parameter mode page variant %lx.",
	      variant);
      writeParametersSet = 1;
      break;
    }
  }

  if (!writeParametersSet) {
    log_message(-2, "Cannot setup write parameters for session-at-once mode.");
    log_message(-2, "Please try to use the 'generic-mmc-raw' driver.");
    return 1;
  }

  if (!simulate_) {
    if (performPowerCalibration() != 0) {
      if (!force()) {
	log_message(-2, "Use option --force to ignore this error.");
	return 1;
      }
      else {
	log_message(-2, "Ignored because of option --force.");
      }
    }
  }

  // It does not hurt if the following command fails.
  // The Panasonic CW-7502 needs it, unfortunately it returns the wrong
  // data so we ignore the returned data and start writing always with
  // LBA -150.
  getNWA(NULL);

  if (sendCueSheet() != 0)
    return 1;

  //log_message(2, "Writing lead-in and gap...");

  if (writeCdTextLeadIn() != 0) {
    return 1;
  }


  long lba = diskInfo_.thisSessionLba - 150;

  TrackData::Mode mode = TrackData::AUDIO;
  TrackData::SubChannelMode subChanMode = TrackData::SUBCHAN_NONE;
  TrackIterator itr(toc_);
  const Track *tr;

  if ((tr = itr.first()) != NULL) {
    mode = tr->type();
    subChanMode = tr->subChannelType();
  }
  
  if (writeZeros(mode, subChanMode, lba, lba + 150, 150) != 0) {
    return 1;
  }
  
  return 0;
}

int GenericMMC::finishDao()
{
  int ret;

  flushCache(); /* Some drives never return to a ready state after writing
		 * the lead-out. This is a try to solve this problem.
		 */

  while ((ret = checkDriveReady()) == 2) {
    mSleep(2000);
  }

  if (ret != 0)
    log_message(-1, "TEST UNIT READY failed after recording.");
  
  log_message(2, "Flushing cache...");
  
  if (flushCache() != 0) {
    return 1;
  }

  scsiIf_->timeout(scsiTimeout_);

  delete cdTextEncoder_;
  cdTextEncoder_ = NULL;

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void GenericMMC::abortDao()
{
  flushCache();

  delete cdTextEncoder_;
  cdTextEncoder_ = NULL;
}

// Writes data to target, the block length depends on the actual writing
// 'mode'. 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function.
// return: 0: OK
//         1: scsi command failed
int GenericMMC::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
			  long &lba, const char *buf, long len)
{
  assert(blocksPerWrite_ > 0);
  int writeLen = 0;
  unsigned char cmd[10];
  long blockLength = blockSize(mode, sm);
  int retry;
  int ret;

#if 0
  long bufferCapacity;
  int waitForBuffer;
  int speedFrac;

  if (speed_ > 0)
    speedFrac = 75 * speed_;
  else
    speedFrac = 75 * 10; // adjust this value when the first >10x burner is out
#endif

#if 0
  long sum, i;

  sum = 0;

  for (i = 0; i < len * blockLength; i++) {
    sum += buf[i];
  }

  log_message(0, "W: %ld: %ld, %ld, %ld", lba, blockLength, len, sum);
#endif

  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE1
  
  while (len > 0) {
    writeLen = (len > blocksPerWrite_ ? blocksPerWrite_ : len);

    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;

    cmd[7] = writeLen >> 8;
    cmd[8] = writeLen & 0xff;

#if 0
    do {
      long available
      waitForBuffer = 0;

      if (readBufferCapacity(&bufferCapacity, &available) == 0) {
	//log_message(0, "Buffer Capacity: %ld", bufferCapacity);
	if (bufferCapacity < writeLen * blockLength) {
	  long t = 1000 * writeLen;
	  t /= speedFrac;
	  if (t <= 0)
	    t = 1;

	  log_message(0, "Waiting for %ld msec at lba %ld", t, lba);

	  mSleep(t);
	  waitForBuffer = 1;
	}
      }
    } while (waitForBuffer);
#endif

    do {
      retry = 0;

      ret = sendCmd(cmd, 10, (unsigned char *)buf, writeLen * blockLength,
		    NULL, 0, 0);

      if(ret == 2) {
        const unsigned char *sense;
        int senseLen;

        sense = scsiIf_->getSense(senseLen);

	// check if drive rejected the command because the internal buffer
	// is filled
	if(senseLen >= 14 && (sense[2] & 0x0f) == 0x2 && sense[7] >= 6 &&
	   sense[12] == 0x4 && sense[13] == 0x8) {
	  // Not Ready, long write in progress
	  mSleep(40);
          retry = 1;
        }
	else {
	  scsiIf_->printError();
	}
      }
    } while (retry);

    if (ret != 0) {
      log_message(-2, "Write data failed.");
      return 1;
    }

    buf += writeLen * blockLength;

    lba += writeLen;
    len -= writeLen;
  }
      
  return 0;
}

int GenericMMC::writeCdTextLeadIn()
{
  unsigned char cmd[10];
  const PWSubChannel96 **cdTextSubChannels;
  long cdTextSubChannelCount;
  long channelsPerCmd;
  long scp = 0;
  long lba = -150 - leadInLen_;
  long len = leadInLen_;
  long n;
  long i;
  int retry;
  int ret;
  unsigned char *p;

  if (cdTextEncoder_ == NULL)
    return 0;

  channelsPerCmd = scsiIf_->maxDataLen() / 96;

  cdTextSubChannels = cdTextEncoder_->getSubChannels(&cdTextSubChannelCount);

  assert(channelsPerCmd > 0);
  assert(cdTextSubChannels != NULL);
  assert(cdTextSubChannelCount > 0);

  log_message(2, "Writing CD-TEXT lead-in...");

  log_message(4, "Start LBA: %ld, length: %ld", lba, len);

  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE1

  while (len > 0) {
    n = (len > channelsPerCmd) ? channelsPerCmd : len;

    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;

    cmd[7] = n >> 8;
    cmd[8] = n;

    p = transferBuffer_;
    
    for (i = 0; i < n; i++) {
      memcpy(p, cdTextSubChannels[scp]->data(), 96);
      p += 96;

      scp++;
      if (scp >= cdTextSubChannelCount)
	scp = 0;
    }

    log_message(5, "Writing %ld CD-TEXT sub-channels at LBA %ld.", n, lba);

    do {
      retry = 0;

      ret = sendCmd(cmd, 10, transferBuffer_, n * 96, NULL, 0, 0);

      if(ret == 2) {
        const unsigned char *sense;
        int senseLen;
	
        sense = scsiIf_->getSense(senseLen);

	// check if drive rejected the command because the internal buffer
	// is filled
	if(senseLen >= 14 && (sense[2] & 0x0f) == 0x2 && sense[7] >= 6 &&
	   sense[12] == 0x4 && sense[13] == 0x8) {
	  // Not Ready, long write in progress
	  mSleep(40);
          retry = 1;
        }
	else {
	  scsiIf_->printError();
	}
      }
    } while (retry);

    if (ret != 0) {
      log_message(-2, "Writing of CD-TEXT data failed.");
      return 1;
    }
      

    len -= n;
    lba += n;
  }

  return 0;
}

DiskInfo *GenericMMC::diskInfo()
{
  unsigned char cmd[10];
  unsigned long dataLen = 34;
  unsigned char data[34];
  char spd;

  memset(&diskInfo_, 0, sizeof(DiskInfo));

  // perform READ DISK INFORMATION
  memset(cmd, 0, 10);
  memset(data, 0, dataLen);

  cmd[0] = 0x51; // READ DISK INFORMATION
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen, 0) == 0) {
    diskInfo_.cdrw = (data[2] & 0x10) ? 1 : 0;
    diskInfo_.valid.cdrw = 1;

    switch (data[2] & 0x03) {
    case 0:
      // disc is empty
      diskInfo_.empty = 1;
      diskInfo_.append = 1;

      diskInfo_.manufacturerId = Msf(data[17], data[18], data[19]);
      diskInfo_.valid.manufacturerId = 1;
      break;

    case 1:
      // disc is not empty but appendable
      diskInfo_.sessionCnt = data[4];
      diskInfo_.lastTrackNr = data[6];

      diskInfo_.diskTocType = data[8];

      switch ((data[2] >> 2) & 0x03) {
      case 0:
	// last session is empty
	diskInfo_.append = 1;

	// don't count the empty session and invisible track
	diskInfo_.sessionCnt -= 1;
	diskInfo_.lastTrackNr -= 1;

	if (getStartOfSession(&(diskInfo_.thisSessionLba)) == 0) {
	  // reserve space for pre-gap after lead-in
	  diskInfo_.thisSessionLba += 150;
	}
	else {
	  // try to guess start of data area from start of lead-in
	  // reserve space for 4500 lead-in and 150 pre-gap sectors
	  diskInfo_.thisSessionLba = Msf(data[17], data[18],
					 data[19]).lba() - 150 + 4650;
	}
	break;

      case 1:
	// last session is incomplete (not fixated)
	// we cannot append in DAO mode, just update the statistic data
	
	diskInfo_.diskTocType = data[8];

	// don't count the invisible track
	diskInfo_.lastTrackNr -= 1;
	break;
      }
      break;

    case 2:
      // disk is complete
      diskInfo_.sessionCnt = data[4];
      diskInfo_.lastTrackNr = data[6];
      diskInfo_.diskTocType = data[8];
      break;
    }

    diskInfo_.valid.empty = 1;
    diskInfo_.valid.append = 1;

    if (data[21] != 0xff || data[22] != 0xff || data[23] != 0xff) {
      diskInfo_.valid.capacity = 1;
      diskInfo_.capacity = Msf(data[21], data[22], data[23]).lba() - 150;
    }
  }

  // perform READ TOC to get session info
  memset(cmd, 0, 10);
  dataLen = 12;
  memset(data, 0, dataLen);

  cmd[0] = 0x43; // READ TOC
  cmd[2] = 1; // get session info
  cmd[8] = dataLen; // allocation length

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen, 0) == 0) {
    diskInfo_.lastSessionLba = (data[8] << 24) | (data[9] << 16) |
                               (data[10] << 8) | data[11];

    if (!diskInfo_.valid.empty) {
      diskInfo_.valid.empty = 1;
      diskInfo_.empty = (data[3] == 0) ? 1 : 0;

      diskInfo_.sessionCnt = data[3];
    }
  }

  // read ATIP data
  dataLen = 28;
  memset(cmd, 0, 10);
  memset(data, 0, dataLen);

  cmd[0] = 0x43; // READ TOC/PMA/ATIP
  cmd[1] = 0x00;
  cmd[2] = 4; // get ATIP
  cmd[7] = 0;
  cmd[8] = dataLen; // data length
  
  if (sendCmd(cmd, 10, NULL, 0, data, dataLen, 0) == 0) {
    if (data[6] & 0x04) {
      diskInfo_.valid.recSpeed = 1;
      spd = (data[16] >> 4) & 0x07;
      diskInfo_.recSpeedLow = spd == 1 ? 2 : 0;
      
      spd = (data[16] & 0x0f);
      diskInfo_.recSpeedHigh = spd >= 1 && spd <= 4 ? spd * 2 : 0;
    }

    if (data[8] >= 80 && data[8] <= 99) {
      diskInfo_.manufacturerId = Msf(data[8], data[9], data[10]);
      diskInfo_.valid.manufacturerId = 1;
    }
  }

  return &diskInfo_;
}


// tries to read catalog number from disk and adds it to 'toc'
// return: 1 if valid catalog number was found, else 0
int GenericMMC::readCatalog(Toc *toc, long startLba, long endLba)
{
  unsigned char cmd[10];
  unsigned char data[24];
  char catalog[14];
  int i;

  if (options_ & OPT_MMC_SCAN_MCN) {
    if (readCatalogScan(catalog, startLba, endLba) == 0) {
      if (catalog[0] != 0) {
	if (toc->catalog(catalog) == 0)
	  return 1;
	else
	  log_message(-1, "Found illegal MCN data: %s", catalog);
      }
    }
  }
  else {
    memset(cmd, 0, 10);
    memset(data, 0, 24);

    cmd[0] = 0x42; // READ SUB-CHANNEL
    cmd[2] = 1 << 6;
    cmd[3] = 0x02; // get media catalog number
    cmd[8] = 24;   // transfer length
    
    if (sendCmd(cmd, 10, NULL, 0, data, 24) != 0) {
      log_message(-2, "Cannot get catalog number.");
      return 0;
    }
    
    if (data[8] & 0x80) {
      for (i = 0; i < 13; i++) {
	catalog[i] = data[0x09 + i];
      }
      catalog[13] = 0;
      
      if (toc->catalog(catalog) == 0) {
	return 1;
      }
    }
  }

  return 0;
}

int GenericMMC::readIsrc(int trackNr, char *buf)
{
  unsigned char cmd[10];
  unsigned char data[24];
  int i;

  buf[0] = 0;

  memset(cmd, 0, 10);
  memset(data, 0, 24);

  cmd[0] = 0x42; // READ SUB-CHANNEL
  cmd[2] = 1 << 6;
  cmd[3] = 0x03; // get ISRC
  cmd[6] = trackNr;
  cmd[8] = 24;   // transfer length

  if (sendCmd(cmd, 10, NULL, 0, data, 24) != 0) {
    log_message(-2, "Cannot get ISRC code.");
    return 0;
  }

  if (data[8] & 0x80) {
    for (i = 0; i < 12; i++) {
      buf[i] = data[0x09 + i];
    }
    buf[12] = 0;
  }

  return 0;
}

int GenericMMC::analyzeTrack(TrackData::Mode mode, int trackNr, long startLba,
			     long endLba, Msf *indexIncrements,
			     int *indexIncrementCnt, long *pregap,
			     char *isrcCode, unsigned char *ctl)
{
  int ret;
  int noScan = 0;

  selectSpeed();

  if ((readCapabilities_ & CDR_AUDIO_SCAN_CAP) == 0) {
    ret = analyzeTrackSearch(mode, trackNr, startLba, endLba, indexIncrements,
			     indexIncrementCnt, pregap, isrcCode, ctl);
    noScan = 1;
  }
  else {
    ret = analyzeTrackScan(mode, trackNr, startLba, endLba,
			   indexIncrements, indexIncrementCnt, pregap,
			   isrcCode, ctl);
  }

  if (noScan || (options_ & OPT_MMC_READ_ISRC) != 0 ||
      (readCapabilities_ & CDR_READ_CAP_AUDIO_PQ_HEX) != 0) {
    // The ISRC code is usually not usable if the PQ channel data is
    // converted to hex numbers by the drive. Read them with the
    // appropriate command in this case

    *isrcCode = 0;
    if (mode == TrackData::AUDIO)
      readIsrc(trackNr, isrcCode);
  }

  return ret;
}

int GenericMMC::readSubChannels(TrackData::SubChannelMode sm,
				long lba, long len, SubChannel ***chans,
				Sample *audioData)
{
  int retries = 5;
  unsigned char cmd[12];
  int i;
  long blockLen = 0;
  unsigned long subChanMode = 0;

  cmd[0] = 0xbe;  // READ CD
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = len >> 16;
  cmd[7] = len >> 8;
  cmd[8] = len;
  cmd[9] = 0xf8;
  cmd[11] = 0;

  switch (sm) {
  case TrackData::SUBCHAN_NONE:
    // no sub-channel data selected choose what is available

    if ((readCapabilities_ & CDR_READ_CAP_AUDIO_PW_RAW) != 0) {
      // reading of raw P-W sub-channel data is supported
      blockLen = AUDIO_BLOCK_LEN + 96;
      cmd[10] = 0x01;  // raw P-W sub-channel data

      subChanMode = CDR_READ_CAP_AUDIO_PW_RAW;
    }
    else if ((readCapabilities_ & 
	      (CDR_READ_CAP_AUDIO_PQ_BCD|CDR_READ_CAP_AUDIO_PQ_HEX)) != 0) {
      // reading of PQ sub-channel data is supported
      blockLen = AUDIO_BLOCK_LEN + 16;
      cmd[10] = 0x02;  // PQ sub-channel data

      if ((readCapabilities_ & CDR_READ_CAP_AUDIO_PQ_BCD) != 0)
	subChanMode = CDR_READ_CAP_AUDIO_PQ_BCD;
      else
	subChanMode = CDR_READ_CAP_AUDIO_PQ_HEX;
    }
    else {
      // no usable sub-channel reading mode is supported
      blockLen = AUDIO_BLOCK_LEN;
      cmd[10] = 0;

      subChanMode = 0;
    }
    break;

  case TrackData::SUBCHAN_RW:
    blockLen = AUDIO_BLOCK_LEN + 96;
    cmd[10] = 0x04;
    break;

  case TrackData::SUBCHAN_RW_RAW:
    blockLen = AUDIO_BLOCK_LEN + 96;
    cmd[10] = 0x01;
    break;
  }

  while (1) {
    if (sendCmd(cmd, 12, NULL, 0,
		transferBuffer_, len * blockLen, retries == 0 ? 1 : 0) != 0) {
      if (retries == 0) 
	return 1;
    }
    else {
      break;
    }

    retries--;
  }

#if 0
  if (lba > 5000) {
    char fname[200];
    sprintf(fname, "testout_%ld", lba);
    FILE *fp = fopen(fname, "w");
    fwrite(transferBuffer_, blockLen, len, fp);
    fclose(fp);
  }
#endif

  if (subChanMode != 0) {
    unsigned char *buf = transferBuffer_ + AUDIO_BLOCK_LEN;

    for (i = 0; i < len; i++) {
      switch (subChanMode) {
      case CDR_READ_CAP_AUDIO_PQ_HEX:
	// All numbers in sub-channel data are hex conforming to the
	// MMC standard. We have to convert them back to BCD for the
	// 'SubChannel' class.
	buf[1] = SubChannel::bcd(buf[1]);
	buf[2] = SubChannel::bcd(buf[2]);
	buf[3] = SubChannel::bcd(buf[3]);
	buf[4] = SubChannel::bcd(buf[4]);
	buf[5] = SubChannel::bcd(buf[5]);
	buf[6] = SubChannel::bcd(buf[6]);
	buf[7] = SubChannel::bcd(buf[7]);
	buf[8] = SubChannel::bcd(buf[8]);
	buf[9] = SubChannel::bcd(buf[9]);
	// fall through

      case CDR_READ_CAP_AUDIO_PQ_BCD:
	((PQSubChannel16*)scannedSubChannels_[i])->init(buf);
	if (scannedSubChannels_[i]->type() != SubChannel::QMODE_ILLEGAL) {
	  // the CRC of the sub-channel data is usually invalid -> mark the
	  // sub-channel object that it should not try to verify the CRC
	  scannedSubChannels_[i]->crcInvalid();
	}
	break;
      
      case CDR_READ_CAP_AUDIO_PW_RAW:
	((PWSubChannel96*)scannedSubChannels_[i])->init(buf);
	break;
      }

#if 0
      if (subChanMode == CDR_READ_CAP_AUDIO_PW_RAW) {
	// xxam!
	int j, k;
	
	log_message(0, "");
	for (j = 0; j < 4; j++) {
	  for (k = 0; k < 24; k++) {
	    unsigned char data = buf[j * 24 + k];
	    log_message(0, "%02x ", data&0x3f);
	  }
	  log_message(0, "");
	}
      }
#endif

      buf += blockLen;
    }
  }
  
  if (audioData != NULL) {
    unsigned char *p = transferBuffer_;

    for (i = 0; i < len; i++) {
      memcpy(audioData, p, AUDIO_BLOCK_LEN);

      audioData += SAMPLES_PER_BLOCK;

      switch (sm) {
      case TrackData::SUBCHAN_NONE:
	break;

      case TrackData::SUBCHAN_RW:
      case TrackData::SUBCHAN_RW_RAW:
	memcpy(audioData, p + AUDIO_BLOCK_LEN, PW_SUBCHANNEL_LEN);
	audioData += PW_SUBCHANNEL_LEN / SAMPLE_LEN;
	break;
      }

      p += blockLen;
    }
  }

  if (subChanMode == 0)
    *chans = NULL;
  else
    *chans = scannedSubChannels_;

  return 0;
}


// Tries to retrieve configuration feature 'feature' and fills data to
// provided buffer 'buf' with maximum length 'bufLen'.
// Return: 0: OK
//         1: feature not available
//         2: SCSI error
int GenericMMC::getFeature(unsigned int feature, unsigned char *buf,
			   unsigned long bufLen, int showMsg)
{
  unsigned char header[8];
  unsigned char *data;
  unsigned char cmd[10];
  unsigned long len;

  memset(cmd, 0, 10);
  memset(header, 0, 8);

  cmd[0] = 0x46; // GET CONFIGURATION
  cmd[1] = 0x02; // return single feature descriptor
  cmd[2] = feature >> 8;
  cmd[3] = feature;
  cmd[8] = 8; // allocation length

  if (sendCmd(cmd, 10, NULL, 0, header, 8, showMsg) != 0) {
    if (showMsg)
      log_message(-2, "Cannot get feature 0x%x.", feature);
    return 2;
  }

  len = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];

  log_message(4, "getFeature: data len: %lu", len);

  if (len < 8)
    return 1; // feature not defined

  if (bufLen == 0)
    return 0;

  len -= 4; 

  if (len > bufLen)
    len = bufLen;

  data = new unsigned char[len + 8];

  cmd[7] = (len + 8) >> 8;
  cmd[8] = (len + 8);

  if (sendCmd(cmd, 10, NULL, 0, data, len + 8, showMsg) != 0) {
    if (showMsg)
      log_message(-2, "Cannot get data for feature 0x%x.", feature);
    
    delete[] data;
    return 2;
  }
  
  len = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
  
  log_message(4, "getFeature: data len: %lu", len);

  if (len < 8) {
    delete[] data;
    return 1; // feature not defined
  }

  len -= 4;

  if (len > bufLen)
    len = bufLen;

  memcpy(buf, data + 8, len);

  delete[] data;
  
  return 0;
}

const DriveInfo *GenericMMC::driveInfo(bool showErrorMsg)
{
  unsigned char mp[32];

  if (driveInfo_ != NULL)
    return driveInfo_;

  driveInfo_ = new DriveInfo;

  if (getModePage(0x2a, mp, 32, NULL, NULL, showErrorMsg) != 0) {
    if (showErrorMsg) {
      log_message(-2, "Cannot retrieve drive capabilities mode page.");
    }
    delete driveInfo_;
    driveInfo_ = NULL;
    return NULL;
  }

  driveInfo_->burnProof = (mp[4] & 0x80) ? 1 : 0;
  driveInfo_->accurateAudioStream = mp[5] & 0x02 ? 1 : 0;

  driveInfo_->maxReadSpeed = (mp[8] << 8) | mp[9];
  driveInfo_->currentReadSpeed = (mp[14] << 8) | mp[15];

  driveInfo_->maxWriteSpeed = (mp[18] << 8) | mp[19];
  driveInfo_->currentWriteSpeed = (mp[20] << 8) | mp[21];

#if 0
  unsigned char cdMasteringFeature[8];
  if (getFeature(0x2e, cdMasteringFeature, 8, 1) == 0) {
    log_message(0, "Feature: %x %x %x %x %x %x %x %x", cdMasteringFeature[0],
	    cdMasteringFeature[1], cdMasteringFeature[2],
	    cdMasteringFeature[3], cdMasteringFeature[4],
	    cdMasteringFeature[5], cdMasteringFeature[6],
	    cdMasteringFeature[7]);
  }
#endif

  RicohGetWriteOptions();

  return driveInfo_;
}

TrackData::Mode GenericMMC::getTrackMode(int, long trackStartLba)
{
  unsigned char cmd[12];
  unsigned char data[AUDIO_BLOCK_LEN];

  memset(cmd, 0, 12);
  cmd[0] = 0xbe;  // READ CD

  cmd[2] = trackStartLba >> 24;
  cmd[3] = trackStartLba >> 16;
  cmd[4] = trackStartLba >> 8;
  cmd[5] = trackStartLba;

  cmd[8] = 1;

  cmd[9] = 0xf8;

  if (sendCmd(cmd, 12, NULL, 0, data, AUDIO_BLOCK_LEN) != 0) {
    log_message(-2, "Cannot read sector of track.");
    return TrackData::MODE0;
  }

  if (memcmp(CdrDriver::syncPattern, data, 12) != 0) {
    // cannot be a data sector
    return TrackData::MODE0;
  }

  TrackData::Mode mode = determineSectorMode(data + 12);

  if (mode == TrackData::MODE0) {
    // illegal
    log_message(-2, "Found illegal mode in sector %ld.", trackStartLba);
  }

  return mode;
}

CdRawToc *GenericMMC::getRawToc(int sessionNr, int *len)
{
  unsigned char cmd[10];
  unsigned short dataLen;
  unsigned char *data = NULL;;
  unsigned char reqData[4]; // buffer for requestion the actual length
  unsigned char *p;
  int i, entries;
  CdRawToc *rawToc;

  assert(sessionNr >= 1);

  // read disk toc length
  memset(cmd, 0, 10);
  cmd[0] = 0x43; // READ TOC
  cmd[2] = 2;
  cmd[6] = sessionNr;
  cmd[8] = 4;

  if (sendCmd(cmd, 10, NULL, 0, reqData, 4) != 0) {
    log_message(-2, "Cannot read disk toc.");
    return NULL;
  }

  dataLen = ((reqData[0] << 8) | reqData[1]) + 2;
  
  log_message(4, "Raw toc data len: %d", dataLen);

  data = new unsigned char[dataLen];
  
  // read disk toc
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read disk toc.");
    delete[] data;
    return NULL;
  }

  entries = (((data[0] << 8) | data[1]) - 2) / 11;

  rawToc = new CdRawToc[entries];

  for (i = 0, p = data + 4; i < entries; i++, p += 11 ) {
#if 0
    log_message(5, "%d %02x %02d %2x %02d:%02d:%02d %02d %02d:%02d:%02d",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10]);
#endif
    rawToc[i].sessionNr = p[0];
    rawToc[i].adrCtl = p[1];
    rawToc[i].point = p[3];
    rawToc[i].pmin = p[8];
    rawToc[i].psec = p[9];
    rawToc[i].pframe = p[10];
  }

  delete[] data;

  *len = entries;

  return rawToc;
}

long GenericMMC::readTrackData(TrackData::Mode mode,
			       TrackData::SubChannelMode sm,
			       long lba, long len, unsigned char *buf)
{
  long i;
  long inBlockLen = AUDIO_BLOCK_LEN;
  unsigned char cmd[12];
  const unsigned char *sense;
  int senseLen;

  memset(cmd, 0, 12);

  cmd[0] = 0xbe; // READ CD
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = len >> 16;
  cmd[7] = len >> 8;
  cmd[8] = len;
  cmd[9] = 0xf8;

  switch (sm) {
  case TrackData::SUBCHAN_NONE:
    cmd[10] = 0; // no sub-channel reading
    break;

  case TrackData::SUBCHAN_RW:
    cmd[10] = 0x4;
    inBlockLen += PW_SUBCHANNEL_LEN;
    break;

  case TrackData::SUBCHAN_RW_RAW:
    cmd[10] = 0x1;
    inBlockLen += PW_SUBCHANNEL_LEN;
    break;
  }

  switch (sendCmd(cmd, 12, NULL, 0, transferBuffer_, len * inBlockLen, 0)) {
  case 0:
    break;

  case 2:
    sense = scsiIf_->getSense(senseLen);

    if (senseLen > 0x0c) {
      if ((sense[2] & 0x0f) == 5) { // Illegal request
	switch (sense[12]) {
	case 0x63: // End of user area encountered on this track
	case 0x64: // Illegal mode for this track
	  return -2;
	  break;

	case 0x20: // INVALID COMMAND OPERATION CODE
	case 0x24: // INVALID FIELD IN CDB
	case 0x26: // INVALID FIELD IN PARAMETER LIST
	  /* These error codes mean that something was wrong with the
	   * command we are sending. Report them as hard errors to the
	   * upper level.
	   */
	  scsiIf_->printError();
	  return -1;
	  break;
	}
      }
      else if ((sense[2] & 0x0f) == 4) { // Hardware Error
	switch (sense[12]) {
	case 0x9: // focus servo failure
	  return -2;
	  break;
	}
      }    
      else if ((sense[2] & 0x0f) == 3) { // Medium error
	switch (sense[12]) {
	case 0x02: // No seek complete, sector not found
	case 0x06: // no reference position found
	case 0x11: // L-EC error
	case 0x15: // random positioning error
	  return -2;
	  break;
	}
      }
    }

    /* All other errors are unexpected. They will be treated like L-EC errors
     * by the upper layer. Just print the error code so that we can decice
     * later to add the errors to the known possible error list.
     */
    
    scsiIf_->printError();
    return -2;
    break;

  default:
    log_message(-2, "Read error at LBA %ld, len %ld", lba, len);
    return -2;
    break;
  }

  unsigned char *sector = transferBuffer_;
  for (i = 0; i < len; i++) {
    if (buf != NULL) {
      switch (mode) {
      case TrackData::MODE1:
	memcpy(buf, sector + 16, MODE1_BLOCK_LEN);
	buf += MODE1_BLOCK_LEN;
	break;
      case TrackData::MODE1_RAW:
	memcpy(buf, sector, AUDIO_BLOCK_LEN);
	buf += AUDIO_BLOCK_LEN;
	break;
      case TrackData::MODE2:
      case TrackData::MODE2_FORM_MIX:
	memcpy(buf, sector + 16, MODE2_BLOCK_LEN);
	buf += MODE2_BLOCK_LEN;
	break;
      case TrackData::MODE2_FORM1:
	memcpy(buf, sector + 24, MODE2_FORM1_DATA_LEN);
	buf += MODE2_FORM1_DATA_LEN;
	break;
      case TrackData::MODE2_FORM2:
	memcpy(buf, sector + 24, MODE2_FORM2_DATA_LEN);
	buf += MODE2_FORM2_DATA_LEN;
	break;
      case TrackData::MODE2_RAW:
	memcpy(buf, sector, AUDIO_BLOCK_LEN);
	buf += AUDIO_BLOCK_LEN;
	break;
      case TrackData::MODE0:
      case TrackData::AUDIO:
	log_message(-3, "GenericMMC::readTrackData: Illegal mode.");
	return 0;
	break;
      }

      // copy sub-channel data
      switch (sm) {
      case TrackData::SUBCHAN_NONE:
	break;

      case TrackData::SUBCHAN_RW:
      case TrackData::SUBCHAN_RW_RAW:
	memcpy(buf, sector + AUDIO_BLOCK_LEN, PW_SUBCHANNEL_LEN);
	buf += PW_SUBCHANNEL_LEN;
	break;
      }
    }

#if 0
    // xxam!
    int j, k;

    log_message(0, "");
    for (j = 0; j < 4; j++) {
      for (k = 0; k < 24; k++) {
	unsigned char data = sector[AUDIO_BLOCK_LEN + j * 24 + k];
	log_message(0, "%02x ", data&0x3f);
      }
      log_message(0, "");
    }
#endif

    sector += inBlockLen;
  }

  return len;
}

int GenericMMC::readAudioRange(ReadDiskInfo *rinfo, int fd, long start,
			       long end, int startTrack,
			       int endTrack, TrackInfo *info)
{
  if (!onTheFly_) {
    if (((readCapabilities_ & CDR_READ_CAP_AUDIO_PQ_BCD) == 0 &&
	 (readCapabilities_ & CDR_READ_CAP_AUDIO_PW_RAW) == 0) ||
	(options_ & OPT_MMC_READ_ISRC) != 0) {
      int t;
      long pregap = 0;

      // The ISRC code is usually not usable if the PQ channel data is
      // converted to hex numbers by the drive. Read them with the
      // appropriate command in this case

      log_message(1, "Analyzing...");


      for (t = startTrack; t <= endTrack; t++) {
	long totalProgress;

	log_message(1, "Track %d...", t + 1);

	totalProgress = t * 1000;
	totalProgress /= rinfo->tracks;
	sendReadCdProgressMsg(RCD_ANALYZING, rinfo->tracks, t + 1, 0,
			      totalProgress);

	if ((readCapabilities_ & CDR_AUDIO_SCAN_CAP) == 0) {
	  // we have to use the binary search method to find pre-gap and
	  // index marks if the drive cannot read sub-channel data
	  if (!fastTocReading_) {
	    long slba, elba;
	    int i, indexCnt;
	    Msf index[98];
	    unsigned char ctl;

	    if (pregap > 0)
	      log_message(2, "Found pre-gap: %s", Msf(pregap).str());

	    slba = info[t].start;
	    if (info[t].mode == info[t + 1].mode)
	      elba = info[t + 1].start;
	    else
	      elba = info[t + 1].start - 150;
	    
	    pregap = 0;
	    if (analyzeTrackSearch(TrackData::AUDIO, t + 1, slba, elba,
				   index, &indexCnt, &pregap, info[t].isrcCode,
				   &ctl) != 0)
	      return 1;
	  
	    for (i = 0; i < indexCnt; i++)
	      info[t].index[i] = index[i].lba();
	    
	    info[t].indexCnt = indexCnt;
	    
	    if (t < endTrack)
	      info[t + 1].pregap = pregap;
	  }
	  else {
	    info[t].indexCnt = 0;
	    info[t + 1].pregap = 0;
	  }
	}


	info[t].isrcCode[0] = 0;
	readIsrc(t + 1, info[t].isrcCode);
	if (info[t].isrcCode[0] != 0)
	  log_message(2, "Found ISRC code.");

	totalProgress = (t + 1) * 1000;
	totalProgress /= rinfo->tracks;
	sendReadCdProgressMsg(RCD_ANALYZING, rinfo->tracks, t + 1, 1000,
			      totalProgress);
      }

      log_message(1, "Reading...");
    }
  }

  if (subChanReadMode_ == TrackData::SUBCHAN_NONE) {
    return CdrDriver::readAudioRangeParanoia(rinfo, fd, start, end, startTrack,
					     endTrack, info);
  }
  else {
    return CdrDriver::readAudioRangeStream(rinfo, fd, start, end, startTrack,
					   endTrack, info);
  }
}

int GenericMMC::getTrackIndex(long lba, int *trackNr, int *indexNr,
			      unsigned char *ctl)
{
  unsigned char cmd[12];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];
  int waitLoops = 10;
  int waitFailed = 0;

  // play one audio block
  memset(cmd, 0, 10);
  cmd[0] = 0x45; // PLAY AUDIO
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[7] = 0;
  cmd[8] = 1;

  if (sendCmd(cmd, 10, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot play audio block.");
    return 1;
  }

  // wait until the play command finished
  memset(cmd, 0, 12);
  cmd[0] = 0xbd; // MECHANISM STATUS
  cmd[9] = 8;

  while (waitLoops > 0) {
    if (sendCmd(cmd, 12, NULL, 0, data, 8, 0) == 0) {
      //log_message(0, "%d, %x", waitLoops, data[1]);
      if ((data[1] >> 5) == 1) // still playing?
	waitLoops--;
      else
	waitLoops = 0;
    }
    else {
      waitFailed = 1;
      waitLoops = 0;
    }
  }

  if (waitFailed) {
    // The play operation immediately returns success status and the waiting
    // loop above failed. Wait here for a while until the desired block is
    // played. It takes ~13 msecs to play a block but access time is in the
    // order of several 100 msecs
    mSleep(300);
  }

  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x01; // get sub Q channel data
  cmd[6] = 0;
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read sub Q channel data.");
    return 1;
  }

  *trackNr = data[6];
  *indexNr = data[7];
  if (ctl != NULL) {
    *ctl = data[5] & 0x0f;
  }

  //log_message(0, "%d %d", *trackNr, *indexNr);

  return 0;
}

/*
 * Checks if a certain sub-channel reading mode is supported.
 * lba: start address for reading
 * len: maximum number of sectors available for testing
 * subChanMode: 1: read PQ sub-channels
 *              2: read raw P-W sub-channels
 *              3: read cooked R-W sub-channels
 * Return: 0 sub-channel read mode not supported
 *         1 sub-channel read mode supported (BCD for PQ)
 *         2 sub-channel read mode supported (HEX for PQ)
 *         3 sub-channel read mode PQ supported but cannot determine data
 *           format
*/

int GenericMMC::readCdTest(long lba, long len, int subChanMode) const
{
  unsigned char cmd[12];
  long blockLen;
  int ret;
  int successRead = 0;
  int pqSubChanBcdOk = 0;
  int pqSubChanHexOk = 0;

  memset(cmd, 0, sizeof(cmd));

  //log_message(0, "readCdTest: %ld %ld %d", lba, len, subChanMode);

  if (len <= 0)
    return 0;

  cmd[0] = 0xbe;  // READ CD
  cmd[8] = 1; // transfer length: 1
  cmd[9] = 0xf8;

  blockLen = AUDIO_BLOCK_LEN;

  switch (subChanMode) {
  case 1: // PQ
    blockLen += PQ_SUBCHANNEL_LEN;
    cmd[10] = 0x02;
    if (len > 300)
      len = 300; /* we have to check many sub-channels here to determine the
		  * data mode (BCD or HEX)
		  */
    break;

  case 2: // PW_RAW
    cmd[10] = 0x01;
    blockLen +=  PW_SUBCHANNEL_LEN;
    if (len > 10)
      len = 10;
    break;

  case 3: // RW_COOKED
    cmd[10] = 0x04;
    blockLen +=  PW_SUBCHANNEL_LEN;
    if (len > 10)
      len = 10;
    break;
  }

  while (len > 0) {
    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;
    
    if ((ret = sendCmd(cmd, 12, NULL, 0, transferBuffer_, blockLen, 0)) == 0) {
      successRead++;

      if (subChanMode == 1) {
	unsigned char *buf = transferBuffer_ + AUDIO_BLOCK_LEN;

#if 0
	{
 	  PQSubChannel16 chan;
	  chan.init(buf);
	  chan.print();
	}
#endif

	// check if Q sub-channel values are in BCD or HEX format
	if (SubChannel::isBcd(buf[1]) &&
	    SubChannel::isBcd(buf[2]) &&
	    SubChannel::isBcd(buf[3]) &&
	    SubChannel::isBcd(buf[4]) &&
	    SubChannel::isBcd(buf[5]) &&
	    SubChannel::isBcd(buf[6]) &&
	    SubChannel::isBcd(buf[7]) &&
	    SubChannel::isBcd(buf[8]) &&
	    SubChannel::isBcd(buf[9])) {
	  PQSubChannel16 chan;
	  chan.init(buf);

	  chan.type(SubChannel::QMODE1DATA);

	  int min = chan.amin();
	  int sec = chan.asec();
	  int frac = chan.aframe();

	  if ((frac >= 0 && frac < 75) &&
          (sec >= 0 && sec < 60) &&
          (min >= 0)) {
	  long pqlba = Msf(min, sec, frac).lba() - 150;

	  long diff = pqlba - lba;
	  if (diff < 0)
	    diff = -diff;
	  
	  if (diff < 20) {
	    pqSubChanBcdOk++;
	  }
	  }
	}

	if (buf[7] < 100 && buf[8] < 60 && buf[9] < 75) {
	  long pqlba = Msf(buf[7], buf[8], buf[9]).lba() - 150;

	  //log_message(0, "readCdTest: pqlba: %ld", pqlba);
	  long diff = pqlba - lba;
	  if (diff < 0)
	    diff = -diff;
	  
	  if (diff < 20) {
	    pqSubChanHexOk++;
	  }
	}
      }
    }


    len--;
    lba++;
  }

  if (successRead) {
    if (subChanMode == 1) {
      if (pqSubChanBcdOk > pqSubChanHexOk)
	return 1;
      else if (pqSubChanHexOk > pqSubChanBcdOk)
	return 2;
      else return 3;
    }
    else {
      return 1;
    }
  }

  return 0;
}

unsigned long GenericMMC::getReadCapabilities(const CdToc *toc,
					      int nofTracks) const
{
  unsigned long caps = 0;
  int audioRawPWChecked = 0;
  int audioPQChecked = 0;
  int audioCookedRWChecked = 0;
  int dataRawPWChecked = 0;
  int dataPQChecked = 0;
  int dataCookedRWChecked = 0;
  int t;

  if ((options_ & OPT_MMC_NO_SUBCHAN) != 0) {
    // driver options indicate that PQ and raw RW sub-channel reading for
    // audio tracks is not supported so skip all corresponding tests
    audioPQChecked = 1;
    audioRawPWChecked = 1;
  }
  else if ((options_ & OPT_MMC_USE_PQ) != 0) {
    // driver options indicated that PQ sub-channel reading is supported for
    // audio/data tracks and RW sub-channel reading is not supported, skip
    // the corresponding checks and set the capabilities appropriately
    audioPQChecked = 1;
    audioRawPWChecked = 1;
    dataPQChecked = 1;
    
    if ((options_ & OPT_MMC_PQ_BCD) != 0)
      caps |= CDR_READ_CAP_AUDIO_PQ_BCD | CDR_READ_CAP_DATA_PQ_BCD;
    else
      caps |= CDR_READ_CAP_AUDIO_PQ_HEX | CDR_READ_CAP_DATA_PQ_HEX;
  }
  else if ((options_ & OPT_MMC_USE_RAW_RW) != 0) {
    // driver options indicated that raw PW sub-channel reading is supported
    // audio tracks and raw PW sub-channel reading is not supported, skip
    // the corresponding checks and set the capabilities appropriately
    audioPQChecked = 1;
    audioRawPWChecked = 1;

    caps |= CDR_READ_CAP_DATA_PW_RAW;
  }

  for (t = 0; t < nofTracks; t++) {
    long tlen = toc[t+1].start - toc[t].start;

    if ((toc[t].adrCtl & 0x04) != 0) {
      // data track
      if (!dataPQChecked) {
	dataPQChecked = 1;

	log_message(3, "Checking for PQ sub-channel reading support (data track)...");
	switch (readCdTest(toc[t].start, tlen, 1)) {
	case 0:
	  log_message(3, "PQ sub-channel reading (data track) not supported.");
	  break;

	case 1:
	  log_message(2, "PQ sub-channel reading (data track) is supported, data format is BCD.");
	  caps |= CDR_READ_CAP_DATA_PQ_BCD;
	  break;
	  
	case 2:
	  log_message(2, "PQ sub-channel reading (data track) is supported, data format is HEX.");
	  caps |= CDR_READ_CAP_DATA_PQ_HEX;
	  break;
	  
	case 3:
	  log_message(2, "PQ sub-channel reading (data track) seems to be supported but cannot determine data format.");
	  log_message(2, "Please use driver option '--driver generic-mmc:0x1' or '--driver generic-mmc:0x3' to set the data format explicitly.");
	  break;
	}
      }
	
      if (!dataRawPWChecked) {
	dataRawPWChecked = 1;

	log_message(3, "Checking for raw P-W sub-channel reading support (data track)...");
	if (readCdTest(toc[t].start, tlen, 2)) {
	  log_message(2, "Raw P-W sub-channel reading (data track) is supported.");
	  caps |= CDR_READ_CAP_DATA_PW_RAW;
	}
	else {
	  log_message(3, "Raw P-W sub-channel reading (data track) is not supported.");
	}
      }

      if (!dataCookedRWChecked) {
	dataCookedRWChecked = 1;
      
	log_message(3, "Checking for cooked R-W sub-channel reading support (data track)...");
	if (readCdTest(toc[t].start, tlen, 3)) {
	  log_message(2, "Cooked R-W sub-channel reading (data track) is supported.");
	  caps |= CDR_READ_CAP_DATA_RW_COOKED;
	}
	else {
	  log_message(3, "Cooked R-W sub-channel reading (data track) is not supported.");
	}
      }
    }
    else {
      // audio track
      if (!audioPQChecked) {
	audioPQChecked = 1;

	log_message(3, "Checking for PQ sub-channel reading support (audio track)...");
	switch (readCdTest(toc[t].start, tlen, 1)) {
	case 0:
	  log_message(3, "PQ sub-channel reading (audio track) is not supported.");
	  break;

	case 1:
	  log_message(2, "PQ sub-channel reading (audio track) is supported, data format is BCD.");
	  caps |= CDR_READ_CAP_AUDIO_PQ_BCD;
	  break;

	case 2:
	  log_message(2, "PQ sub-channel reading (audio track) is supported, data format is HEX.");
	  caps |= CDR_READ_CAP_AUDIO_PQ_HEX;
	  break;

	case 3:
	  log_message(2, "PQ sub-channel reading (audio track) seems to be supported but cannot determine data format.");
	  log_message(2, "Please use driver option '--driver generic-mmc:0x1' or '--driver generic-mmc:0x3' to set the data format explicitly.");
	  break;
	}
      }

      if (!audioRawPWChecked) {
	audioRawPWChecked = 1;

	log_message(3, "Checking for raw P-W sub-channel reading support (audio track)...");
	if (readCdTest(toc[t].start, tlen, 2)) {
	  log_message(2, "Raw P-W sub-channel reading (audio track) is supported.");
	  caps |= CDR_READ_CAP_AUDIO_PW_RAW;
	}
	else {
	  log_message(3, "Raw P-W sub-channel reading (audio track) is not supported.");
	}
      }

      if (!audioCookedRWChecked) {
	audioCookedRWChecked = 1;

	log_message(3, "Checking for cooked R-W sub-channel reading support (audio track)...");
	if (readCdTest(toc[t].start, tlen, 3)) {
	  log_message(2, "Cooked R-W sub-channel reading (audio track) is supported.");
	  caps |= CDR_READ_CAP_AUDIO_RW_COOKED;
	}
	else {
	  log_message(3, "Raw R-W sub-channel reading (audio track) is not supported.");
	}
      }
    }
  }

  return caps;
}

int GenericMMC::RicohGetWriteOptions()
{
  unsigned char mp[14];

  driveInfo_->ricohJustLink = 0;
  driveInfo_->ricohJustSpeed = 0;
   
  if (getModePage(0x30, mp, 14, NULL, NULL, 0) != 0) {
    return 1;
  }

  if (mp[1] != 14) 
    return 1;

  if (mp[2] & (1 << 5))
    driveInfo_->ricohJustSpeed = 1;

  if (mp[2] & 0x01)
    driveInfo_->ricohJustLink = 1;

  return 0;
}

int GenericMMC::RicohSetWriteOptions(const DriveInfo *di)
{
  unsigned char mp[14];

  if (di->ricohJustLink == 0 && di->ricohJustSpeed == 0)
    return 0;

  if (getModePage(0x30, mp, 14, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve Ricoh mode page 30.");
    return 1;
  }

  if (di->ricohJustLink) {
    if (bufferUnderRunProtection()) {
      log_message(2, "Enabling JustLink.");
      mp[3] |= 0x1;
    } 
    else {
      log_message(2, "Disabling JustLink.");
      mp[3] &= ~0x1;
    }
  }

  if (di->ricohJustSpeed) {
    if (writeSpeedControl()) {
      log_message(2, "Enabling JustSpeed.");
      mp[3] &= ~(1 << 5); // clear bit to enable write speed control
    }
    else {
      log_message(2, "Disabling JustSpeed.");
      mp[3] |= (1 << 5);  // set bit to disable write speed control
    }
  }

  if (setModePage(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set Ricoh mode page 30.");
    return 1;
  }
  
  return 0;
}
