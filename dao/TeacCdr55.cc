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

#include "TeacCdr55.h"

#include "Toc.h"
#include "PQSubChannel16.h"
#include "log.h"

TeacCdr55::TeacCdr55(ScsiIf *scsiIf, unsigned long options)
  : CdrDriver(scsiIf, options)
{
  int i;
  driverName_ = "Teac CD-R50/55 - Version 0.1 (data)";
  
  speed_ = 0;
  simulate_ = true;
  encodingMode_ = 1;

  scsiTimeout_ = 0;

  actMode_ = TrackData::AUDIO;
  writeEndLba_ = 0;

  memset(modeSelectData_, 0, 12);
  memset(&diskInfo_, 0, sizeof(DiskInfo));

  for (i = 0; i < maxScannedSubChannels_; i++)
    scannedSubChannels_[i] = new PQSubChannel16;

  // reads little endian samples
  audioDataByteOrder_ = 0;
}

TeacCdr55::~TeacCdr55()
{
  int i;

  for (i = 0; i < maxScannedSubChannels_; i++) {
    delete scannedSubChannels_[i];
    scannedSubChannels_[i] = NULL;
  }
}

// static constructor
CdrDriver *TeacCdr55::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new TeacCdr55(scsiIf, options);
}

// sets speed
// return: 0: OK
//         1: illegal speed
int TeacCdr55::speed(int s)
{
  if (s >= 0 && s <= 4 && s != 3) {
    speed_ = s;
  }
  else if (s == 3) {
    speed_ = 2;
  }
  else if (s > 4) {
    speed_ = 4;
  }
  else {
    return 1;
  }

  return 0;
}

// loads ('unload' == 0) or ejects ('unload' == 1) tray
// return: 0: OK
//         1: scsi command failed
int TeacCdr55::loadUnload(int unload) const
{
  
 unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x1b; // START/STOP UNIT
  cmd[4] = unload ? 0x02 : 0x03;

  if (sendCmd(cmd, 6, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot load/unload medium.");
    return 1;
  }

  return 0;
}

// Retrieves mode select header and block descriptor and stores it in
// class member 'modeSelectData_' for later usage with various mode select
// commands.
// Return: 0: OK, 1: SCSI error
int TeacCdr55::getModeSelectData()
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);
  memset(modeSelectData_, 0, 12);

  cmd[0] = 0x1a; // MODE SENSE
  cmd[4] = 12;

  if (sendCmd(cmd, 6, NULL, 0, modeSelectData_, 12, 1) != 0) {
    log_message(-2, "Mode Sense failed.");
    return 1;
  }

  return 0;
}

// sets read/write speed and simulation mode
// return: 0: OK
//         1: scsi command failed
int TeacCdr55::setWriteSpeed()
{
  unsigned char mp[4];
  
  mp[0] = 0x31;
  mp[1] = 2;
  mp[2] = 0;
  mp[3] = 0;

  switch (speed_) {
  case 1:
    mp[2] = 0;
    break;
  case 2:
    mp[2] = 1;
    break;
  case 0:
  case 4:
    mp[2] = 2;
    break;
  }

  if (setModePage6(mp, modeSelectData_, NULL, 1) != 0) {
    log_message(-2, "Cannot set speed mode page.");
    return 1;
  }
    
  return 0;
}

// Sets write parameters via mode page 0x22.
// return: 0: OK
//         1: scsi command failed
int TeacCdr55::setWriteParameters()
{
  unsigned char mp[9];

  memset(mp, 0, 9);

  mp[0] = 0x22;
  mp[1] = 7;

  if (multiSession()) {
    mp[4] = 4; // session at once, keep session open
  }
  else {
    if (diskInfo_.sessionCnt > 0)
      mp[4] = 0x84; // session at once and close disk
    else
      mp[4] = 2; // disk at once
  }

  if (setModePage6(mp, modeSelectData_, NULL, 1) != 0) {
    log_message(-2, "Cannot set write method mode page.");
    return 1;
  }

  return 0;
}

int TeacCdr55::setSimulationMode()
{
  unsigned char mp[3];

  mp[0] = 0x21;
  mp[1] = 1;

  if (simulate())
    mp[2] = 3; 
  else
    mp[2] = 0; 

  if (setModePage6(mp, modeSelectData_, NULL, 1) != 0) {
    log_message(-2, "Cannot set preview write mode page.");
    return 1;
  }

  return 0;
}

int TeacCdr55::setWriteDensity(TrackData::Mode mode)
{
  long blockLength = blockSize(mode, TrackData::SUBCHAN_NONE);
  unsigned char cmd[6];
  unsigned char data[12];

  memset(cmd, 0, 6);

  cmd[0] = 0x15; // MODE SELECT
  cmd[1] = 0x10;
  cmd[4] = 12;

  memcpy(data, modeSelectData_, 12);

  data[0] = 0;
  data[3] = 8;
  data[9] = 0;

  switch (mode) {
  case TrackData::AUDIO:
    data[4] = 0x04;
    break;
  case TrackData::MODE1:
  case TrackData::MODE1_RAW:
    data[4] = 0x01;
    break;
  case TrackData::MODE2:
    data[4] = 0xc1;
    break;
  case TrackData::MODE2_FORM1:
    data[4] = 0x81;
    break;
  case TrackData::MODE2_FORM2:
    data[4] = 0x82;
    break;
  case TrackData::MODE2_RAW:
  case TrackData::MODE2_FORM_MIX:
    data[4] = 0x83; // I'm not sure if this really allows writing of mixed
                    // form 1 and form 2 sectors.
    break;
  case TrackData::MODE0:
    log_message(-3, "Illegal mode in 'TeacCdr55::setWriteDensity()'.");
    return 0;
    break;
  }

  data[10] = blockLength >> 8;
  data[11] = blockLength;

  log_message(3, "Changing write density to 0x%02x/0x%02x%02x.", data[4], data[10],
	  data[11]);

  if (sendCmd(cmd, 6, data, 12, NULL, 0, 1) != 0) {
    log_message(-2, "Cannot set density/block size.");
    return 1;
  }

  return 0;
}


// Judges disc and performs power calibration if possible.
// judge: 1: judge disc, 0: don't judge disc
// Return 0: OK
//        1: judge disc rejected inserted medium
//        2: SCSI error occured
int TeacCdr55::executeOPC(int judge)
{
  unsigned char cmd[12];
  const unsigned char *senseCode;
  int senseLen;

  memset(cmd, 0, 12);

  cmd[0] = 0xec; // OPC EXECUTION

  if (judge) {
    cmd[1] = 1; // judge current disc

    log_message(2, "Judging disk...");
    switch (sendCmd(cmd, 12, NULL, 0, NULL, 0, 0)) {
    case 0: // OK, disc can be written at selected speed
      break;

    case 1:
      log_message(-2, "Judge disk command failed.");
      return 2;
      break;
      
    case 2: // Check sense code
      senseCode = scsiIf_->getSense(senseLen);
      if (senseLen > 12 && (senseCode[2] & 0x0f) == 5 && senseCode[7] != 0) {
	switch (senseCode[12]) {
	case 0xcd:
	  log_message(-2, "Cannot ensure reliable writing with inserted disk.");
	  return 1;
	  break;
	case 0xce:
	  log_message(-2, "Cannot ensure reliable writing at selected speed.");
	  return 1;
	  break;
	case 0xcf:
	  log_message(-2, "Cannot ensure reliable writing - disk has no ID code.");
	  return 1;
	  break;
	}
      }
      scsiIf_->printError();
      return 2;
      break;
    }
  }

  if (simulate()) {
    log_message(2, "Skipping optimum power calibration in simulation mode.");
    return 0;
  }

  cmd[1] = 0;

  log_message(2, "Performing optimum power calibration...");
  if (sendCmd(cmd, 12, NULL, 0, NULL, 0, 1) != 0) {
    log_message(-2, "Optimum power calibration failed.");
    return 2;
  }

  return 0;
}

// Clears the drive's subcode data.
// Return: 0: OK, 1: SCSI error
int TeacCdr55::clearSubcode()
{
  unsigned char cmd[12];

  memset(cmd, 0, 12);

  cmd[0] = 0xe4; // CLEAR SUBCODE
  cmd[5] = 0x80;

  if (sendCmd(cmd, 12, NULL, 0, NULL, 0, 1) != 0) {
    log_message(-2, "Clear subcode failed.");
    return 1;
  }
  
  return 0;
}

// Sets subcode data.
// start:   start LBA 
// end:     end LBA, one greater than last block with specfied subcode data
// ctl:     track mode flags in bits 4-7
// trackNr: track number
// indexNr: index number
// pflag:   P-channel flag
// return: 0: OK, 1: SCSI error
int TeacCdr55::setSubcode(long start, long end, unsigned char ctl, int trackNr,
			  int indexNr, int pflag)
{
  assert(start < end);

  unsigned char cmd[12];
  unsigned char data[4];

  long len = end - start;

  log_message(4,
	  "Setting subcode: start: %6ld, end: %6ld, len: %6ld, CTL: %02x P: %d, T: %2d I: %2d",
	  start, end, len, ctl, pflag, trackNr, indexNr);

  memset(cmd, 0, 12);

  cmd[0] = 0xb3; // SET LIMITS

  cmd[2] = start >> 24;
  cmd[3] = start >> 16;
  cmd[4] = start >> 8;
  cmd[5] = start;

  cmd[6] = len >> 24;
  cmd[7] = len >> 16;
  cmd[8] = len >> 8;
  cmd[9] = len;

  if (sendCmd(cmd, 12, NULL, 0, NULL, 0, 1) != 0) {
    log_message(-2, "Cannot set limits.");
    return 1;
  }

  memset(cmd, 0, 10);
  
  cmd[0] = 0xc2; // SET SUBCODE
  cmd[8] = 4; // parameter list length

  data[0] = pflag != 0 ? 1 : 0;
  data[1] = (ctl & 0xf0) | 0x01;
  data[2] = SubChannel::bcd(trackNr);
  data[3] = SubChannel::bcd(indexNr);

  if (sendCmd(cmd, 10, data, 4, NULL, 0, 1) != 0) {
    log_message(-2, "Cannot set subcode.");
    return 1;
  }
  
  return 0;
}
	
// Sets the subcodes for given track.
// track: actual track
// trackNr: number of current track
// start: LBA of track start (index 1), pre-gap length is taken from 'track'
// end:   LBA of track end, same as 'nstart' if next track has no pre-gap
// nstart: LBA of next track start (index 1)
// Return: 0: OK
//         1: SCSI command failed

struct SubCodeData {
  long start; // start LBA
  int index;  // index
  int pflag;  // P channel flag
};

int TeacCdr55::setSubcodes(const Track *track, int trackNr,
			   long start, long end, long nstart)
{
  assert(start < end);
  assert(end <= nstart);

  unsigned char ctl = trackCtl(track);
  SubCodeData subCodes[102]; // index 0-99 + P-channel entry + end-entry
  int n = 0;
  int i;

  if (track->start().lba() != 0) {
    // pre-gap
    subCodes[n].start = start - track->start().lba();
    subCodes[n].index = 0;
    subCodes[n].pflag = 1;
    n++;
  }

  // track start (index 1)
  subCodes[n].start = start;
  subCodes[n].index = 1;
  subCodes[n].pflag = 0;
  n++;

  // create index marks
  
  // We have to check if we have to add an entry for the P-channel flag if the
  // pre-gap of the following track is shorter than 2 seconds

  // calculate LBA where P-channel must be set for next track, if
  // it is behind 'end' it'll be ignored
  long pChangeLba = nstart - 150;

  int pChangeEntry = 0; // set to 1 if a P-channel entry was created
  long indexStart;

  for (i = 0; i < track->nofIndices(); i++) {
    indexStart = track->getIndex(i).lba() + start;

    if (indexStart > pChangeLba && pChangeEntry == 0) {
      subCodes[n].start = pChangeLba;
      subCodes[n].index = i + 1;
      subCodes[n].pflag = 1;
      n++;
      pChangeEntry = 1;
    }
    else if (indexStart == pChangeLba) {
      // index increment coincides with p channel change position
      // -> no entry required
      pChangeEntry = 1;
    }

    subCodes[n].start = indexStart;
    subCodes[n].index = i + 2;
    subCodes[n].pflag = indexStart >= pChangeLba ? 1 : 0;
    n++;
  }

  if (pChangeEntry == 0 && pChangeLba < end) {
    subCodes[n].start = pChangeLba;
    subCodes[n].index = i + 1;
    subCodes[n].pflag = 1;
    n++;
  }
  
  // add end of track as last entry
  subCodes[n].start = end;
  subCodes[n].index = 0; // not used
  subCodes[n].pflag = 0; // not used

  assert(n <= 101);

  // now issue the subcode commands
  for (i = 0; i < n; i++) {
    if (setSubcode(subCodes[i].start, subCodes[i+1].start, ctl, trackNr,
		   subCodes[i].index, subCodes[i].pflag) != 0) {
      return 1;
    }
  }

  return 0;
}

// Sets sub codes for all tracks.
// Return: 0: OK
//         1: toc contains no track
//         2: SCSI error
int TeacCdr55::setSubcodes()
{
  const Track *t, *nt;
  int trackNr;
  Msf start, end;
  Msf nstart, nend;
  long offset = diskInfo_.thisSessionLba;
  int first = 1;

  TrackIterator itr(toc_);

  trackNr = diskInfo_.lastTrackNr;
  if ((nt = itr.first(nstart, nend)) == NULL) {
    return 1;
  }

  if (clearSubcode() != 0)
    return 2;

  do {
    trackNr += 1;
    t = nt;
    start = nstart;
    end = nend;

    if ((nt = itr.next(nstart, nend)) == NULL)
      nstart = Msf(toc_->length()); // start of lead-out

    if (first) {
      // set sub code for pre-gap of first track
      first = 0;
      if (setSubcode(offset - 150, offset, trackCtl(t), trackNr, 0, 1) != 0)
	return 2;
    }

    if (setSubcodes(t, trackNr, start.lba() + offset, end.lba() + offset,
		    nstart.lba() + offset) != 0) {
      return 2;
    }

  } while (nt != NULL);

  return 0;
}

// Sets toc data.
// Return: 0: OK
//         1: toc contains no track
//         2: SCSI command failed
int TeacCdr55::setToc()
{
  const Track *t;
  int trackNr;
  Msf start, end;
  long offset = diskInfo_.thisSessionLba + 150; // LBA offset
  int trackOffset = diskInfo_.lastTrackNr + 1;
  unsigned char firstTrackCtl = 0;
  unsigned char lastTrackCtl = 0;
  unsigned char ctl;
  int lastTrackNr = 0;
  int sessFormat;
  unsigned char cmd[10];
  unsigned char data[3 + 102 * 5]; // max. 99 tracks + 3 POINT entries
  unsigned char *p = data + 3;

  data[0] = 0;
  data[1] = 0;
  data[2] = 0;

  TrackIterator itr(toc_);

  for (t = itr.first(start, end), trackNr = trackOffset;
       t != NULL && trackNr <= 99;
       t = itr.next(start, end), trackNr++) {

    Msf s(start.lba() + offset);

    ctl = trackCtl(t);
    if (trackNr == trackOffset)
      firstTrackCtl = ctl;

    p[0] = ctl | 0x01;
    p[1] = SubChannel::bcd(trackNr);
    p[2] = SubChannel::bcd(s.min());
    p[3] = SubChannel::bcd(s.sec());
    p[4] = SubChannel::bcd(s.frac());
    p += 5;

    lastTrackNr = trackNr;
    lastTrackCtl = ctl;
  }

  if (lastTrackNr == 0)
    return 1;

  
  // Point A0 entry (first track nr and session format)
  if (diskInfo_.empty) {
    // disk is empty -> take toc type from toc-file
    sessFormat = sessionFormat();
  }
  else {
    // Disk has already a recorded session -> take toc type from
    // previous session if it is well defined.
    switch (diskInfo_.diskTocType) {
    case 0x00:
    case 0x10:
    case 0x20:
      sessFormat = diskInfo_.diskTocType;
      break;
    default:
      sessFormat = sessionFormat();
      break;
    }
  }

  p[0] = firstTrackCtl | 0x01;
  p[1] = 0xa0;
  p[2] = SubChannel::bcd(trackOffset);
  p[3] = SubChannel::bcd(sessFormat);
  p[4] = 0;
  p += 5;

  // Point A1 entry (last track nr)
  p[0] = lastTrackCtl | 0x01;
  p[1] = 0xa1;
  p[2] = SubChannel::bcd(lastTrackNr);
  p[3] = 0;
  p[4] = 0;
  p += 5;
  
  // Point A2 entry (start of lead-out)
  Msf los(toc_->length().lba() + offset);

  p[0] = lastTrackCtl | 0x01;
  p[1] = 0xa2;
  p[2] = SubChannel::bcd(los.min());
  p[3] = SubChannel::bcd(los.sec());
  p[4] = SubChannel::bcd(los.frac());
  p += 5;
  

  long dataLen = p - data;

#if 1
  log_message(4, "TOC data:");
  long i;
  for (i = 3; i < dataLen; i += 5) {
    log_message(4, "%02x %02x %02x:%02x:%02x", 
	    data[i], data[i+1], data[i+2], data[i+3], data[i+4]);
  }
#endif


  memset(cmd, 0, 10);

  cmd[0] = 0xc2; // SET SUBCODE
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, data, dataLen, NULL, 0, 1) != 0) {
    log_message(-2, "Cannot set TOC data.");
    return 2;
  }
   
  return 0;
}

// Sets catalog number.
// Return: 0: OK
//         1: SCSI command failed
int TeacCdr55::setCatalog()
{
  unsigned char cmd[10];
  unsigned char data[15];
  int i;

  if (!toc_->catalogValid())
    return 0;

  data[0] = 0;

  if (toc_->leadInMode() == TrackData::AUDIO)
    data[1] = 0x02;
  else
    data[1] = 0x42;

  for (i = 0; i < 13; i++)
    data[2 + i] = toc_->catalog(i);


  memset(cmd, 0, 10);

  cmd[0] = 0xc2; // SET SUBCODE
  cmd[8] = 15;

  if (sendCmd(cmd, 10, data, 15, NULL, 0, 1) != 0) {
    log_message(-2, "Cannot set catalog number data.");
    return 2;
  }

  return 0;
}

// Sets ISRC codes for all tracks
int TeacCdr55::setIsrc()
{
  const Track *t;
  int trackNr;
  Msf start, end;
  int trackOffset = diskInfo_.lastTrackNr + 1;
  unsigned char cmd[10];
  unsigned char data[15];

  data[0] = 0;

  memset(cmd, 0, 10);

  cmd[0] = 0xc2; // SET SUBCODE
  cmd[8] = 15;

  TrackIterator itr(toc_);

  for (t = itr.first(start, end), trackNr = trackOffset;
       t != NULL && trackNr <= 99;
       t = itr.next(start, end), trackNr++) {
    if (t->isrcValid()) {
      data[1] = trackCtl(t) | 0x03;
      data[2] = t->isrcCountry(0);
      data[3] = t->isrcCountry(1);
      data[4] = t->isrcOwner(0);
      data[5] = t->isrcOwner(1);
      data[6] = t->isrcOwner(2);
      data[7] = t->isrcYear(0) + '0';
      data[8] = t->isrcYear(1) + '0';
      data[9] = t->isrcSerial(0) + '0';
      data[10] = t->isrcSerial(1) + '0';
      data[11] = t->isrcSerial(2) + '0';
      data[12] = t->isrcSerial(3) + '0';
      data[13] = t->isrcSerial(4) + '0';
      data[14] = 0;

      cmd[6] = trackNr;
      if (sendCmd(cmd, 10, data, 15, NULL, 0, 1) != 0) {
	log_message(-2, "Cannot set ISRC code.");
	return 2;
      }
    }
  }

  return 0;
}


// Need to overload this function to set the WriteExtension flag. It'll
// also change the write density if the actual mode changes.
int TeacCdr55::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
			 long &lba, const char *buf, long len)
{
  assert(blocksPerWrite_ > 0);
  int writeLen = 0;
  unsigned char cmd[10];
  long blockLength = blockSize(mode, TrackData::SUBCHAN_NONE);
  
#if 0
  long sum, i;

  sum = 0;

  for (i = 0; i < len * blockLength; i++) {
    sum += buf[i];
  }

  log_message(0, "W: %ld: %ld, %ld, %ld", lba, blockLength, len, sum);
#endif

  if (mode != actMode_) {
    actMode_ = mode;
    log_message(3, "Changed write density at LBA %ld.", lba);

    if (setWriteDensity(actMode_) != 0) {
      return 1;
    }
  }

  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE10
  
  while (len > 0) {
    writeLen = (len > blocksPerWrite_ ? blocksPerWrite_ : len);

    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;

    cmd[7] = writeLen >> 8;
    cmd[8] = writeLen & 0xff;

    if (lba + writeLen >= writeEndLba_) {
      // last write command
      log_message(4, "Last write command at LBA: %ld", lba);
      cmd[9] = 0;
    }
    else {
      cmd[9] = 0x80; // extended write
    }

    if (sendCmd(cmd, 10, (unsigned char *)buf, writeLen * blockLength,
		NULL, 0) != 0) {
      log_message(-2, "Write data failed.");
      return 1;
    }

    buf += writeLen * blockLength;

    lba += writeLen;
    len -= writeLen;
  }
      
  return 0;
}


int TeacCdr55::initDao(const Toc *toc)
{
  long offset; // LBA offset for this session
  long n;
  
  toc_ = toc;

  blockLength_ = AUDIO_BLOCK_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

  diskInfo();

  if (!diskInfo_.valid.empty || !diskInfo_.valid.append) {
    log_message(-2, "Cannot determine status of inserted medium.");
    return 1;
  }

  if (!diskInfo_.append) {
    log_message(-2, "Inserted medium is not appendable.");
    return 1;
  }

  offset = diskInfo_.thisSessionLba;


  // allocate buffer for writing zeros
  n = blocksPerWrite_ * blockLength_;
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);


  writeEndLba_ = toc_->length().lba() + offset;
  actMode_ = toc_->leadInMode();
  
  if (getModeSelectData() != 0 ||
      setWriteDensity(actMode_) != 0 ||
      setSimulationMode() != 0 ||
      setWriteParameters() != 0 ||
      setWriteSpeed() != 0) {
    return 1;
  }

  return 0;
}

int TeacCdr55::startDao()
{
  scsiTimeout_ = scsiIf_->timeout(3 * 60);

  if (executeOPC(1) != 0 ||
      setSubcodes() != 0 ||
      setCatalog() != 0 ||
      setToc() != 0 ||
      setIsrc() != 0) {
    return 1;
  }

  long lba = diskInfo_.thisSessionLba - 150;

  if (writeZeros(actMode_, TrackData::SUBCHAN_NONE, lba, lba + 150, 150)
      != 0) {
    return 1;
  }
  
  return 0;
}

int TeacCdr55::finishDao()
{
  //log_message(0, "Writing lead-out...");

  // wait until writing of lead-out is finished ???

  scsiIf_->timeout(scsiTimeout_);

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void TeacCdr55::abortDao()
{
  unsigned char cmd[10];

  // Send a zero length write command with cleared extendened write bit.
  // I don't know if it has the expected effect.

  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE10

  sendCmd(cmd, 10, NULL, 0, NULL, 0, 0);
}

DiskInfo *TeacCdr55::diskInfo()
{
  int i;
  unsigned char cmd[12];
  unsigned char data[12];

  memset(&diskInfo_, 0, sizeof(DiskInfo));

  diskInfo_.cdrw = 0;
  diskInfo_.valid.cdrw = 1;

  memset(cmd, 0, 10);
  memset(data, 0, 8);

  cmd[0] = 0x25; // READ CAPACITY
  cmd[9] = 0x80;

  if (sendCmd(cmd, 10, NULL, 0, data, 8, 1) != 0) {
    log_message(-1, "Cannot read CD-R capacity.");
  }
  else {
    diskInfo_.capacity = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) |
                        data[3];
    diskInfo_.valid.capacity = 1;
  }

  memset(cmd, 0, 12);
  memset(data, 0, 4);

  cmd[0] = 0xe6; // NEXT WRITABLE ADDRESS
  cmd[2] = 0xff;
  cmd[3] = 0xff;
  cmd[4] = 0xff;
  cmd[5] = 0xff;
  cmd[6] = 0xff;
  cmd[7] = 0xff;
  cmd[8] = 0xff;
  cmd[9] = 0xff;

  long nwa = 0;

  switch (sendCmd(cmd, 12, NULL, 0, data, 4, 0)) {
  case 0: // command succeed -> writable area exists
    diskInfo_.empty = 1;
    diskInfo_.valid.empty = 1;
    nwa = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    log_message(4, "Next writable address: %ld", nwa);
    break;
  case 1: // SCSI command failed, no sense data -> cannot tell anything
    log_message(-1, "Get next writable address failed.");
    break;
  case 2: // SCSI command failed, sense data available
    // assume that disk is not writable
    log_message(4, "Cannot get next writable address.");
    diskInfo_.empty = 0;
    diskInfo_.valid.empty = 1;
    break;
  }

  // SCSI command failed -> stop further processing
  if (diskInfo_.valid.empty == 0)
    return &diskInfo_;

  memset(cmd, 0, 10);
  memset(data, 0, 4);

  cmd[0] = 0x43; // READ TOC
  cmd[6] = 1;
  cmd[8] = 4;

  if (sendCmd(cmd, 10, NULL, 0, data, 4, 0) == 0) {
    log_message(4, "First track %u, last track %u", data[2], data[3]);
    diskInfo_.lastTrackNr = data[3];
  }
  else {
    log_message(4, "READ TOC (format 0) failed.");
  }

  if (diskInfo_.lastTrackNr > 0) {
    diskInfo_.empty = 0;
    diskInfo_.diskTocType = 0xff; // undefined

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

      log_message(4, "First session %u, last session %u, last session start %ld",
	      data[2], data[3], diskInfo_.lastSessionLba);
    }
    else {
      log_message(4, "READ TOC (format 1) failed.");
    }

    if (diskInfo_.sessionCnt > 0) {
      int len;
      CdRawToc *toc = getRawToc(diskInfo_.sessionCnt, &len);

      if (toc != NULL) {
	for (i = 0; i < len; i++) {
	  if (toc[i].sessionNr == diskInfo_.sessionCnt) {
	    if (toc[i].point == 0xb0 && toc[i].min != 0xff &&
		toc[i].sec != 0xff && toc[i].frame != 0xff) {
	      int m = SubChannel::bcd2int(toc[i].min);
	      int s = SubChannel::bcd2int(toc[i].sec);
	      int f = SubChannel::bcd2int(toc[i].frame);
	      
	      if (m < 90 && s < 60 && f < 75) 
		diskInfo_.thisSessionLba = Msf(m, s, f).lba(); // + 150 - 150
	    }

	    if (toc[i].point == 0xa0) {
	      diskInfo_.diskTocType = SubChannel::bcd2int(toc[i].psec);
	    }
	  }
	}

	if (diskInfo_.thisSessionLba > 0) {
	  if (diskInfo_.lastTrackNr < 99)
	    diskInfo_.append = 1;
	}
	else {
	  log_message(4, "Did not find BO pointer in session %d.",
		  diskInfo_.sessionCnt);
	}

	delete[] toc;
      }
      else {
	log_message(4, "getRawToc failed.");
      }
    }
  }
  else {
    // disk is empty and appendable
    diskInfo_.empty = 1;
    diskInfo_.append = 1;
  }

  diskInfo_.valid.append = 1;

  return &diskInfo_;
}


// tries to read catalog number from disk and adds it to 'toc'
// return: 1 if valid catalog number was found, else 0
int TeacCdr55::readCatalog(Toc *toc, long startLba, long endLba)
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

int TeacCdr55::readIsrc(int trackNr, char *buf)
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

int TeacCdr55::analyzeTrack(TrackData::Mode mode, int trackNr,
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

int TeacCdr55::readSubChannels(TrackData::SubChannelMode,
			       long lba, long len, SubChannel ***chans,
			       Sample *audioData)
{
  int retries = 5;
  unsigned char cmd[12];
  int i;

  cmd[0] = 0xd8;  // READ CD_DA
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = len >> 24;
  cmd[7] = len >> 16;
  cmd[8] = len >> 8;
  cmd[9] = len;
  cmd[10] = 0x01;  // Q sub-channel data
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


CdRawToc *TeacCdr55::getRawToc(int sessionNr, int *len)
{
  unsigned char cmd[10];
  unsigned short dataLen;
  unsigned char *data = NULL;;
  unsigned char reqData[4]; // buffer for requestion the actual length
  unsigned char *p = NULL;
  int i, entries;
  CdRawToc *rawToc;
  
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

  log_message(4, "Raw toc data len: %d", dataLen);

  if (dataLen == 4)
    return NULL;

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
    log_message(0, "%d %02x %02d %2x %02x:%02x:%02x %02x %02x:%02x:%02x",
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

long TeacCdr55::readTrackData(TrackData::Mode mode,
			      TrackData::SubChannelMode,
			      long lba, long len, unsigned char *buf)
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
	log_message(-3, "TeacCdr55::readTrackData: Illegal mode.");
	return 0;
	break;
      }
    }

    sector += blockLen;
  }  

  return len;
}

Toc *TeacCdr55::readDiskToc(int session, const char *audioFilename)
{
  Toc *toc = CdrDriver::readDiskToc(session, audioFilename);

  setBlockSize(MODE1_BLOCK_LEN);
  
  return toc;
}

Toc *TeacCdr55::readDisk(int session, const char *fname)
{
  Toc *toc = CdrDriver::readDisk(session, fname);

  setBlockSize(MODE1_BLOCK_LEN);

  return toc;
}

int TeacCdr55::readAudioRange(ReadDiskInfo *rinfo, int fd, long start,
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

