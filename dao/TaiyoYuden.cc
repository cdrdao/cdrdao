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

/* Driver for the TaiyoYuden drive created by Henk-Jan Slotboom.
 * Very similar to the Philips CDD2x00 drives.
 */

#include <config.h>

#include <string.h>
#include <assert.h>

#include "TaiyoYuden.h"
#include "SubChannel.h"

#include "Toc.h"
#include "log.h"

TaiyoYuden::TaiyoYuden(ScsiIf *scsiIf, unsigned long options)
  : PlextorReader(scsiIf, options), CDD2600Base(this)
{
  driverName_ = "Taiyo-Yuden - Version 0.1(alpha)";
  
  leadInLength_ = leadOutLength_ = 0;
  speed_ = 2;
  simulate_ = true;
  encodingMode_ = 0;

  audioDataByteOrder_ = 0; // little endian

  memset(&diskInfo_, 0, sizeof(DiskInfo));
}

TaiyoYuden::~TaiyoYuden()
{
}

// static constructor
CdrDriver *TaiyoYuden::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new TaiyoYuden(scsiIf, options);
}


// sets speed
// return: 0: OK
//         1: illegal speed
int TaiyoYuden::speed(int s)
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
int TaiyoYuden::loadUnload(int unload) const
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
};

int TaiyoYuden::initDao(const Toc *toc)
{
  long n;

  toc_ = toc;

  blockLength_ = AUDIO_BLOCK_LEN;
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;

  assert(blocksPerWrite_ > 0);

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

int TaiyoYuden::startDao()
{
  long lba = -leadInLength_ - 150; // Value is not really important since the
                                   // LBA is not used by 'writeData'.
 
  if (writeSession(toc_, multiSession_, 0) != 0) {
    return 1;
  }

  log_message(2, "Writing lead-in and gap...");

  // write lead-in
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, 0,
		 leadInLength_) != 0) {
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

int TaiyoYuden::finishDao()
{
  long lba = toc_->length().lba();

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

  delete[] zeroBuffer_, zeroBuffer_ = NULL;

  return 0;
}

void TaiyoYuden::abortDao()
{
  flushCache();
}

// Writes data to target, the block length depends on the actual writing mode
// and is stored internally. 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function but not used for writing
// return: 0: OK
//         1: scsi command failed
int TaiyoYuden::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
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

// Retrieve disk information.
// return: DiskInfo structure or 'NULL' on error
DiskInfo *TaiyoYuden::diskInfo()
{
  unsigned char cmd[10];
  unsigned long dataLen = 34;
  unsigned char data[34];
  
  memset(&diskInfo_, 0, sizeof(DiskInfo));
  
  if (readCapacity(&(diskInfo_.capacity), 0) == 0) {
    diskInfo_.valid.capacity = 1;
  }
  
  if (readSessionInfo(&leadInLength_, &leadOutLength_, 0) == 0) {
    diskInfo_.valid.manufacturerId = 1;
  			        // start time of lead-in
    diskInfo_.manufacturerId = Msf(450150 - leadInLength_ - 150 );
    diskInfo_.empty = 1;
  }
  else {
    diskInfo_.empty = 0;
  }

  diskInfo_.valid.empty = 0; // does not work for this drive
  
  memset(cmd, 0, 10);
  memset(data, 0, dataLen);
  
  cmd[0] = 0x51; // READ DISK INFORMATION
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;
   
  if (sendCmd(cmd, 10, NULL, 0, data, dataLen, 0) == 0) {
    diskInfo_.empty = (data[2] & 0x03) == 0 ? 1 : 0;
    diskInfo_.cdrw = (data[2] & 0x10) != 0 ? 1 : 0;
    diskInfo_.valid.cdrw = 1;
  }

  return &diskInfo_;
}

// Special toc check for Taiyo-Yuden drives. Every track must have a pre-gap
// to create a correct disk. The pre-gap length may be only 1 block.
// Only warnings are created.
// Return: 0: OK
//         1: at least one warning was given
//         2: at least on error was given

int TaiyoYuden::checkToc(const Toc *toc)
{
  int err = PlextorReader::checkToc(toc);
  int trackNr;
  Msf start, end;
  const Track *t;
  int warningGiven = 0;

  TrackIterator itr(toc);

  for (t = itr.first(start, end), trackNr = 1;
       t != NULL;
       t = itr.next(start, end), trackNr++) {
    if (t->start().lba() == 0 && trackNr > 1) {
      if (!warningGiven) {
	warningGiven = 1;
	log_message(-1, "Each track must have a minimal pre-gap to create a useful disk.");
	log_message(-1, "This is not fulfilled for following track(s):");
      }
      log_message(-1, "Track %d", trackNr);
    }
  }

  if (warningGiven) {
    log_message(0,"");
    if (err < 1)
      err = 1;
  }

  return err;
}
