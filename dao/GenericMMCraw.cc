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
#include <assert.h>

#include "GenericMMCraw.h"
#include "PQSubChannel16.h"
#include "PWSubChannel96.h"
#include "CdTextEncoder.h"

#include "Toc.h"
#include "log.h"
#include "port.h"

GenericMMCraw::GenericMMCraw(ScsiIf *scsiIf, unsigned long options) 
  : GenericMMC(scsiIf, options), PQChannelEncoder()
{
  driverName_ = "Generic SCSI-3/MMC (raw writing) - Version 2.0";

  encodingMode_ = 0;

  subChannelMode_ = 0;

  leadInLen_ = leadOutLen_ = 0;
  subChannel_ = NULL;
  encSubChannel_ = NULL;
  encodeBuffer_ = NULL;

  // CD-TEXT dynamic data
  cdTextStartLba_ = 0;
  cdTextEndLba_ = 0;
  cdTextSubChannels_ = NULL;
  cdTextSubChannelCount_ = 0;
  cdTextSubChannelAct_ = 0;
}

GenericMMCraw::~GenericMMCraw()
{
  delete subChannel_, subChannel_ = NULL;
  delete[] encodeBuffer_, encodeBuffer_ = NULL;

  delete[] encSubChannel_;
  encSubChannel_ = NULL;

  cdTextStartLba_ = 0;
  cdTextEndLba_ = 0;
  cdTextSubChannels_ = NULL;
  cdTextSubChannelCount_ = 0;
  cdTextSubChannelAct_ = 0;
}

int GenericMMCraw::multiSession(bool m)
{
    if (m) {
	// multi session mode is currently not support for raw writing
	return 1;
    }

  return 0;
}

// static constructor
CdrDriver *GenericMMCraw::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new GenericMMCraw(scsiIf, options);
}

int GenericMMCraw::subChannelEncodingMode(TrackData::SubChannelMode sm) const
{
  int ret = 0;

  
  if (subChannelMode_ == 0) {
    // The supported sub-channel writing mode has not been determined, yet,
    // so just return the plain mode here. 'initDao' will finally check if
    // writing of the sub-channel data defined in 'toc_' is supported by the
    // drive.
    return 0;
  }

  switch (sm) {
  case TrackData::SUBCHAN_NONE:
    ret = 0;
    break;

  case TrackData::SUBCHAN_RW:
    switch (subChannelMode_) {
    case 2:
      ret = 0; // plain
      break;
    case 3:
      ret = -1; // currently not supported
      //ret = 1; // have to create parity and perform interleaving
      break;
    default:
      ret = -1; // not supported
      break;
    }
    break;

  case TrackData::SUBCHAN_RW_RAW:
    if (subChannelMode_ == 3)
      ret = 1;
    else 
      ret = -1;
    break;
  }

  return ret;
}

// Sets write parameters via mode page 0x05.
// return: 0: OK
//         1: scsi command failed
int GenericMMCraw::setWriteParameters(int dataBlockType)
{
  unsigned char mp[0x38];

  if (getModePage(5/*write parameters mode page*/, mp, 0x38,
		  NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve write parameters mode page.");
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag

  mp[2] &= 0xe0;
  mp[2] |= 0x03; // write type: raw
  if (simulate_) {
    mp[2] |= 1 << 4; // test write
  }

  const DriveInfo *di;
  if ((di = driveInfo(1)) != NULL) {
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
  mp[3] = 0;

  mp[4] &= 0xf0;
  mp[4] = dataBlockType & 0x0f;  // Data Block Type:
                                 // 1: raw data, block size: 2368 PQ sub chan
                                 // 2: raw data, block size: 2448
                                 // 3: raw data, block size: 2448

  mp[8] = 0; // session format: CD-DA or CD-ROM

  if (setModePage(mp, NULL, NULL, 0) != 0) {
    //log_message(-2, "Cannot set write parameters mode page.");
    return 1;
  }

  return 0;
}

int GenericMMCraw::getMultiSessionInfo(int sessionNr, int multi,
				       SessionInfo *info)
{
  int err = 0;

  memset(info, 0, sizeof(SessionInfo));

  info->sessionNr = 1;

  if (getSessionInfo() != 0) 
    return 1;

  info->leadInStart = leadInStart_.lba() - 150;

  if (leadInStart_.min() >= 80) {
    info->leadInStart = leadInStart_.lba() - 450000;
  }

  info->leadInLen = leadInLen_;
  info->leadOutLen = leadOutLen_;

  if (multi) {
    unsigned char cmd[10];
    unsigned char data[4];
    unsigned char *buf = NULL;
    long dataLen;
 
    // read ATIP data
    memset(cmd, 0, 10);
    memset(data, 0, 4);

    cmd[0] = 0x43; // READ TOC/PMA/ATIP
    cmd[1] = 0x00;
    cmd[2] = 4; // get ATIP
    cmd[7] = 0;
    cmd[8] = 4; // data length
    
    if (sendCmd(cmd, 10, NULL, 0, data, 4, 0) != 0) {
      log_message(-2, "Cannot read ATIP data.");
      return 1;
    }
    
    dataLen = (data[0] << 8) | data[1];
    dataLen += 2;

    log_message(4, "ATIP data len: %ld", dataLen);

    if (sessionNr == 1) {
      if (dataLen < 19) {
	log_message(-2, "Cannot read ATIP data.");
	return 1;
      }
    }
    else {
      if (dataLen < 15) {
	log_message(-2, "Cannot read ATIP data.");
	return 1;
      }
    }      
    
    buf = new unsigned char[dataLen];
    memset(buf, 0, dataLen);
    
    cmd[7] = dataLen >> 8;
    cmd[8] = dataLen;
    
    if (sendCmd(cmd, 10, NULL, 0, buf, dataLen, 0) != 0) {
      log_message(-2, "Cannot read ATIP data.");
      delete[] buf;
      return 1;
    }

    info->lastLeadoutStart = Msf(buf[12], buf[13], buf[14]);
    
    if (sessionNr == 1) {
      info->optimumRecordingPower = buf[4];

      if (buf[8] >= 80 && buf[8] <= 99) {
	info->atipLeadinStart = Msf(buf[8], buf[9], buf[10]);
      }
      else {
	log_message(-2, "Invalid start time of lead-in in ATIP.");
	err = 1;
      }

      info->cdrw = (buf[6] & 0x40) ? 1 : 0;
      
      if (info->cdrw) {
	if (buf[6] & 0x04) {
	  info->atipA1[0] = buf[16];
	  info->atipA1[1] = buf[17];
	  info->atipA1[2] = buf[18];
	}
	else {
	  log_message(-2, "ATIP data does not contain point A1 data.");
	  err = 1;
	}
      }
    }

    delete[] buf;
    buf = NULL;
  }

  log_message(4, "SI: session nr: %d", info->sessionNr);
  log_message(4, "SI: lead-in start: %ld", info->leadInStart);
  log_message(4, "SI: lead-in len: %ld", info->leadInLen);
  log_message(4, "SI: lead-out len: %ld", info->leadOutLen);
  log_message(4, "SI: last lead-out start: %d %d %d", info->lastLeadoutStart.min(),
	  info->lastLeadoutStart.sec(), info->lastLeadoutStart.frac());
  log_message(4, "SI: cdrw: %d", info->cdrw);
  log_message(4, "SI: atip lead-in start: %d %d %d", info->atipLeadinStart.min(),
	  info->atipLeadinStart.sec(), info->atipLeadinStart.frac());
  log_message(4, "SI: optimum recording power: %u", info->optimumRecordingPower);
  log_message(4, "SI: atip A1: %u %u %u", info->atipA1[0], info->atipA1[1],
	  info->atipA1[2]);

  return err;
}

int GenericMMCraw::getSubChannelModeFromToc()
{
  TrackIterator itr(CdrDriver::toc_);
  const Track *tr;
  int mode = 0;
  
  for (tr = itr.first(); tr != NULL; tr = itr.next()) {
    switch (tr->subChannelType()) {
    case TrackData::SUBCHAN_NONE:
      break;

    case TrackData::SUBCHAN_RW:
      // we need at least packed RW writing
      if (mode < 2)
	mode = 2;
      break;

    case TrackData::SUBCHAN_RW_RAW:
      // we need raw PW writing
      mode = 3;
      break;
    }
  }

  log_message(5, "Sub-channel mode requested by toc: %d", mode);

  return mode;
}

int GenericMMCraw::setSubChannelMode()
{
  delete subChannel_;
  subChannel_ = NULL;

  subChannelMode_ = 0;
  
#if 1
  if (cdTextEncoder_ != NULL) {
    if (setWriteParameters(3) == 0) {
      subChannel_ = new PWSubChannel96;
      subChannelMode_ = 3;
    }
    else {
      delete cdTextEncoder_;
      cdTextEncoder_ = NULL;

      log_message(force() ? -1 : -2,
	      "Cannot write CD-TEXT data because the 96 byte raw P-W sub-channel data mode is not supported.");

      if (force()) {
	log_message(-1, "Ignored because of --force option.");
      }
      else {
	log_message(-2, "Use option --force to ignore this error.");
	return 1;
      }
    }
  }

  // check if the toc requires a certain sub-channel mode and try to set it
  if (subChannel_ == NULL) {
    int tocMode = getSubChannelModeFromToc();

    if (tocMode > 0) {
      for (; tocMode <= 3 && subChannel_ == NULL; tocMode++) {
	if (setWriteParameters(tocMode) == 0) {
	  if (tocMode == 1)
	    subChannel_ = new PQSubChannel16;
	  else
	    subChannel_ = new PWSubChannel96;

	  subChannelMode_ = tocMode;
	}
      }

      if (subChannel_ == NULL) {
	log_message(-2, "Cannot setup sub-channel writing mode for sub-channel data defined in the toc-file.");
	return 1;
      }
    }
  }

  // select any available sub-channel mode
  if (subChannel_ == NULL) {
    if (setWriteParameters(1) == 0) {
      subChannel_ = new PQSubChannel16;
      subChannelMode_ = 1;
    }
    else if (setWriteParameters(3) == 0) {
      subChannel_ = new PWSubChannel96;
      subChannelMode_ = 3;
    }
    else if (setWriteParameters(2) == 0) {
      subChannel_ = new PWSubChannel96;
      subChannelMode_ = 2;
    }
    else {
      log_message(-2, "Cannot setup disk-at-once writing for this drive.");
      return 1;
    }
  }

#else

  //subChannel_ = new PWSubChannel96;
  subChannel_ = new PQSubChannel16;
  subChannelMode_ = 1;
#endif

  switch (subChannelMode_) {
  case 1:
    log_message(2, "Using 16 byte P-Q sub-channel data mode.");
    break;
  case 2:
    log_message(2, "Using 96 byte packed P-W sub-channel data mode.");
    break;
  case 3:
    if (cdTextEncoder_ != NULL)
      log_message(2, "Using 96 byte raw P-W sub-channel data mode for CD-TEXT.");
    else
      log_message(2, "Using 96 byte raw P-W sub-channel data mode.");
    break;
  }

  return 0;
}

int GenericMMCraw::initDao(const Toc *toc)
{
  long n;

  CdrDriver::toc_ = toc;

  if (selectSpeed() != 0 ||
      getSessionInfo() != 0) {
    return 1;
  }

  delete cdTextEncoder_;
  cdTextEncoder_ = new CdTextEncoder(toc);
  if (cdTextEncoder_->encode() != 0) {
    log_message(-2, "CD-TEXT encoding failed.");
    return 1;
  }

  if (cdTextEncoder_->getSubChannels(&n) == NULL || n == 0) {
    delete cdTextEncoder_;
    cdTextEncoder_ = NULL;
  }

  if (setSubChannelMode() != 0)
    return 1; 

  blockLength_ = AUDIO_BLOCK_LEN + subChannel_->dataLength();
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;
  assert(blocksPerWrite_ > 0);
  log_message(4, "Block length: %ld", blockLength_);

  long cueSheetLen;
  unsigned char *cueSheet = createCueSheet(0, &cueSheetLen);

  if (cueSheet == NULL) {
    return 1;
  }
  
  if (setCueSheet(subChannel_, sessionFormat(), cueSheet, cueSheetLen,
		  leadInStart_) != 0) {
    return 1;
  }

  if (cdTextEncoder_ != NULL) {
    cdTextStartLba_ = leadInStart_.lba() - 450150;
    cdTextEndLba_ = cdTextStartLba_ + leadInLen_;
    cdTextSubChannels_ = cdTextEncoder_->getSubChannels(&cdTextSubChannelCount_);
    cdTextSubChannelAct_ = 0;
  }
  else {
    cdTextStartLba_ = 0;
    cdTextEndLba_ = 0;
    cdTextSubChannels_ = NULL;
    cdTextSubChannelCount_ = 0;
    cdTextSubChannelAct_ = 0;
  }

  // allocate buffer for write zeros
  n = blocksPerWrite_ * (AUDIO_BLOCK_LEN + subChannel_->dataLength());
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);

  // allocate buffer for sub-channel encoding
  n = blocksPerWrite_ * blockLength_;
  delete[] encodeBuffer_;
  encodeBuffer_ = new unsigned char[n];

  delete[] encSubChannel_;
  encSubChannel_ = new unsigned char[blocksPerWrite_ * subChannel_->dataLength()];

  /*
  SessionInfo sessInfo;
  
  getMultiSessionInfo(1, 1, &sessInfo);

  return 1;
  */

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

  return 0;
}

int GenericMMCraw::startDao()
{
  log_message(2, "Writing lead-in and gap...");

  long lba = leadInStart_.lba() - 450150;
  
  if (writeZeros(CdrDriver::toc_->leadInMode(), TrackData::SUBCHAN_NONE,
		 lba, 0, leadInLen_) != 0) {
    return 1;
  }

  TrackData::SubChannelMode subChanMode = TrackData::SUBCHAN_NONE;
  TrackIterator itr(CdrDriver::toc_);
  const Track *tr;

  if ((tr = itr.first()) != NULL) {
    subChanMode = tr->subChannelType();
  }

  if (writeZeros(CdrDriver::toc_->leadInMode(), subChanMode, lba, 0, 150)
      != 0) {
    return 1;
  }

  return 0;
}

int GenericMMCraw::finishDao()
{
  int ret;

  log_message(2, "Writing lead-out...");

  long lba = CdrDriver::toc_->length().lba();

  writeZeros(CdrDriver::toc_->leadOutMode(), TrackData::SUBCHAN_NONE,
	     lba, lba + 150, leadOutLen_);

  log_message(2, "\nFlushing cache...");
  
  if (flushCache() != 0) {
    return 1;
  }

  while ((ret = checkDriveReady()) == 2)
    mSleep(2000);

  if (ret != 0)
    log_message(-1, "TEST UNIT READY failed after recording.");

  delete cdTextEncoder_, cdTextEncoder_ = NULL;
  delete[] zeroBuffer_, zeroBuffer_ = NULL;
  delete[] encodeBuffer_, encodeBuffer_ = NULL;

  return 0;
}

long GenericMMCraw::nextWritableAddress()
{
  unsigned char cmd[10];
  unsigned char data[28];
  long lba = 0xffffffff;

  memset(cmd, 0, 10);
  memset(data, 0, 28);

  cmd[0] = 0x52; // READ TRACK INFORMATION
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[8] = 28;

  if (sendCmd(cmd, 10, NULL, 0, data, 28) != 0) {
    log_message(-2, "Cannt get track information.");
    return 0;
  }

  long adr = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) |
    data[15];

  return adr;

}

// Writes data to target. The encoded sub-channel data is appended to each
// block.
// return: 0: OK
//         1: scsi command failed
int GenericMMCraw::writeData(TrackData::Mode mode,
			     TrackData::SubChannelMode sm,
			     long &lba, const char *buf, long len)
{
  assert(blockLength_ > 0);
  assert(blocksPerWrite_ > 0);
  assert(mode == TrackData::AUDIO);
  int writeLen = 0;
  unsigned char cmd[10];
  int i, j;

  long iblen = blockSize(mode, sm);
  long slen = subChannel_->dataLength();

  /*
  log_message(0, "lba: %ld, len: %ld, bpc: %d, bl: %d ", lba, len, blocksPerCmd,
	 blockLength_);
   */

  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE1
  
  while (len > 0) {
    writeLen = (len > blocksPerWrite_ ? blocksPerWrite_ : len);

    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;

    cmd[7] = writeLen >> 8;
    cmd[8] = writeLen;

    // encode the PQ sub-channel data
    encode(lba, encSubChannel_, writeLen);

    for (i = 0; i < writeLen; i++) {
      memcpy(encodeBuffer_ + i * blockLength_, buf + i * iblen,
	     AUDIO_BLOCK_LEN);

      memcpy(encodeBuffer_ + i * blockLength_ + AUDIO_BLOCK_LEN,
	     encSubChannel_ + i * slen, slen);

      if (cdTextSubChannels_ != NULL && lba >= cdTextStartLba_ &&
	  lba + i < cdTextEndLba_) {

	const unsigned char *data = cdTextSubChannels_[cdTextSubChannelAct_]->data();
	long dataLen = cdTextSubChannels_[cdTextSubChannelAct_]->dataLength();

	unsigned char *actBuf = encodeBuffer_ + i * blockLength_ + AUDIO_BLOCK_LEN;

	//log_message(0, "Adding CD-TEXT channel %ld for LBA %ld", cdTextSubChannelAct_, lba + i);
	for (j = 0; j < dataLen; j++) {
	  *actBuf |= (*data & 0x3f);
	  actBuf++;
	  data++;
	}

	cdTextSubChannelAct_++;
	if (cdTextSubChannelAct_ >= cdTextSubChannelCount_)
	  cdTextSubChannelAct_ = 0;
      }
      else {
	switch (sm) {
	case TrackData::SUBCHAN_NONE:
	  break;

	case TrackData::SUBCHAN_RW:
	case TrackData::SUBCHAN_RW_RAW:
	  {
	    unsigned char *oBuf = encodeBuffer_ + i * blockLength_ + AUDIO_BLOCK_LEN;
	    const char *iBuf = buf + i * iblen + AUDIO_BLOCK_LEN;

	    for (j = 0; j < PW_SUBCHANNEL_LEN; j++) {
	      *oBuf |= (*iBuf & 0x3f);
	      oBuf++;
	      iBuf++;
	    }
	  }
	  break;
	}
      }
    }


#if 0
    // consistency checks
    long sum1, sum2;
    int n;
    const char *p;
    for (i = 0; i < writeLen; i++) {
      log_message(0, "%ld: ", lba + i);
      SubChannel *chan = subChannel_->makeSubChannel(encodeBuffer_ + i * blockLength_ + AUDIO_BLOCK_LEN);
      chan->print();
      delete chan;

      sum1 = 0;
      for (p = buf + i * iblen, n = 0;
	   n < AUDIO_BLOCK_LEN; n++, p++) {
	sum1 += *p;
      }

      sum2 = 0;
      for (n = 0; n < AUDIO_BLOCK_LEN; n++) {
	sum2 += *(char *)(encodeBuffer_ + i * blockLength_ + n);
      }

      //log_message(0, "%ld - %ld", sum1, sum2);
      assert(sum1 == sum2);
    }
#endif

#if 1
    if (sendCmd(cmd, 10, encodeBuffer_, writeLen * blockLength_,
		NULL, 0) != 0) {
      log_message(-2, "Write data failed.");
      return 1;
    }
#endif
    //log_message(0, ". ");

    lba += writeLen;
    len -= writeLen;
    buf += writeLen * iblen;
  }

  //log_message(0, "");

  return 0;
}
