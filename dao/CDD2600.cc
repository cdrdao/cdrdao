/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <mueller@daneb.ping.de>
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

#include "CDD2600.h"
#include "SubChannel.h"
#include "PQSubChannel16.h"

#include "Toc.h"
#include "log.h"

CDD2600::CDD2600(ScsiIf *scsiIf, unsigned long options)
  : CdrDriver(scsiIf, options|OPT_DRV_NO_PREGAP_READ), CDD2600Base(this)
{
  driverName_ = "CDD2600 - Version 1.1";
  
  leadInLength_ = leadOutLength_ = 0;
  speed_ = 2;
  simulate_ = true;
  encodingMode_ = 0;

  // reads big endian samples
  audioDataByteOrder_ = 1;

  memset(&diskInfo_, 0, sizeof(DiskInfo));
}

CDD2600::~CDD2600()
{
}

// static constructor
CdrDriver *CDD2600::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new CDD2600(scsiIf, options);
}

// sets speed
// return: 0: OK
//         1: illegal speed
int CDD2600::speed(int s)
{
  if (s >= 0 && s <= 2) {
    speed_ = s;
    return 0;
  }
  else if (s > 2) {
    speed_ = 2;
    return 0;
  }
  else {
    return 1;
  }
}

// loads ('unload' == 0) or ejects ('unload' == 1) tray
// return: 0: OK
//         1: scsi command failed

int CDD2600::loadUnload(int unload) const
{
  unsigned char cmd[10];

  memset(cmd, 0, 10);

  cmd[0] = 0xe7; // MEDIUM LOAD/UNLOAD
  if (unload) {
    cmd[8] |= 0x01;
  }
  
  if (sendCmd(cmd, 10, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot load/unload medium.");
    return 1;
  }

  return 0;
}

// sets various audio play parameters, output channels are set to stereo mode
// and given volume
// immediate: 0: wait until audio play command finished
//            1: command finishs immediately after playback has started
// sotc:      0: play across track boundaries
//            1: stop playing at track boundaries
int CDD2600::modeSelectPlay(int immediate, int sotc, unsigned char volume)
{
  unsigned char mp[16];

  memset(mp, 0, 16);

  mp[0] = 0x0e; // PLAY page code
  mp[1] = 14; // parameter length
  if (immediate != 0) {
    mp[2] |= 0x04;
  }
  if (sotc != 0) {
    mp[2] |= 0x02;
  }
  mp[8]  = 1;
  mp[9]  = volume;
  mp[10] = 2;
  mp[11] = volume;

  if (setModePage(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set play parameters.");
    return 1;
  }

  return 0;
}

int CDD2600::initDao(const Toc *toc)
{
  long n;
  blockLength_ = AUDIO_BLOCK_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

  toc_ = toc;

  diskInfo();

  if (!diskInfo_.valid.empty || !diskInfo_.valid.append) {
    log_message(-2, "Cannot determine status of inserted medium.");
    return 1;
  }

  if (!diskInfo_.append) {
    log_message(-2, "Inserted medium is not appendable.");
    return 1;
  }

  if (modeSelectBlockSize(blockLength_, 1) != 0 ||
      modeSelectSpeed(-1, speed_, simulate_, 1) != 0 ||
      modeSelectCatalog(toc_) != 0 ||
      readSessionInfo(&leadInLength_, &leadOutLength_, 1) != 0) {
    return 1;
  }

  // allocate buffer for write zeros
  n = blocksPerWrite_ * blockLength_;
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);

  return 0;
}

int CDD2600::startDao()
{
  long lba;

  if (writeSession(toc_, multiSession_, diskInfo_.thisSessionLba) != 0) {
    return 1;
  }

  log_message(2, "Writing lead-in and gap...");

  lba = diskInfo_.thisSessionLba - 150 - leadInLength_;

  // write lead-in
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, lba + 150,
		 leadInLength_) != 0) {
    flushCache();
    return 1;
  }

  log_message(5, "Lba after lead-in: %ld", lba);

  // write gap (2 seconds)
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE,
		 lba, lba + 150, 150) != 0) {
    flushCache();
    return 1;
  }

  log_message(2, "");

  return 0;
}

int CDD2600::finishDao()
{
  long lba = diskInfo_.thisSessionLba + toc_->length().lba();

  log_message(2, "Writing lead-out...");

  // write lead-out
  if (writeZeros(toc_->leadOutMode(), TrackData::SUBCHAN_NONE, lba, lba + 150,
		 leadOutLength_) != 0) {
    flushCache();
    return 1;
  }

  log_message(2, "\nFlushing cache...");
  
  if (flushCache() != 0) {
    return 1;
  }

  log_message(2, "");

  blockLength_ = MODE1_BLOCK_LEN;
  modeSelectBlockSize(blockLength_, 1);

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void CDD2600::abortDao()
{
  flushCache();

  blockLength_ = MODE1_BLOCK_LEN;
  modeSelectBlockSize(blockLength_, 1);
}

// Writes data to target, the block length depends on the actual writing mode
// and is stored internally. 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function but not used for writing
// return: 0: OK
//         1: scsi command failed
int CDD2600::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
		       long &lba, const char *buf, long len)
{
  assert(blocksPerWrite_ > 0);
  assert(blockLength_ > 0);
  assert(mode == TrackData::AUDIO);
  int nwritten = 0;
  int writeLen = 0;
  unsigned char cmd[10];

  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE1
  
  while (len > 0) {
    writeLen = (len > blocksPerWrite_ ? blocksPerWrite_ : len);

    cmd[7] = writeLen >> 8;
    cmd[8] = writeLen & 0xff;

    if (sendCmd(cmd, 10, (unsigned char *)(buf + (nwritten * blockLength_)),
		writeLen * blockLength_, NULL, 0) != 0) {
      log_message(-2, "Write data failed.");
      return 1;
    }

    lba += writeLen;

    len -= writeLen;
    nwritten += writeLen;
  }
      
  return 0;
}

Toc *CDD2600::readDiskToc(int session, const char *audioFilename)
{

  blockLength_ = AUDIO_BLOCK_LEN;
  if (modeSelectBlockSize(blockLength_, 1) != 0) {
    return NULL;
  }

  modeSelectSpeed(2, -1, 1, 0);

  Toc *toc = CdrDriver::readDiskToc(session, audioFilename);

  setBlockSize(MODE1_BLOCK_LEN);
  
  return toc;
}

Toc *CDD2600::readDisk(int session, const char *fname)
{
  Toc *toc = CdrDriver::readDisk(session, fname);

  setBlockSize(MODE1_BLOCK_LEN);

  return toc;
}

int CDD2600::readIsrc(int trackNr, char *buf)
{
  unsigned char cmd[10];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];
  int i;

  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x03; // get ISRC code
  cmd[6] = trackNr;
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-1, "Cannot read ISRC code.");
    return 1;
  }
  else {
    if (data[0x08] & 0x80) {
      for (i = 0; i < 12; i++) {
	buf[i] = data[0x09 + i];
      }
      buf[12] = 0;
    }
  }

  return 0;
}

// tries to read catalog number from disk and adds it to 'toc'
// return: 1 if valid catalog number was found, else 0

int CDD2600::readCatalog(Toc *toc, long startLba, long endLba)
{
  unsigned char cmd[10];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];
  char catalog[14];
  int i;

  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x02; // get media catalog number
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read sub channel data.");
    return 0;
  }

  if (data[0x08] & 0x80) {
    for (i = 0; i < 13; i++) {
      catalog[i] = data[0x09 + i];
    }
    catalog[13] = 0;

    if (toc->catalog(catalog) == 0) {
      return 1;
    }
  }

  return 0;
}

int CDD2600::analyzeTrack(TrackData::Mode mode, int trackNr, long startLba,
			  long endLba,
			  Msf *index, int *indexCnt, long *pregap,
			  char *isrcCode, unsigned char *ctl)
{
  blockLength_ = AUDIO_BLOCK_LEN;
  modeSelectBlockSize(blockLength_, 1);

  int ret = analyzeTrackSearch(mode, trackNr, startLba, endLba,
			       index, indexCnt, pregap, isrcCode, ctl);

  *isrcCode = 0;

  if (mode == TrackData::AUDIO) {
    // read ISRC code from sub channel
    readIsrc(trackNr, isrcCode);
  }

  return ret;
}

int CDD2600::getTrackIndex(long lba, int *trackNr, int *indexNr, 
			   unsigned char *ctl)
{
  long relPos;

  readBlock(lba);

  return readSubChannelData(trackNr, indexNr, &relPos, ctl);
}

int CDD2600::readSubChannelData(int *trackNr, int *indexNr, long *relPos,
				unsigned char *ctl)
{
  unsigned char cmd[10];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];

  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x00; // get sub Q channel data
  cmd[6] = 0;
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read sub Q channel data.");
    return 1;
  }

  *trackNr = data[6];
  *indexNr = data[7];
  *relPos = 0;
  *relPos |= data[0x0c] << 24;
  *relPos |= data[0x0d] << 16;
  *relPos |= data[0x0e] << 8;
  *relPos |= data[0x0f];

  if (ctl != NULL) {
    *ctl = data[5] & 0x0f;
  }

  return 0;
}

// reads a single block of length 'blockLength_' from given sector
// return: 0: OK
//         1: error occured
void CDD2600::readBlock(unsigned long sector)
{
  unsigned char cmd[10];
  unsigned long dataLen = 2 * blockLength_;
  unsigned char *data = new unsigned char[dataLen];


  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x28; // READ10
  cmd[2] = sector >> 24;
  cmd[3] = sector >> 16;
  cmd[4] = sector >> 8;
  cmd[5] = sector;
  cmd[7] = 0;
  cmd[8] = 2;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read block - ignored.");
  }

  delete[] data;
}

int CDD2600::nextWritableAddress(long *lba, int showError)
{
  unsigned char cmd[10];
  unsigned char data[6];

  memset(data, 0, 6);
  memset(cmd, 0, 10);

  cmd[0] = 0xe2; // FIRST WRITABLE ADDRESS
  cmd[3] = 1 /*<< 2*/; // AUDIO
  // cmd[7] = 1; // NPA
  cmd[8] = 6; // allocation length

  if (sendCmd(cmd, 10, NULL, 0, data, 6, showError) != 0) {
    if (showError)
      log_message(-2, "Cannot retrieve next writable address.");
    return 1;
  }

  *lba = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];

  return 0;
}

// Retrieve disk information.
// return: DiskInfo structure or 'NULL' on error
DiskInfo *CDD2600::diskInfo()
{
  unsigned char cmd[10];
  unsigned char data[34];
  long nwa;
  int i;

  memset(&diskInfo_, 0, sizeof(DiskInfo));

  if (readCapacity(&(diskInfo_.capacity), 0) == 0) {
    diskInfo_.valid.capacity = 1;
  }
  
  if (readSessionInfo(&leadInLength_, &leadOutLength_, 0) == 0) {
    diskInfo_.valid.manufacturerId = 1;
    
    // start time of lead-in
    diskInfo_.manufacturerId = Msf(450150 - leadInLength_ - 150 );
    diskInfo_.append = 1; // this is for the CDD2000 which does not support
                         // READ DISK INFORMATION
  }
  else {
    diskInfo_.append = 0; // this is for the CDD2000 which does not support
                         // READ DISK INFORMATION
  }
  diskInfo_.valid.empty = 1;
  diskInfo_.valid.append = 1;

  memset(cmd, 0, 10);
  memset(data, 0, 4);

  cmd[0] = 0x43; // READ TOC
  cmd[6] = 0;
  cmd[8] = 4;

  if (sendCmd(cmd, 10, NULL, 0, data, 4, 0) == 0) {
    log_message(5, "First track %u, last track %u", data[2], data[3]);
    diskInfo_.lastTrackNr = data[3];
  }
  else {
    log_message(5, "READ TOC (format 0) failed.");
  }

  if (diskInfo_.lastTrackNr > 0) {
    // the lead-in length does not specify the manufacturer ID anymore
    diskInfo_.valid.manufacturerId = 0;

    diskInfo_.empty = 0; // CD-R is not empty
    diskInfo_.diskTocType = 0xff; // undefined

    if (diskInfo_.lastTrackNr < 99 && nextWritableAddress(&nwa, 0) == 0) {
      log_message(5, "NWA: %ld", nwa);
      diskInfo_.thisSessionLba = nwa;
      diskInfo_.append = 1;
    }
    else {
      diskInfo_.append = 0;
    }  

    
    memset(cmd, 0, 10);
    memset(data, 0, 12);

    cmd[0] = 0x43; // READ TOC
    cmd[6] = 0;
    cmd[8] = 12;
    cmd[9] = 1 << 6;

    if (sendCmd(cmd, 10, NULL, 0, data, 12) == 0) {
      diskInfo_.sessionCnt = data[3];
      diskInfo_.lastSessionLba = (data[8] << 24) | (data[9] << 16) |
                                 (data[10] << 8) | data[11];

      log_message(5, "First session %u, last session %u, last session start %ld",
	      data[2], data[3], diskInfo_.lastSessionLba);
    }
    else {
      log_message(5, "READ TOC (format 1) failed.");
    }

    if (diskInfo_.sessionCnt > 0) {
      int len;
      CdRawToc *toc = getRawToc(diskInfo_.sessionCnt, &len);

      if (toc != NULL) {
	for (i = 0; i < len; i++) {
	  if (toc[i].sessionNr == diskInfo_.sessionCnt) {
#if 0
	    if (toc[i].point == 0xb0 && toc[i].min != 0xff &&
		toc[i].sec != 0xff && toc[i].frame != 0xff) {
	      int m = toc[i].min;
	      int s = toc[i].sec;
	      int f = toc[i].frame;
	      
	      if (m < 90 && s < 60 && f < 75) {
		diskInfo_.thisSessionLba = Msf(m, s, f).lba(); // + 150 - 150
		diskInfo_.thisSessionLba += leadInLength_;
	      }
	    }
#endif
	    if (toc[i].point == 0xa0) {
	      diskInfo_.diskTocType = toc[i].psec;
	    }
	  }

	  // The point C0 entry may be only stored in the first session's
	  // lead-in
	  if (toc[i].point == 0xc0 && toc[i].pmin <= 99 &&
	      toc[i].psec < 60 && toc[i].pframe < 75) {
	    diskInfo_.manufacturerId =
	      Msf(toc[i].pmin, toc[i].psec, toc[i].pframe);
	    diskInfo_.valid.manufacturerId = 1;
	  }
	}

#if 0
	if (diskInfo_.thisSessionLba > 0) {
	  if (diskInfo_.lastTrackNr < 99)
	    diskInfo_.append = 1;
	}
	else {
	  log_message(4, "Did not find BO pointer in session %d.",
		  diskInfo_.sessionCnt);
	  
	}
#endif

	delete[] toc;
      }
      else {
	log_message(5, "getRawToc failed.");
      }
    }
  }
  else {
    // disk is empty and appendable
    diskInfo_.empty = 1;
  }

  if (diskInfo_.append == 0)
    diskInfo_.empty = 0;

  return &diskInfo_;
}

CdRawToc *CDD2600::getRawToc(int sessionNr, int *len)
{
  unsigned char cmd[10];
  unsigned short dataLen;
  unsigned char *data = NULL;;
  unsigned char reqData[4]; // buffer for requestion the actual length
  unsigned char *p = NULL;
  int i;
  CdRawToc *rawToc;
  int entries;

  assert(sessionNr >= 1);

  // read disk toc length
  memset(cmd, 0, 10);
  cmd[0] = 0x43; // READ TOC
  cmd[6] = sessionNr;
  cmd[8] = 4;
  cmd[9] |= 2 << 6; // get Q subcodes

  if (sendCmd(cmd, 10, NULL, 0, reqData, 4) != 0) {
    log_message(-2, "Cannot read raw disk toc.");
    return NULL;
  }

  dataLen = ((reqData[0] << 8) | reqData[1]) + 2;

  log_message(5, "Raw toc data len: %d", dataLen);

  data = new unsigned char[dataLen];
  
  // read disk toc
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read raw disk toc.");
    delete[] data;
    return NULL;
  }

  entries = (((data[0] << 8) | data[1]) - 2) / 11;


  rawToc = new CdRawToc[entries];

  for (i = 0, p = data + 4; i < entries; i++, p += 11 ) {
#if 0
    log_message(0, "%d %02x %02d %2x %02d:%02d:%02d %02x %02d:%02d:%02d",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10]);
#endif
    rawToc[i].sessionNr = p[0];
    rawToc[i].adrCtl = p[1];
    rawToc[i].point = p[3];
    rawToc[i].min = p[4];
    rawToc[i].sec = p[5];
    rawToc[i].frame = p[6];
    rawToc[i].pmin = p[8];
    rawToc[i].psec = p[9];
    rawToc[i].pframe = p[10];
  }

  delete[] data;

  *len = entries;

  return rawToc;
}

long CDD2600::readTrackData(TrackData::Mode mode, TrackData::SubChannelMode,
			    long lba, long len, unsigned char *buf)
{
  unsigned char cmd[10];
  long blockLen = 2340;
  long i;
  TrackData::Mode actMode;
  int ok = 0;
  const unsigned char *sense;
  int senseLen;
  int softError;

  if (setBlockSize(blockLen) != 0)
    return 0;

  memset(cmd, 0, 10);

  cmd[0] = 0x28; // READ10
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;

  /*  
  if (mode == TrackData::MODE2_FORM1 || mode == TrackData::MODE2_FORM2)
    cmd[9] = 1 << 6; // MIX flag
    */

  while (len > 0 && !ok) {
    cmd[7] = len >> 8;
    cmd[8] = len;

    memset(transferBuffer_, 0, len * blockLen);
    switch (sendCmd(cmd, 10, NULL, 0, transferBuffer_, len * blockLen, 0)) {
    case 0:
      ok = 1;
      break;

    case 2:
      softError = 0;
      sense = scsiIf_->getSense(senseLen);

      if (senseLen > 0x0c) {
	if ((sense[2] &0x0f) == 5) {
	  switch (sense[12]) {
	  case 0x64: // Illegal mode for this track
	    softError = 1;
	    break;
	  }
	}
	else if ((sense[2] & 0x0f) == 3) { // Medium error
	  switch (sense[12]) {
	  case 0x02: // No seek complete, sector not found
	  case 0x11: // L-EC error
	    return -2;
	    break;
	  }
	}
      }

      if (!softError) {
	scsiIf_->printError();
	return -1;
      }
      break;

    default:
      log_message(-2, "Read error at LBA %ld, len %ld", lba, len);
      return -1;
      break;
    }

    if (!ok) {
      len--;
    }
  }

  unsigned char *sector = transferBuffer_;
  for (i = 0; i < len; i++) {
    actMode = determineSectorMode(sector);

    if (!(actMode == mode ||
	  (mode == TrackData::MODE2_FORM_MIX &&
	   (actMode == TrackData::MODE2_FORM1 ||
	    actMode == TrackData::MODE2_FORM2)) ||

	  (mode == TrackData::MODE1_RAW && actMode == TrackData::MODE1) ||

	  (mode == TrackData::MODE2_RAW &&
	   (actMode == TrackData::MODE2 ||
	    actMode == TrackData::MODE2_FORM1 ||
	    actMode == TrackData::MODE2_FORM2)))) {
      return i;
    }

    if (buf != NULL) {
      switch (mode) {
      case TrackData::MODE1:
	memcpy(buf, sector + 4, MODE1_BLOCK_LEN);
	buf += MODE1_BLOCK_LEN;
	break;
      case TrackData::MODE2:
      case TrackData::MODE2_FORM_MIX:
	memcpy(buf, sector + 4, MODE2_BLOCK_LEN);
	buf += MODE2_BLOCK_LEN;
	break;
      case TrackData::MODE2_FORM1:
	memcpy(buf, sector + 12, MODE2_FORM1_DATA_LEN);
	buf += MODE2_FORM1_DATA_LEN;
	break;
      case TrackData::MODE2_FORM2:
	memcpy(buf, sector + 12, MODE2_FORM2_DATA_LEN);
	buf += MODE2_FORM2_DATA_LEN;
	break;
      case TrackData::MODE1_RAW:
      case TrackData::MODE2_RAW:
	memcpy(buf, syncPattern, 12);
	memcpy(buf + 12, sector, 2340);
	buf += AUDIO_BLOCK_LEN;
	break;
      case TrackData::MODE0:
      case TrackData::AUDIO:
	log_message(-3, "CDD2600::readTrackData: Illegal mode.");
	return 0;
	break;
      }
    }

    sector += blockLen;
  }

  return len;
}

int CDD2600::readSubChannels(TrackData::SubChannelMode, long lba, long len,
			     SubChannel ***chans, Sample *audioData)
{
  unsigned char cmd[10];
  int tries = 5;
  int ret;

  if (setBlockSize(AUDIO_BLOCK_LEN) != 0)
    return 1;

  memset(cmd, 0, 10);

  cmd[0] = 0x28; // READ10
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[7] = len >> 8;
  cmd[8] = len;

  do {
    ret = sendCmd(cmd, 10, NULL, 0, 
		  (unsigned char*)audioData, len * AUDIO_BLOCK_LEN,
		  (tries == 1) ? 1 : 0);

    if (ret != 0 && tries == 1) {
      log_message(-2, "Reading of audio data failed at sector %ld.", lba);
      return 1;
    }
    
    tries--;
  } while (ret != 0 && tries > 0);

  *chans = NULL;
  return 0;
}

int CDD2600::readAudioRange(ReadDiskInfo *rinfo, int fd, long start, long end,
			    int startTrack, int endTrack,
			    TrackInfo *info)
{
  if (!onTheFly_) {
    int t;

    log_message(1, "Analyzing...");

    for (t = startTrack; t <= endTrack; t++) {
      long totalProgress;

      totalProgress = t * 1000;
      totalProgress /= rinfo->tracks;

      sendReadCdProgressMsg(RCD_ANALYZING, rinfo->tracks, t + 1, 0,
			    totalProgress);

      log_message(2, "Track %d...", t + 1);
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

  return CdrDriver::readAudioRangeParanoia(rinfo, fd, start, end, startTrack,
					   endTrack, info);
}
