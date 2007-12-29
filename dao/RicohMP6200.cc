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

/* Driver for the Ricoh MP6200 drive. It's mainly SCSI-3/mmc compatible but
 * disk-at-once writing is done with the Philips CDD2x00 commands.
 */

#include <config.h>

#include <string.h>
#include <assert.h>

#include "RicohMP6200.h"
#include "SubChannel.h"
#include "Toc.h"
#include "log.h"

RicohMP6200::RicohMP6200(ScsiIf *scsiIf, unsigned long options)
  : GenericMMC(scsiIf, options), CDD2600Base(this)
{
  driverName_ = "Ricoh MP6200 - Version 0.1(alpha)";
  
  simulate_ = true;
  encodingMode_ = 0;
}

RicohMP6200::~RicohMP6200()
{
}

// static constructor
CdrDriver *RicohMP6200::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new RicohMP6200(scsiIf, options);
}


// Sets write parameters via mode page 0x05.
// return: 0: OK
//         1: scsi command failed
int RicohMP6200::setWriteParameters()
{
  int i;
  unsigned char mp[0x38];

  if (getModePage(5/*write parameters mode page*/, mp, 0x38, 
		  NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot retrieve write parameters mode page.");
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag

  mp[2] &= 0xef;

  if (simulate_) {
    mp[2] |= 1 << 4; // test write
  }

  mp[3] &= 0x3f; // Multi-session: No B0 pointer, next session not allowed
  if (multiSession_ != 0)
    mp[3] |= 0x03 << 6; // open next session

  if (toc_->catalogValid()) {
    mp[16] = 0x80;

    for (i = 0; i < 13; i++)
      mp[17 + i] = toc_->catalog(i) + '0';

    mp[30] = 0;
    mp[31] = 0;
  }
  else {
    mp[16] = 0;
  }
  
  if (setModePage(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set write parameters mode page.");
    return 1;
  }

  return 0;
}


int RicohMP6200::initDao(const Toc *toc)
{
  long n;
  blockLength_ = AUDIO_BLOCK_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

  toc_ = toc;

  // the ATAPI version does not like the following command so a failure
  // is non fatal
  modeSelectBlockSize(blockLength_, 0);

  if (readSessionInfo(&leadInLen_, &leadOutLen_, 1) != 0 ||
      setWriteParameters() != 0)
    return 1;

  // allocate buffer for write zeros
  n = blocksPerWrite_ * blockLength_;
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);

  return 0;
}

int RicohMP6200::startDao()
{
  long lba = 0;

  if (writeSession(toc_, multiSession_, 0) != 0) {
    return 1;
  }

  log_message(2, "Writing lead-in and gap...");

  // write lead-in
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, 0,
		 leadInLen_) != 0) {
    flushCache();
    return 1;
  }

  // write gap (2 seconds)
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, 0, 150)
      != 0) {
    flushCache();
    return 1;
  }

  log_message(2, "");

  return 0;
}

int RicohMP6200::finishDao()
{
  long lba = toc_->length().lba();

  log_message(2, "Writing lead-out...");

  // write lead-out
  if (writeZeros(toc_->leadOutMode(), TrackData::SUBCHAN_NONE, lba, lba + 150,
		 leadOutLen_) != 0) {
    flushCache();
    return 1;
  }

  log_message(2, "\nFlushing cache...");
  
  if (flushCache() != 0) {
    return 1;
  }

  log_message(2, "");

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void RicohMP6200::abortDao()
{
  flushCache();
}

// Writes data to target, the block length depends on the actual writing mode
// and is stored internally. 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function but not used for writing
// return: 0: OK
//         1: scsi command failed
int RicohMP6200::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
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

// loads ('unload' == 0) or ejects ('unload' == 1) tray
// return: 0: OK
//         1: scsi command failed

int RicohMP6200::loadUnload(int unload) const
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
