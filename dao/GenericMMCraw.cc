/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
/*
 * $Log: GenericMMCraw.cc,v $
 * Revision 1.2  2000/04/24 12:47:57  andreasm
 * Fixed unit attention problem after writing is finished.
 * Added cddb disk id calculation.
 *
 * Revision 1.1.1.1  2000/02/05 01:36:30  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.10  1999/09/03 15:00:02  mueller
 * Changed message levels from 0 to 1.
 *
 * Revision 1.9  1999/04/05 11:04:10  mueller
 * Added driver option flags.
 *
 * Revision 1.8  1999/03/27 20:51:05  mueller
 * Adapted to changed writing interface.
 *
 * Revision 1.7  1998/10/03 15:07:53  mueller
 * Moved 'writeZeros()' to base class 'CdrDriver'.
 *
 * Revision 1.6  1998/09/27 19:19:18  mueller
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 * Added multi session mode.
 *
 * Revision 1.5  1998/09/22 19:15:13  mueller
 * Removed memory allocations during write process.
 *
 * Revision 1.4  1998/09/06 13:34:22  mueller
 * Use 'message()' for printing messages.
 *
 * Revision 1.3  1998/09/02 18:49:45  mueller
 * Added writing with data block type 0x02 using the raw P-W sub channel
 * data format.
 *
 * Revision 1.2  1998/08/30 19:16:51  mueller
 * Added support for different sub-channel data formats.
 * Now 16 byte PQ sub-channel and 96 byte raw P-W sub-channel is
 * supported.
 *
 * Revision 1.1  1998/08/25 19:28:06  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: GenericMMCraw.cc,v 1.2 2000/04/24 12:47:57 andreasm Exp $";

#include <config.h>

#include <string.h>
#include <assert.h>

#include "GenericMMCraw.h"
#include "PQSubChannel16.h"
#include "PWSubChannel96.h"

#include "Toc.h"
#include "util.h"
#include "port.h"

GenericMMCraw::GenericMMCraw(ScsiIf *scsiIf, unsigned long options) 
  : GenericMMC(scsiIf, options), PQChannelEncoder()
{
  driverName_ = "Generic SCSI-3/MMC (raw writing) - Version 1.0";

  encodingMode_ = 0;

  leadInLen_ = leadOutLen_ = 0;
  subChannel_ = NULL;
  encodeBuffer_ = NULL;
}

GenericMMCraw::~GenericMMCraw()
{
  delete subChannel_, subChannel_ = NULL;
  delete[] encodeBuffer_, encodeBuffer_ = NULL;
}

int GenericMMCraw::multiSession(int m)
{
  if (m != 0) {
    return 1; // multi session mode is currently not support for raw writing
  }

  return 0;
}

// static constructor
CdrDriver *GenericMMCraw::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new GenericMMCraw(scsiIf, options);
}

// Sets write parameters via mode page 0x05.
// return: 0: OK
//         1: scsi command failed
int GenericMMCraw::setWriteParameters(int dataBlockType)
{
  unsigned char mp[0x38];

  if (getModePage(5/*write parameters mode page*/, mp, 0x38,
		  NULL, NULL, 1) != 0) {
    message(-2, "Cannot retrieve write parameters mode page.");
    return 1;
  }

  mp[0] &= 0x7f; // clear PS flag

  mp[2] &= 0xe0;
  mp[2] |= 0x03; // write type: raw
  if (simulate_) {
    mp[2] |= 1 << 4; // test write
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
    //message(-2, "Cannot set write parameters mode page.");
    return 1;
  }

  return 0;
}

int GenericMMCraw::initDao(const Toc *toc)
{
  long n;

  CdrDriver::toc_ = toc;

  if (selectSpeed(0) != 0 ||
      getSessionInfo() != 0) {
    return 1;
  }

  delete subChannel_;

#if 1
  if (setWriteParameters(1) == 0) {
    subChannel_ = new PQSubChannel16;
    message(1, "Using 16 byte P-Q sub-channel data mode.");
  }
  else if (setWriteParameters(3) == 0) {
    subChannel_ = new PWSubChannel96;
    message(1, "Using 96 byte raw P-W sub-channel data mode.");
  }
  else if (setWriteParameters(2) == 0) {
    // since we don't use the R-W channels it's the same as the latter mode,
    // I think
    subChannel_ = new PWSubChannel96;
    message(1, "Using 96 byte packed P-W sub-channel data mode.");
  }
  else {
    message(-2, "Cannot setup disk-at-once writing for this drive.");
    return 1;
  }
#else
  //subChannel_ = new PWSubChannel96;
  subChannel_ = new PQSubChannel16;
#endif

  blockLength_ = AUDIO_BLOCK_LEN + subChannel_->dataLength();
  blocksPerWrite_ = scsiIf_->maxDataLen() / blockLength_;
  assert(blocksPerWrite_ > 0);
  message(3, "Block length: %ld", blockLength_);

  long cueSheetLen;
  unsigned char *cueSheet = createCueSheet(&cueSheetLen);

  if (cueSheet == NULL) {
    return 1;
  }
  
  if (setCueSheet(subChannel_, sessionFormat(), cueSheet, cueSheetLen,
		  leadInStart_) != 0) {
    return 1;
  }

  // allocate buffer for write zeros
  n = blocksPerWrite_ * AUDIO_BLOCK_LEN;
  delete[] zeroBuffer_;
  zeroBuffer_ = new char[n];
  memset(zeroBuffer_, 0, n);

  // allocate buffer for sub-channel encoding
  n = blocksPerWrite_ * blockLength_;
  delete[] encodeBuffer_;
  encodeBuffer_ = new (unsigned char)[n];
  
  return 0;
}

int GenericMMCraw::startDao()
{
  message(1, "Writing lead-in and gap...");

  long lba = leadInStart_.lba() - 450150;
  
  if (writeZeros(CdrDriver::toc_->leadInMode(), lba, 0, leadInLen_) != 0) {
    return 1;
  }

  if (writeZeros(CdrDriver::toc_->leadInMode(), lba, 0, 150) != 0) {
    return 1;
  }

  return 0;
}

int GenericMMCraw::finishDao()
{
  int ret;

  message(1, "Writing lead-out...");

  long lba = CdrDriver::toc_->length().lba();

  writeZeros(CdrDriver::toc_->leadOutMode(), lba, lba + 150, leadOutLen_);

  message(1, "\nFlushing cache...");
  
  if (flushCache() != 0) {
    return 1;
  }

  while ((ret = testUnitReady(0)) == 2)
    mSleep(2000);

  if (ret != 0)
    message(-1, "TEST UNIT READY failed after recording.");

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
    message(-2, "Cannt get track information.");
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
int GenericMMCraw::writeData(TrackData::Mode mode, long &lba, const char *buf,
			     long len)
{
  assert(blockLength_ > 0);
  assert(blocksPerWrite_ > 0);
  assert(mode == TrackData::AUDIO);
  int nwritten = 0;
  int writeLen = 0;
  unsigned char cmd[10];

  /*
  message(0, "lba: %ld, len: %ld, bpc: %d, bl: %d ", lba, len, blocksPerCmd,
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

    encode(lba, (unsigned char*)(buf + (nwritten * AUDIO_BLOCK_LEN)),
	   writeLen, encodeBuffer_);

#if 0
    // consistency checks
    long sum1, sum2;
    int i, n;
    const char *p;
    for (i = 0; i < writeLen; i++) {
      message(0, "%ld: ", lba + i);
      SubChannel *chan = subChannel_->makeSubChannel(encodeBuffer_ + i * blockLength_ + AUDIO_BLOCK_LEN);
      chan->print();
      delete chan;

      sum1 = 0;
      for (p = buf + (nwritten + i) * AUDIO_BLOCK_LEN, n = 0;
	   n < AUDIO_BLOCK_LEN; n++, p++) {
	sum1 += *p;
      }

      sum2 = 0;
      for (n = 0; n < AUDIO_BLOCK_LEN; n++) {
	sum2 += *(char *)(encodeBuffer_ + i * blockLength_ + n);
      }

      //message(0, "%ld - %ld", sum1, sum2);
      assert(sum1 == sum2);
    }
#endif

#if 1
    if (sendCmd(cmd, 10, encodeBuffer_, writeLen * blockLength_,
		NULL, 0) != 0) {
      message(-2, "Write data failed.");
      return 1;
    }
#endif
    //message(0, ". ");

    lba += writeLen;

    len -= writeLen;
    nwritten += writeLen;
  }

  //message(0, "");

  return 0;
}

