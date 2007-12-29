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

#include "CDD2600Base.h"
#include "SubChannel.h"
#include "Toc.h"
#include "log.h"

CDD2600Base::CDD2600Base(CdrDriver *driver)
{
  driver_ = driver;
}

CDD2600Base::~CDD2600Base()
{
  driver_ = NULL;
}

// sets the block length
// return: 0: OK
//         1: scsi command failed
int CDD2600Base::modeSelectBlockSize(int blockSize, int showMsg)
{
  unsigned char cmd[6];
  unsigned char data[12];

  memset(cmd, 0, 6);
  memset(data, 0, 12);

  cmd[0] = 0x15; // MODE SELECT
  cmd[4] = 12;

  data[3] = 8;

  data[9] = blockSize >> 16;
  data[10] = blockSize >> 8;
  data[11] = blockSize;

  if (driver_->sendCmd(cmd, 6, data, 12, NULL, 0, showMsg) != 0) {
    if (showMsg)
      log_message(-2, "Cannot set block size to %d.", blockSize);
    return 1;
  }

  return 0;
}

// sets read/write speed and simulation mode
// return: 0: OK
//         1: scsi command failed
int CDD2600Base::modeSelectSpeed(int readSpeed, int writeSpeed, int simulate,
				 int showMessage)
{
  unsigned char mp[8];

  memset(mp, 0, 8);

  mp[0] = 0x23;
  mp[1] = 0x06;
  if (writeSpeed >= 0) {
    mp[2] = writeSpeed;
  }
  mp[3] = (simulate != 0 ? 1 : 0);
  if (readSpeed >= 0) {
    mp[4] = readSpeed;
  }

  if (driver_->setModePage(mp, NULL, NULL, showMessage) != 0) {
    if (showMessage) {
      log_message(-2, "Cannot set speed/simulation mode.");
    }
    return 1;
  }

  return 0;
}

// Sets catalog number if valid in given TOC.
// return: 0: OK
//         1: scsi command failed
int CDD2600Base::modeSelectCatalog(const Toc *toc)
{
  unsigned char mp[10];

  memset(mp, 0, 10);

  mp[0] = 0x22;
  mp[1] = 0x08;

  if (toc->catalogValid()) {
    mp[2] = 0x01;
    mp[3] = (toc->catalog(0) << 4) | toc->catalog(1);
    mp[4] = (toc->catalog(2) << 4) | toc->catalog(3);
    mp[5] = (toc->catalog(4) << 4) | toc->catalog(5);
    mp[6] = (toc->catalog(6) << 4) | toc->catalog(7);
    mp[7] = (toc->catalog(8) << 4) | toc->catalog(9);
    mp[8] = (toc->catalog(10) << 4) | toc->catalog(11);
    mp[9] = (toc->catalog(12) << 4);
  }

  if (driver_->setModePage(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set catalog number.");
    return 1;
  }

  return 0;
}


// Requests length of lead-in and lead-out.
// return: 0: OK
//         1: scsi command failed
int CDD2600Base::readSessionInfo(long *leadInLen, long *leadOutLen,
				 int showMessage)
{
  unsigned char cmd[10];
  unsigned char data[4];

  memset(cmd, 0, 10);
  memset(data, 0, 4);

  cmd[0] = 0xee; // READ SESSION INFO
  cmd[8] = 4;

  if (driver_->sendCmd(cmd, 10, NULL, 0, data, 4, showMessage) != 0) {
    if (showMessage)
      log_message(-2, "Cannot read session info.");
    return 1;
  }

  *leadInLen = (data[0] << 8) | data[1];
  *leadOutLen = (data[2] << 8) | data[3];

  log_message(4, "Lead-in length: %ld, lead-out length: %ld", *leadInLen,
	  *leadOutLen);

  return 0;
}

// Sends initiating write session command for disk at once writing. This
// includes the complete table of contents with all related data.
// return: 0: OK
//         1: scsi command failed
int CDD2600Base::writeSession(const Toc *toc, int multiSession, long lbaOffset)
{
  unsigned char cmd[10];
  unsigned char *data = NULL;
  unsigned int dataLen = 0;
  unsigned char *tp = NULL;
  unsigned int tdl = 0; // track descriptor length
  int cdromMode = 0;
  int indexCount = 0;
  int i;
  int n;
  const Track *t;
  Msf start, end, index;

  TrackIterator itr(toc);

  // count total number of index increments
  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    indexCount += t->nofIndices();
  }

  dataLen = toc->nofTracks() * 20 + indexCount * 4;

  /*
  log_message(0, "%d tracks, %d indexes -> dataLen %u", toc->nofTracks(),
	  indexCount, dataLen);
  */

  data = new unsigned char[dataLen];
  memset(data, 0, dataLen);
  tp = data;

  memset(cmd, 0, 10);

  cmd[0] = 0xed; // write session

  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  // setup toc-type, lead-out type and multi session flag
  cmd[6] = 0;


  switch (toc->tocType()) {
  case Toc::CD_DA:
  case Toc::CD_ROM:
    cmd[6] = 0;
    break;
  case Toc::CD_ROM_XA:
    cmd[6] = 3;
    break;
  case Toc::CD_I:
    cmd[6] = 4;
    break;
  }

  if (cdromMode) {
    // Programming manual says that lead-out fixation parameter is only

    // valid if the toc-type is CD-ROM. Unfortunately it does not tell
    // which values to set.
#if 0
    switch (toc->leadOutMode()) {
    case TrackData::MODE1:
      cmd[6] |= 1 << 4;
      break;
    case TrackData::MODE2:
    case TrackData::MODE2_FORM1:
    case TrackData::MODE2_FORM2:
    case TrackData::MODE2_FORM_MIX:
      cmd[6] |= 2 << 4;
      break;
    default:
      break;
    }
#endif
  }

  if (multiSession != 0) 
    cmd[6] |= 1 << 3; // open next session
  
  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    n = t->nofIndices();
    tdl = 20 + n * 4;
    tp[0] = tdl >> 8;
    tp[1] = tdl;

    start = Msf(start.lba() + lbaOffset);
    end = Msf(end.lba() + lbaOffset);

    tp[2] = 0;
    if (t->copyPermitted()) {
      tp[2] |= 0x02;
    }
    if (t->type() == TrackData::AUDIO) {
      // audio track
      if (t->preEmphasis()) {
	tp[2] |= 0x01;
      }
      if (t->audioType() == 1) {
	tp[2] |= 0x08;
      }
      
      if (t->isrcValid()) {
	tp[2] |= 0x80;
      
	tp[3] = SubChannel::ascii2Isrc(t->isrcCountry(0));
	tp[4] = SubChannel::ascii2Isrc(t->isrcCountry(1));
	tp[5] = SubChannel::ascii2Isrc(t->isrcOwner(0));
	tp[6] = SubChannel::ascii2Isrc(t->isrcOwner(1));
	tp[7] = SubChannel::ascii2Isrc(t->isrcOwner(2));
	tp[8] = (t->isrcYear(0) << 4) | t->isrcYear(1);
	tp[9] = (t->isrcSerial(0) << 4) | t->isrcSerial(1);
	tp[10] = (t->isrcSerial(2) << 4) | t->isrcSerial(3);
	tp[11] = (t->isrcSerial(4) << 4);
      }
    }
    else {
      // data track
      tp[2] |= 0x04;
    }

    log_message(4, "Track start: %s(0x%06lx)", start.str(), start.lba());
    tp[12] = start.lba() >> 24;
    tp[13] = start.lba() >> 16;
    tp[14] = start.lba() >> 8;
    tp[15] = start.lba();

    for (i = 0; i < n; i++) {
      index = start + t->getIndex(i);
      log_message(4, "      index: %s(0x%06lx)", index.str(), index.lba());
      
      tp[16 + i * 4] = index.lba() >> 24;
      tp[17 + i * 4] = index.lba() >> 16;
      tp[18 + i * 4] = index.lba() >> 8;
      tp[19 + i * 4] = index.lba();
    }

    log_message(4, "      end  : %s(0x%06lx)", end.str(), end.lba());
    
    tp[16 + n * 4] = end.lba() >> 24;
    tp[17 + n * 4] = end.lba() >> 16;
    tp[18 + n * 4] = end.lba() >> 8;
    tp[19 + n * 4] = end.lba();

    tp += tdl;
  }

  //log_message(0, "tp: %d", tp - data);

  if (driver_->sendCmd(cmd, 10, data, dataLen, NULL, 0) != 0) {
    log_message(-2, "Cannot write disk toc.");
    delete[] data;
    return 1;
  }

  delete[] data;
  return 0;
}

