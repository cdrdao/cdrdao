/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999-2001  Cameron G. MacKinnon <C_MacKinnon@yahoo.com>
 *                           Andreas Mueller <andreas@daneb.de>
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

/* Driver for Yamaha CDR10X drives. 
 * Written by Cameron G. MacKinnon <C_MacKinnon@yahoo.com>.
 */


#include <config.h>

#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "YamahaCDR10x.h"

#include "Toc.h"
#include "PQSubChannel16.h"
#include "log.h"
#include "port.h"


YamahaCDR10x::YamahaCDR10x(ScsiIf *scsiIf, unsigned long options)
  : CdrDriver(scsiIf, options)
{
  int i;

  driverName_ = "Yamaha CDR10x - Version 0.5 (data)";

  encodingMode_ = 1; 
  
  speed_ = 0;
  simulate_ = true;

  scsiTimeout_ = 0;

  for (i = 0; i < maxScannedSubChannels_; i++)
    scannedSubChannels_[i] = new PQSubChannel16;

  // reads little endian samples
  audioDataByteOrder_ = 0;
}

// static constructor
CdrDriver *YamahaCDR10x::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new YamahaCDR10x(scsiIf, options);
}

YamahaCDR10x::~YamahaCDR10x()
{
  int i;

  for (i = 0; i < maxScannedSubChannels_; i++) {
    delete scannedSubChannels_[i];
    scannedSubChannels_[i] = NULL;
  }
}

int YamahaCDR10x::multiSession(int m)
{
  if (m != 0) {
    return 1; // multi session mode is not supported
  }

  return 0;
}

// sets speed
// return: 0: OK
//         1: illegal speed
int YamahaCDR10x::speed(int s)
{
  if (s == 0 || s == 1 || s == 2 || s == 4)
    speed_ = s;
  else if (s == 3)
    speed_ = 2;
  else if (s > 4)
    speed_ = 4;
  else
    return 1;

  if (selectSpeed() != 0)
    return 1;
  
  return 0;
}

int YamahaCDR10x::speed()
{
  DriveInfo di;

  if (driveInfo(&di, 1) != 0) {
    return 0;
  }

  return speed2Mult(di.currentWriteSpeed);

}

// loads ('unload' == 0) or ejects ('unload' == 1) tray
// return: 0: OK
//         1: scsi command failed
int YamahaCDR10x::loadUnload(int unload) const
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x1b; // START/STOP UNIT
  if (unload) {
    cmd[4] = 0x02; // LoUnlo=1, Start=0
  }
  else {
    cmd[4] = 0x01; // LoUnlo=0, Start=1: This is a caddy, and load is reserved
  }
  
  if (sendCmd(cmd, 6, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot %s medium.", (unload ? "stop & unload" : "start"));
    return 1;
  }

  return 0;
}

// sets read/write speed
// return: 0: OK
//         1: scsi command failed
int YamahaCDR10x::selectSpeed()
{
  unsigned char mp[4];

  if (getModePage6(0x31/*drive configuration mode page*/, mp, 4,
		   NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve drive configuration mode page (0x31).");
    return 1;
  }

  mp[3] &= 0x0f;
  if(speed_ == 0 || speed_ == 4) {
	mp[3] |= 0x20;  // read at 4x, write at 2x (CDR102) or 4x (CDR100)
  } else if(speed_ == 2) {
	mp[3] |= 0x10;  // read at 2x, write at 2x
  } else if(speed_ == 1) {
	;  		// read at 1x, write at 1x
  } else {
	log_message(-2, "Invalid write speed argument %d. 0, 1, 2 or 4 expected.",
		speed_);
	return 1;
  }

  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set drive configuration mode page for cd speed.");
    return 1;
  }

  return 0;
}

// Sets write parameters via mode pages 0x31 and 0x32.
// return: 0: OK
//         1: scsi command failed
int YamahaCDR10x::setWriteParameters()
{
  unsigned char mp[8];	// the larger of 4 and 8, gets used twice

  if (getModePage6(0x31/*drive configuration mode page*/, mp, 4,
		   NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve drive configuration mode page (0x31).");
    return 1;
  }

  mp[3] &= 0xf0;	// save speed settings in high nibble
  mp[3] |= (simulate_ ? 1 : 0);

  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set drive configuration mode page (0x31).");
    return 1;
  }

  memset(mp, 0, 8);

  if (getModePage6(0x32/*write format mode page*/, mp, 8,
		   NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve write format mode page (0x32).");
    return 1;
  }

  mp[2] &= 0xf0;	// RW subcode internally zero supplied
  mp[3] &= 0xf0;
  mp[3] |= 0x04;	// disc at once (single session)

  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set write format mode page (0x32).");
    return 1;
  }

  return 0;
}

void YamahaCDR10x::cueSheetDataType(TrackData::Mode mode,
				    unsigned char *dataType,
				    unsigned char *dataForm)
{
  switch (mode) {
  case TrackData::AUDIO:
    *dataType = 0;
    *dataForm = 0;
    break;
    
  case TrackData::MODE1:
  case TrackData::MODE1_RAW:
    *dataType = 2;
    *dataForm = 3;
    break;

  case TrackData::MODE2:
    *dataType = 3;
    *dataForm = 3;
    break;

  case TrackData::MODE2_RAW:
  case TrackData::MODE2_FORM1:
  case TrackData::MODE2_FORM2:
  case TrackData::MODE2_FORM_MIX:
    if (toc_->tocType() == Toc::CD_I)
      *dataType = 4;
    else
      *dataType = 5;
    *dataForm = 3;
    break;

  case TrackData::MODE0:
    log_message(-3, "Illegal mode in 'YamahaCDR10x::cueSheetDataType()'.");
    *dataType = 0;
    *dataForm = 0;
    break;
  }
}

// Creates cue sheet for current toc.
// cueSheetLen: filled with length of cue sheet in bytes
// return: newly allocated cue sheet buffer or 'NULL' on error
// CHECKS OUT WITH DOS-DAO AND MANUAL
unsigned char *YamahaCDR10x::createCueSheet(long *cueSheetLen)
{
  const Track *t;
  int trackNr;
  Msf start, end, index;
  unsigned char *cueSheet;
  long len = 2; // entries for lead-in, lead-out
  long n; // index into cue sheet
  unsigned char ctl; // control nibbles of cue sheet entry CTL/ADR
  long i;
  unsigned char dataForm;
  unsigned char dataType;

  TrackIterator itr(toc_);

  if (itr.first(start, end) == NULL) {
    return NULL;
  }

  if (toc_->catalogValid()) {
    len += 2;
  }

  for (t = itr.first(start, end), trackNr = 1;
       t != NULL; t = itr.next(start, end), trackNr++) {
    len += 2; // entry for track and mandatory pre-gap
    if (t->type() == TrackData::AUDIO && t->isrcValid()) {
      len += 2; // entries for ISRC code
    }
    len += t->nofIndices(); // entry for each index increment
  }

  cueSheet = new unsigned char[len * 8];
  n = 0;

  if (toc_->leadInMode() == TrackData::AUDIO)
    ctl = 0;
  else
    ctl = 0x40;

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

  cueSheetDataType(toc_->leadInMode(), &dataType, &dataForm);

  // entry for lead-in
  cueSheet[n*8] = 0x01 | ctl; // CTL/ADR
  cueSheet[n*8+1] = 0;        // Track number
  cueSheet[n*8+2] = 0;        // Index
  cueSheet[n*8+3] = dataType; // Data Type
  cueSheet[n*8+4] = 0;        // Data Form: always 0 for lead-in
  cueSheet[n*8+5] = 0;        // MIN
  cueSheet[n*8+6] = 0;        // SEC
  cueSheet[n*8+7] = 0;        // FRAME
  n++;
  
  for (t = itr.first(start, end), trackNr = 1;
       t != NULL;
       t = itr.next(start, end), trackNr++) {

    cueSheetDataType(t->type(), &dataType, &dataForm);

    ctl = trackCtl(t);

    if (t->type() == TrackData::AUDIO) {
      // set ISRC code for audio track
      if (t->isrcValid()) {
	cueSheet[n*8] = ctl | 0x03;
	cueSheet[n*8+1] = SubChannel::bcd(trackNr);
	cueSheet[n*8+2] = t->isrcCountry(0);
	cueSheet[n*8+3] = t->isrcCountry(1);
	cueSheet[n*8+4] = t->isrcOwner(0);  
	cueSheet[n*8+5] = t->isrcOwner(1);   
	cueSheet[n*8+6] = t->isrcOwner(2);  
	cueSheet[n*8+7] = t->isrcYear(0) + '0';
	n++;
	
	cueSheet[n*8] = ctl | 0x03;
	cueSheet[n*8+1] = SubChannel::bcd(trackNr);
	cueSheet[n*8+2] = t->isrcYear(1) + '0';
	cueSheet[n*8+3] = t->isrcSerial(0) + '0';
	cueSheet[n*8+4] = t->isrcSerial(1) + '0';
	cueSheet[n*8+5] = t->isrcSerial(2) + '0';
	cueSheet[n*8+6] = t->isrcSerial(3) + '0';
	cueSheet[n*8+7] = t->isrcSerial(4) + '0';
	n++;
      }
    }

    Msf tstart(start.lba() + 150); // start of index 1

    // start of pre-gap, atime equals index 1 if no pre-gap is defined
    Msf pstart(trackNr == 1 ? 0 : tstart.lba() - t->start().lba());

    // entry for pre-gap
    cueSheet[n*8]   = ctl | 0x01;
    cueSheet[n*8+1] = SubChannel::bcd(trackNr);
    cueSheet[n*8+2] = 0;        // Index 0 indicates pre-gap
    cueSheet[n*8+3] = dataType; // Data Type
    cueSheet[n*8+4] = dataForm; // Data Form
    cueSheet[n*8+5] = SubChannel::bcd(pstart.min());
    cueSheet[n*8+6] = SubChannel::bcd(pstart.sec());
    cueSheet[n*8+7] = SubChannel::bcd(pstart.frac());
    n++;

    cueSheet[n*8]   = ctl | 0x01;
    cueSheet[n*8+1] = SubChannel::bcd(trackNr);
    cueSheet[n*8+2] = 1;        // Index 1
    cueSheet[n*8+3] = dataType; // Data Type
    cueSheet[n*8+4] = dataForm; // Data Form
    cueSheet[n*8+5] = SubChannel::bcd(tstart.min());
    cueSheet[n*8+6] = SubChannel::bcd(tstart.sec());
    cueSheet[n*8+7] = SubChannel::bcd(tstart.frac());
    n++;

    for (i = 0; i < t->nofIndices(); i++) {
      index = tstart + t->getIndex(i);
      cueSheet[n*8]   = ctl | 0x01;
      cueSheet[n*8+1] = SubChannel::bcd(trackNr);
      cueSheet[n*8+2] = SubChannel::bcd(i+2); // Index
      cueSheet[n*8+3] = dataType;    // DataType
      cueSheet[n*8+4] = dataForm;    // Data Form
      cueSheet[n*8+5] = SubChannel::bcd(index.min());
      cueSheet[n*8+6] = SubChannel::bcd(index.sec());
      cueSheet[n*8+7] = SubChannel::bcd(index.frac());
      n++;
    }
  }

  assert(n == len - 1);

  // entry for lead out
  Msf lostart(toc_->length().lba() + 150);
  cueSheetDataType(toc_->leadOutMode(), &dataType, &dataForm);
  ctl = toc_->leadOutMode() == TrackData::AUDIO ? 0 : 0x40;
    
  cueSheet[n*8]   = ctl | 0x01;
  cueSheet[n*8+1] = 0xaa;	// This IS BCD
  cueSheet[n*8+2] = 1; // Index 1
  cueSheet[n*8+3] = dataType; // Data for lead-out
  cueSheet[n*8+4] = 0;        // Data Form, always 0 for lead-out
  cueSheet[n*8+5] = SubChannel::bcd(lostart.min());
  cueSheet[n*8+6] = SubChannel::bcd(lostart.sec());
  cueSheet[n*8+7] = SubChannel::bcd(lostart.frac());

  log_message(4, "\nCue Sheet:");
  log_message(4, "CTL/  TNO  INDEX  DATA  DATA  MIN  SEC  FRAME");
  log_message(4, "ADR   BCD  BCD    TYPE  Form  BCD  BCD  BCD <- Note");
  for (n = 0; n < len; n++) {
    log_message(4, "%02x    %02x    %02x     %02x    %02x   %02x   %02x   %02x",
	   cueSheet[n*8],
	   cueSheet[n*8+1], cueSheet[n*8+2], cueSheet[n*8+3], cueSheet[n*8+4],
	   cueSheet[n*8+5], cueSheet[n*8+6], cueSheet[n*8+7]);
  }

  *cueSheetLen = len * 8;
  return cueSheet;
}

// LOOKS GOOD
int YamahaCDR10x::sendCueSheet()
{
  unsigned char cmd[10];
  long cueSheetLen;
  unsigned char *cueSheet = createCueSheet(&cueSheetLen);

  if (cueSheet == NULL) {
    return 1;
  }

  if(cueSheetLen > 32768) {  // Limit imposed by drive.
    log_message(-2, "Cue sheet too long for this device: Limit is 32k bytes.");
    delete[] cueSheet;
    return 1;
  }

  memset(cmd, 0, 10);

  cmd[0] = 0xea; // Yamaha Vendor Specific WRITE TOC

  cmd[7] = cueSheetLen >> 8;
  cmd[8] = cueSheetLen;

  if (sendCmd(cmd, 10, cueSheet, cueSheetLen, NULL, 0) != 0) {
    log_message(-2, "Cannot send cue sheet.");
    delete[] cueSheet;
    return 1;
  }

  delete[] cueSheet;
  return 0;
}

// DOES NOT TALK TO DRIVE: JUST HOUSEKEEPING
int YamahaCDR10x::initDao(const Toc *toc)
{
  long n;

  toc_ = toc;

  blockLength_ = AUDIO_BLOCK_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

  if (selectSpeed() != 0) {
    return 1;
  }

  // allocate buffer for writing zeros
  n = blocksPerWrite_ * blockLength_;
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);

  return 0;
}

// LOOKS OK - dodgy WRITE TRACK parameters, but apparently how DAO16 does it
int YamahaCDR10x::startDao()
{
  unsigned char cmd[10];

  scsiTimeout_ = scsiIf_->timeout(3 * 60);

  if (setWriteParameters() != 0 ||
      sendCueSheet() != 0) {
    return 1;
  }

  memset(cmd, 0, 10);
  cmd[0] = 0xe6; 	// Yamaha WRITE TRACK(10)
  cmd[5] = 0;		// Track Number
  cmd[6] &= 0xdf;	// R Parity = 0;
  cmd[6] &= 0xef;	// Byte Swap = 0 = little endian
  cmd[6] &= 0xf7;	// Copy = 0
  cmd[6] &= 0xfb;	// Audio = 0 (false)
  cmd[6] &= 0xfc;	// Mode=0 NB This combination of mode/audio is reserved
  // so we end up sending a bare e6 command with nine bytes of nulls

  if (sendCmd(cmd, 10, NULL, 0, NULL, 0) != 0) {
	log_message(-2, "Could not send command 0xE6: WRITE TRACK(10)");
	return 1;
  }

  //log_message(2, "Writing lead-in and gap...");

  long lba = -150;

  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, 0, 150)
      != 0) {
    return 1;
  }
  
  return 0;
}

int YamahaCDR10x::finishDao()
{
  unsigned char cmd[6];
  const unsigned char *sense;
  int senseLen;

  log_message(2, "Flushing cache...");
  
  if (flushCache() != 0) {
    return 1;
  }

  memset(cmd, 0, 6);

  // wait until lead-out is written
  while(sendCmd(cmd, 6, NULL, 0, NULL, 0, 0) == 2
	&& (sense = scsiIf_->getSense(senseLen)) && senseLen 
	&& sense[2] == 0x0b  && sense[0xc] == 0x50) {
    sleep(1);
  }

  scsiIf_->timeout(scsiTimeout_);

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void YamahaCDR10x::abortDao()
{
  flushCache();
}

///////////////////////// FIXME THE TOP BAR //////////////////////////////

// NEEDS WORK
DiskInfo *YamahaCDR10x::diskInfo()
{
  unsigned char cmd[10];
  unsigned long dataLen = 8;
  unsigned char data[8];
  static DiskInfo di;
  //char spd;

  memset(&di, 0, sizeof(DiskInfo));

  di.valid.cdrw = 0; // by definition, for this device

  memset(cmd, 0, 10);
  memset(data, 0, dataLen);

  // This looks like CdrDevice::readCapacity, but with a difference: we
  // get data for a ROM, and we need to check that our disc is writable

  cmd[0] = 0x25; // READ CD-ROM CAPACITY
 
  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-1, "Cannot read CD-ROM capacity.");
    return &di;
  }

  if(data[4] | data[5] | data[6] | data[7]) { // block length field, 0=writable
	di.capacity = 0;
  } else {	// disk is writable
	di.capacity = data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
  }
  di.valid.capacity = 1;

  // FIXME: need to come up with empty, manufacturerID, recSpeed

  // maybe use READ DISC INFO (0x43) here? se p. 62
/////////////////////////////////////////
  
  return &di;
}


// tries to read catalog number from disk and adds it to 'toc'
// return: 1 if valid catalog number was found, else 0
int YamahaCDR10x::readCatalog(Toc *toc, long startLba, long endLba)
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

int YamahaCDR10x::readIsrc(int trackNr, char *buf)
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

  // FIXME this looks right, but the offsets could be wrong
  if (data[8] & 0x80) {
    for (i = 0; i < 12; i++) {
      buf[i] = data[0x09 + i];
    }
    buf[12] = 0;
  }

  return 0;
}

int YamahaCDR10x::analyzeTrack(TrackData::Mode mode, int trackNr,
			       long startLba,
			       long endLba, Msf *indexIncrements,
			       int *indexIncrementCnt, long *pregap,
			       char *isrcCode, unsigned char *ctl)
{
  int ret = analyzeTrackScan(mode, trackNr, startLba, endLba,
			     indexIncrements, indexIncrementCnt, pregap,
			     isrcCode, ctl);

  if (mode == TrackData::AUDIO)
    readIsrc(trackNr, isrcCode);
  else
    *isrcCode = 0;

  return ret;
}

int YamahaCDR10x::readSubChannels(TrackData::SubChannelMode, 
				  long lba, long len, SubChannel ***chans,
				  Sample *audioData)
{
  int retries = 5;
  int i;
  unsigned char cmd[12];
  
  cmd[0] = 0xd8;  // Yamaha READ CD-DA
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = len >> 24;
  cmd[7] = len >> 16;
  cmd[8] = len >> 8;
  cmd[9] = len;
  cmd[10] = 0x01;  // 2352 + 10 Q (no CRC) + 6 NULL
  cmd[11] = 0;

  while (1) {
    if (sendCmd(cmd, 12, NULL, 0,
		transferBuffer_, len * (AUDIO_BLOCK_LEN + 16),
		retries == 0 ? 1 : 0) != 0) {
      if (retries == 0)
	return 1;
    }
    else {
      break;
    }

    retries--;
  }

  for (i = 0; i < len; i++) {
    PQSubChannel16 *chan = (PQSubChannel16*)scannedSubChannels_[i];
    unsigned char *p =
      transferBuffer_  + i * (AUDIO_BLOCK_LEN + 16) + AUDIO_BLOCK_LEN;

    // slightly reorder the sub-channel so it is suitable for 'PQSubChannel16'
    p[0] <<= 4;   // ctl/adr
    p[0] |= p[1];
    p[1] = p[2];  // track nr
    p[2] = p[3];  // index nr
    p[3] = p[4];  // min
    p[4] = p[5];  // sec
    p[5] = p[6];  // frame
    p[6] = 0;     // zero
    // p[7] is amin
    // p[8] is asec
    // p[9] is aframe

    chan->init(p);
    
    if (chan->checkConsistency())
      chan->calcCrc();
  }

  if (audioData != NULL) {
    unsigned char *p = transferBuffer_;

    for (i = 0; i < len; i++) {
      memcpy(audioData, p, AUDIO_BLOCK_LEN);

      p += AUDIO_BLOCK_LEN + 16;
      audioData += SAMPLES_PER_BLOCK;
    }
  }
  
  *chans = scannedSubChannels_;
  return 0;
}

// LOOKSGOOD, except for accurate audio stream bit
int YamahaCDR10x::driveInfo(DriveInfo *info, bool showErrorMsg)
{
  unsigned char mp[4];

  if (getModePage6(0x31, mp, 4, NULL, NULL, showErrorMsg) != 0) {
    if (showErrorMsg) {
      log_message(-2, "Cannot retrieve drive configuration mode page (0x31).");
    }
    return 1;
  }

  // FIXME
  // info->accurateAudioStream = mp[5] & 0x02 ? 1 : 0;
  info->accurateAudioStream = 0;

  info->maxReadSpeed = mult2Speed(4);

  if(!strncmp(scsiIf_->product(), "CDR100", 6)) {
	info->maxWriteSpeed = mult2Speed(4);
	info->currentWriteSpeed = info->currentReadSpeed = 
		mult2Speed(1 << (mp[3] >> 4));
  } else {	// CDR102, crippled to 2x write
	info->maxWriteSpeed = mult2Speed(2);
	info->currentReadSpeed = mult2Speed(1 << (mp[3] >> 4));
	info->currentWriteSpeed = mult2Speed((mp[3] >> 4) ? 2 : 1);
  }

  return 0;
}

// writeData is overridden here because the CDR10x, while writing the disc
// lead-in with the buffer full, will return CHECK CONDITION with sense
// DRIVE BUSY and additional sense 0xb8 = WRITE LEAD-IN IN PROGRESS.
// This is normal operation and the write should be retried until successful.

// Writes data to target, the block length depends on the actual writing mode
// 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function.
// return: 0: OK
//         1: scsi command failed
int YamahaCDR10x::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
			    long &lba, const char *buf, long len)
{
  assert(blocksPerWrite_ > 0);
  int writeLen = 0;
  unsigned char cmd[10];
  int retry;
  int retval;
  long blockLength = blockSize(mode, TrackData::SUBCHAN_NONE);

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

    do {
      retry = 0;
      retval = sendCmd(cmd, 10, (unsigned char *)buf, writeLen * blockLength,
		       NULL, 0, 0);
      if(retval == 2) {
        const unsigned char *sense;
        int senseLen;

        sense = scsiIf_->getSense(senseLen);

	// we spin on the write here, waiting a reasonable time in between
	// we should really use READ BLOCK LIMIT (0x05)
	if(senseLen && sense[2] == 9 && sense[0xc] == 0xb8) {
	  mSleep(40);
          retry = 1;
        }
	else {
	  scsiIf_->printError();
	}
      }
    } while(retry == 1);

    if(retval) {
      log_message(-2, "Write data failed.");
      return 1;
    }

    buf += writeLen * blockLength;

    lba += writeLen;
    len -= writeLen;
  }
      
  return 0;
}

CdRawToc *YamahaCDR10x::getRawToc(int sessionNr, int *len)
{
  // not supported by drive
  return NULL;
}


long YamahaCDR10x::readTrackData(TrackData::Mode mode,
				 TrackData::SubChannelMode,
				 long lba, long len,
				 unsigned char *buf)
{
  unsigned char cmd[10];
  long blockLen = 2340;
  long i;
  TrackData::Mode actMode;
  int ok = 0;
  int softError;
  const unsigned char *sense;
  int senseLen;

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
	log_message(-3, "YamahaCDR10x::readTrackData: Illegal mode.");
	return 0;
	break;
      }
    }

    sector += blockLen;
  }  

  return len;
}

Toc *YamahaCDR10x::readDiskToc(int session, const char *audioFilename)
{
  Toc *toc = CdrDriver::readDiskToc(session, audioFilename);

  setBlockSize(MODE1_BLOCK_LEN);
  
  return toc;
}

Toc *YamahaCDR10x::readDisk(int session, const char *fname)
{
  Toc *toc = CdrDriver::readDisk(session, fname);

  setBlockSize(MODE1_BLOCK_LEN);

  return toc;
}

int YamahaCDR10x::readAudioRange(ReadDiskInfo *rinfo, int fd, long start,
				 long end, int startTrack,
				 int endTrack, TrackInfo *info)
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

      log_message(1, "Track %d...", t + 1);
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
