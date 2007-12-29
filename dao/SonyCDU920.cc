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


#include <config.h>

#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "SonyCDU920.h"

#include "port.h"
#include "Toc.h"
#include "log.h"
#include "PQSubChannel16.h"

SonyCDU920::SonyCDU920(ScsiIf *scsiIf, unsigned long options)
  : CdrDriver(scsiIf, options)
{
  int i;
  driverName_ = "Sony CDU920 - Version 0.1 (data) (alpha)";
  
  speed_ = 2;
  simulate_ = true;
  encodingMode_ = 1;

  scsiTimeout_ = 0;
  leadInLen_ = 0;

  for (i = 0; i < maxScannedSubChannels_; i++) {
    scannedSubChannels_[i] = new PQSubChannel16;
  }

  // reads little endian samples
  audioDataByteOrder_ = 0;
}

SonyCDU920::~SonyCDU920()
{
  int i;

  for (i = 0; i < maxScannedSubChannels_; i++) {
    delete scannedSubChannels_[i];
    scannedSubChannels_[i] = NULL;
  }
}

// static constructor
CdrDriver *SonyCDU920::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new SonyCDU920(scsiIf, options);
}

int SonyCDU920::bigEndianSamples() const
{
  return 0; // drive takes little endian samples
 }

int SonyCDU920::multiSession(int m)
{
  return 1; // not supported in DAO mode
}

// sets speed
// return: 0: OK
//         1: illegal speed
int SonyCDU920::speed(int s)
{
  if (s >= 0 && s <= 2)
    speed_ = s;
  else if (s > 2)
    speed_ = 2;
  else
    return 1;

  return 0;
}

// loads ('unload' == 0) or ejects ('unload' == 1) tray
// return: 0: OK
//         1: scsi command failed
int SonyCDU920::loadUnload(int unload) const
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x1b; // START/STOP UNIT
  if (unload) {
    cmd[4] = 0x02; // LoUnlo=1, Start=0
  }
  else {
    cmd[4] = 0x01; // LoUnlo=0, Start=1
  }
  
  if (sendCmd(cmd, 6, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot load/unload medium.");
    return 1;
  }

  return 0;
}


// sets read/write speed and simulation mode
// return: 0: OK
//         1: scsi command failed
int SonyCDU920::selectSpeed()
{
  unsigned char mp[4];
  
  mp[0] = 0x31;
  mp[1] = 2;
  mp[2] = 0;
  mp[3] = 0;

  switch (speed_) {
  case 0:
    mp[2] = 0xff;
  case 1:
    mp[2] = 0;
    break;
  case 2:
    mp[2] = 1;
    break;
  }

  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set speed mode page.");
    return 1;
  }

  return 0;
}

// Determins start and length of lead-in.
// return: 0: OK
//         1: SCSI command failed
int SonyCDU920::getSessionInfo()
{
  unsigned char mp[32];

  if (getModePage6(0x22, mp, 32, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve CD-R disc information mode page.");
    return 1;
  }

  leadInStart_ = Msf(mp[25], mp[26], mp[27]);

  if (leadInStart_.lba() != 0)
    leadInLen_ = 450000 - leadInStart_.lba();
  else
    leadInLen_ = 0;

  log_message(4, "Lead-in start: %s length: %ld", leadInStart_.str(),
	  leadInLen_);

  return 0;
}


// Sets write parameters.
// return: 0: OK
//         1: scsi command failed
int SonyCDU920::setWriteParameters()
{
  unsigned char mp[32];

  memset(mp, 0, 8);
  mp[0] = 0x20;
  mp[1] = 6;
  mp[3] = (simulate_ != 0) ? 2 : 0;
  
  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set CD-R mastering information page.");
    return 1;
  }

  memset(mp, 0, 32);
  if (getModePage6(0x22, mp, 32, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve CD-R disc information page.");
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag
  
  mp[2] = 0; // Disc Style: uninterrupted
  mp[3] = sessionFormat(); // Disc Type: from 'toc_' object
  mp[19] = 0; // no automatic post-gap; required?
  
  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set CD-R disc information page.");
    return 1;
  }

  return 0;
}

// Creates cue sheet for current 'toc_' object.
// cueSheetLen: filled with length of cue sheet in bytes
// return: newly allocated cue sheet buffer or 'NULL' on error
unsigned char *SonyCDU920::createCueSheet(unsigned char leadInDataForm,
					  long *cueSheetLen)
{
  const Track *t;
  int trackNr;
  Msf start, end, index;
  unsigned char *cueSheet;
  long len;
  long n; // index into cue sheet
  unsigned char ctl; // control nibbles of cue sheet entry CTL/ADR
  long i;
  unsigned char dataMode;
  int firstTrack;

  TrackIterator itr(toc_);

  if (itr.first(start, end) == NULL) {
    return NULL;
  }

  // determine length of cue sheet
  len = 3; // entries for lead-in, 1st pre-gap, lead-out
  for (t = itr.first(start, end), trackNr = 1;
       t != NULL;
       t = itr.next(start, end), trackNr++) {
    len += 1; // entry for track

    if (t->start().lba() != 0 && trackNr > 1) {
      len += 1; // entry for pre-gap
    }
    
    len += t->nofIndices(); // entry for each index increment
  }

  cueSheet = new unsigned char[len * 8];
  n = 0;

  // entry for lead-in
  ctl = (toc_->leadInMode() == TrackData::AUDIO) ? 0 : 0x40;

  cueSheet[n*8] = 0x01 | ctl; // CTL/ADR
  cueSheet[n*8+1] = 0;    // Track number
  cueSheet[n*8+2] = 0;    // Index
  cueSheet[n*8+3] = leadInDataForm;
  cueSheet[n*8+4] = 0;    // Serial Copy Management System
  cueSheet[n*8+5] = 0;    // MIN
  cueSheet[n*8+6] = sessionFormat(); // disc type
  cueSheet[n*8+7] = 0;    // FRAME
  n++;

  firstTrack = 1;
  for (t = itr.first(start, end), trackNr = 1;
       t != NULL;
       t = itr.next(start, end), trackNr++) {

    switch (t->type()) {
    case TrackData::AUDIO:
      dataMode = 0x01;
      break;
    case TrackData::MODE1:
    case TrackData::MODE1_RAW:
      dataMode = 0x11;
      break;
    case TrackData::MODE2:
      dataMode = 0x19;
      break;
    case TrackData::MODE2_RAW:
    case TrackData::MODE2_FORM1:
    case TrackData::MODE2_FORM2:
    case TrackData::MODE2_FORM_MIX:
      dataMode = 0x23;
      break;
    default:
      dataMode = 0;
      break;
    }

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
    }
    else {
      // data track
      ctl |= 0x40;
    }

    Msf tstart(start.lba() + 150); // start of index 1 of current track
	 
    if (firstTrack) {
      // entry for pre-gap before first track
      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = SubChannel::bcd(trackNr);
      cueSheet[n*8+2] = 0;    // Index 0
      cueSheet[n*8+3] = dataMode; // Data Form
      cueSheet[n*8+4] = 0;    // Serial Copy Management System
      cueSheet[n*8+5] = 0;    // MIN 0
      cueSheet[n*8+6] = 0;    // SEC 0
      cueSheet[n*8+7] = 0;    // FRAME 0
      n++;
    }
    else if (t->start().lba() != 0) {
      // entry for pre-gap
      Msf pstart(tstart.lba() - t->start().lba());

      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = SubChannel::bcd(trackNr);
      cueSheet[n*8+2] = 0;        // Index 0: pre-gap
      cueSheet[n*8+3] = dataMode; // Data Form
      cueSheet[n*8+4] = 0;        // no alternate copy bit
      cueSheet[n*8+5] = SubChannel::bcd(pstart.min());
      cueSheet[n*8+6] = SubChannel::bcd(pstart.sec());
      cueSheet[n*8+7] = SubChannel::bcd(pstart.frac());
      n++;
    }

    cueSheet[n*8]   = ctl | 0x01;
    cueSheet[n*8+1] = SubChannel::bcd(trackNr);
    cueSheet[n*8+2] = 1; // Index 1
    cueSheet[n*8+3] = dataMode; // Data Form
    cueSheet[n*8+4] = 0; // no alternate copy bit
    cueSheet[n*8+5] = SubChannel::bcd(tstart.min());
    cueSheet[n*8+6] = SubChannel::bcd(tstart.sec());
    cueSheet[n*8+7] = SubChannel::bcd(tstart.frac());
    n++;

    for (i = 0; i < t->nofIndices(); i++) {
      index = tstart + t->getIndex(i);
      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = SubChannel::bcd(trackNr);
      cueSheet[n*8+2] = SubChannel::bcd(i + 2); // Index
      cueSheet[n*8+3] = dataMode; // Data Form
      cueSheet[n*8+4] = 0; // no alternate copy bit
      cueSheet[n*8+5] = SubChannel::bcd(index.min());
      cueSheet[n*8+6] = SubChannel::bcd(index.sec());
      cueSheet[n*8+7] = SubChannel::bcd(index.frac());
      n++;
    }

    firstTrack = 0;
  }

  assert(n == len - 1);

  // entry for lead out
  Msf lostart(toc_->length().lba() + 150);
  ctl = (toc_->leadOutMode() == TrackData::AUDIO) ? 0 : 0x40;
    
  cueSheet[n*8]   = ctl | 0x01;
  cueSheet[n*8+1] = 0xaa;
  cueSheet[n*8+2] = 1; // Index 1
  cueSheet[n*8+3] = 0x00; // CD-DA data, data generated by device
  cueSheet[n*8+4] = 0; // no alternate copy bit
  cueSheet[n*8+5] = SubChannel::bcd(lostart.min());
  cueSheet[n*8+6] = SubChannel::bcd(lostart.sec());
  cueSheet[n*8+7] = SubChannel::bcd(lostart.frac());

  log_message(4, "\nCue Sheet:");
  log_message(4, "CTL/  TNO  INDEX  DATA  SCMS  MIN  SEC  FRAME");
  log_message(4, "ADR               FORM        BCD  BCD  BCD");
  for (n = 0; n < len; n++) {
    log_message(4, "%02x    %02x    %02x     %02x    %02x   %02x   %02x   %02x",
	   cueSheet[n*8],
	   cueSheet[n*8+1], cueSheet[n*8+2], cueSheet[n*8+3], cueSheet[n*8+4],
	   cueSheet[n*8+5], cueSheet[n*8+6], cueSheet[n*8+7]);
  }

  *cueSheetLen = len * 8;
  return cueSheet;
}

int SonyCDU920::sendCueSheet(unsigned char leadInDataForm)
{
  unsigned char cmd[10];
  long cueSheetLen;
  unsigned char *cueSheet = createCueSheet(leadInDataForm, &cueSheetLen);

  if (cueSheet == NULL) {
    return 1;
  }

  if (cueSheetLen > 3600) {
    log_message(-2, "Cue sheet too big. Please remove index marks.");
    delete[] cueSheet;
    return 1;
  }

  memset(cmd, 0, 10);

  cmd[0] = 0xe0; // WRITE START
  cmd[1] = (cueSheetLen >> 16) & 0x0f;
  cmd[2] = cueSheetLen >> 8;
  cmd[3] = cueSheetLen;

  if (sendCmd(cmd, 10, cueSheet, cueSheetLen, NULL, 0) != 0) {
    log_message(-2, "Cannot send cue sheet.");
    delete[] cueSheet;
    return 1;
  }

  delete[] cueSheet;
  return 0;
}

int SonyCDU920::readBufferCapacity(long *capacity)
{
  unsigned char cmd[10];
  unsigned char data[8];

  memset(cmd, 0, 10);
  memset(data, 0, 8);

  cmd[0] = 0xec; // READ BUFFER CAPACITY

  if (sendCmd(cmd, 10, NULL, 0, data, 8) != 0) {
    log_message(-2, "Read buffer capacity failed.");
    return 1;
  }

  *capacity = (data[5] << 16) | (data[6] << 8) | data[7];

  return 0;
}

int SonyCDU920::initDao(const Toc *toc)
{
  long n;

  toc_ = toc;

  blockLength_ = AUDIO_BLOCK_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

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

int SonyCDU920::startDao()
{
  scsiTimeout_ = scsiIf_->timeout(3 * 60);

  if (setWriteParameters() != 0 ||
      sendCueSheet(0x00/* Data Form: CD-DA, generate data by device */) != 0)
    return 1;

  long lba = -150;

  // write mandatory pre-gap after lead-in
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, 0, 150)
      != 0) {
    return 1;
  }
  
  return 0;
}

int SonyCDU920::finishDao()
{
  scsiIf_->timeout(scsiTimeout_);

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void SonyCDU920::abortDao()
{
  unsigned char cmd[10];

  memset(cmd, 0, 10);

  cmd[0] = 0xe2; // DISCONTINUE
  
  sendCmd(cmd, 10, NULL, 0, NULL, 0, 1);
}


// Writes data to target, the block length depends on the actual track 'mode'.
// 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function.
// return: 0: OK
//         1: scsi command failed
int SonyCDU920::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
			  long &lba, const char *buf, long len)
{
  assert(blocksPerWrite_ > 0);

  unsigned char cmd[10];
  int writeLen = 0;
  long blockLength = blockSize(mode, TrackData::SUBCHAN_NONE);
  long byteLen;
  int ret;

#if 0
  long sum, i;

  sum = 0;

  for (i = 0; i < len * blockLength; i++) {
    sum += buf[i];
  }

  log_message(0, "W: %ld: %ld, %ld, %ld", lba, blockLength, len, sum);
#endif

  memset(cmd, 0, 10);
  cmd[0] = 0xe1; // WRITE CONTINUE
  
  while (len > 0) {
    writeLen = (len > blocksPerWrite_ ? blocksPerWrite_ : len);

    byteLen = writeLen * blockLength;

    cmd[1] = (byteLen >> 16) & 0x1f;
    cmd[2] = byteLen >> 8;
    cmd[3] = byteLen;

    if ((ret = sendCmd(cmd, 10, (unsigned char *)buf, byteLen, NULL, 0, 0))
	!= 0) {
      if(ret == 2) {
        const unsigned char *sense;
        int senseLen;

        sense = scsiIf_->getSense(senseLen);

	// The drive logs in the CD-R after write simulation again and
	// reports this by Unit Attention. Ceck for this error and ignore it.
	if(senseLen >= 14 && (sense[2] & 0x0f) == 0x6 && sense[7] >= 6 &&
	   (sense[12] == 0x80 /*this really happened*/ ||
	    sense[12] == 0xd4 /*EXIT FROM PSEUDO TRACK AT ONCE RECORDING*/)) {
	  sleep(10); // wait until drive becomes ready again
        }
	else {
	  scsiIf_->printError();
	  log_message(-2, "Write data failed.");
	  return 1;
	}
      }
      else {
	log_message(-2, "Write data failed.");
	return 1;
      }
    }

    buf += byteLen;

    lba += writeLen;
    len -= writeLen;
  }
      
  return 0;
}

DiskInfo *SonyCDU920::diskInfo()
{
  unsigned char mp[32];
  
  memset(&diskInfo_, 0, sizeof(DiskInfo));

  memset(mp, 0, 32);
  if (getModePage6(0x22, mp, 32, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve CD-R disc information mode page.");
    return &diskInfo_;
  }

  diskInfo_.empty = ((mp[16] >> 6) == 0 ? 1 : 0);
  diskInfo_.valid.empty = 1;

  diskInfo_.cdrw = 0;
  diskInfo_.valid.cdrw = 1;

  if (diskInfo_.empty) {
    diskInfo_.manufacturerId = Msf(mp[25], mp[26], mp[27]);
    diskInfo_.valid.manufacturerId = 1;

    diskInfo_.capacity = Msf(mp[13], mp[14], mp[15]).lba();
    diskInfo_.valid.capacity = 1;
  }
  
  return &diskInfo_;
}

Toc *SonyCDU920::readDiskToc(int session, const char *audioFilename)
{
  Toc *toc = CdrDriver::readDiskToc(session, audioFilename);

  setBlockSize(MODE1_BLOCK_LEN);
  
  return toc;
}

// tries to read catalog number from disk and adds it to 'toc'
// return: 1 if valid catalog number was found, else 0
int SonyCDU920::readCatalog(Toc *toc, long startLba, long endLba)
{
  unsigned char cmd[10];
  unsigned char data[24];
  char catalog[14];
  int i;

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

  return 0;
}

int SonyCDU920::readIsrc(int trackNr, char *buf)
{
  unsigned char cmd[10];
  unsigned char data[24];
  int i;

  buf[0] = 0;

  memset(cmd, 0, 10);
  memset(data, 0, 24);

  cmd[0] = 0x42; // READ SUB-CHANNEL
  cmd[2] = 1 << 6;
  cmd[3] = 0x03; // get media catalog number
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

int SonyCDU920::analyzeTrack(TrackData::Mode mode, int trackNr, long startLba,
			     long endLba, Msf *indexIncrements,
			     int *indexIncrementCnt, long *pregap,
			     char *isrcCode, unsigned char *ctl)
{
  selectSpeed();

  int ret = analyzeTrackScan(mode, trackNr, startLba, endLba,
			     indexIncrements, indexIncrementCnt, pregap,
			     isrcCode, ctl);

  return ret;
}

int SonyCDU920::readSubChannels(TrackData::SubChannelMode,
				long lba, long len, SubChannel ***chans,
				Sample *audioData)
{
  unsigned char cmd[12];
  int i;
  int retries = 5;
  long blockLen = AUDIO_BLOCK_LEN + 16;

  cmd[0] = 0xd8;  // READ CDDA
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = len >> 24;
  cmd[7] = len >> 16;
  cmd[8] = len >> 8;
  cmd[9] = len;
  cmd[10] = 0x01;
  cmd[11] = 0;

  while (1) {
    if (sendCmd(cmd, 12, NULL, 0, transferBuffer_, len * blockLen,
		retries == 0 ? 1 : 0) != 0) {
      if (retries == 0)
	return 1;
    }
    else {
      break;
    }

    retries--;
  }
  
  unsigned char *p =  transferBuffer_ + AUDIO_BLOCK_LEN;

  for (i = 0; i < len; i++) {
    ((PQSubChannel16*)scannedSubChannels_[i])->init(p);

    if (scannedSubChannels_[i]->type() != SubChannel::QMODE_ILLEGAL) {
      // the CRC of the sub-channel data is set to zero -> mark the
      // sub-channel object that it should not try to verify the CRC
      scannedSubChannels_[i]->crcInvalid();
    }

    p += blockLen;
  }

  if (audioData != NULL) {
    p = transferBuffer_;

    for (i = 0; i < len; i++) {
      memcpy(audioData, p, AUDIO_BLOCK_LEN);

      p += blockLen;
      audioData += SAMPLES_PER_BLOCK;
    }
  }

  *chans = scannedSubChannels_;
  return 0;
}

CdRawToc *SonyCDU920::getRawToc(int sessionNr, int *len)
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
  cmd[6] = sessionNr;
  cmd[8] = 4;
  cmd[9] = (2 << 6);

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
    log_message(0, "%d %02x %02d %2x %02d:%02d:%02d %02d %02d:%02d:%02d",
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

long SonyCDU920::readTrackData(TrackData::Mode mode,
			       TrackData::SubChannelMode,
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
	  case 0x63: // end of user area encountered on this track
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
      log_message(4, "Stopped because sector with not matching mode %s found.",
	      TrackData::mode2String(actMode));
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
	log_message(-3, "PlextorReader::readTrackData: Illegal mode.");
	return 0;
	break;
      }
    }

    sector += blockLen;
  }  

  return len;
}

int SonyCDU920::readAudioRange(ReadDiskInfo *rinfo, int fd, long start,
			       long end, int startTrack,
			       int endTrack, TrackInfo *info)
{
  return CdrDriver::readAudioRangeParanoia(rinfo, fd, start, end, startTrack,
					   endTrack, info);
}
