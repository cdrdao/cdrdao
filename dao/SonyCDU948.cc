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

#include "SonyCDU948.h"
#include "PWSubChannel96.h"

#include "log.h"
#include "Toc.h"
#include "CdTextEncoder.h"

SonyCDU948::SonyCDU948(ScsiIf *scsiIf, unsigned long options)
  : SonyCDU920(scsiIf, options)
{
  driverName_ = "Sony CDU948 - Version 0.1 (data) (alpha)";

  speed_ = 4;

  cdTextEncoder_ = NULL;
}

SonyCDU948::~SonyCDU948()
{
  delete cdTextEncoder_;
  cdTextEncoder_ = NULL;
}

// static constructor
CdrDriver *SonyCDU948::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new SonyCDU948(scsiIf, options);
}

int SonyCDU948::checkToc(const Toc *toc)
{
  int err = SonyCDU920::checkToc(toc);
  int e;

  if ((e = toc->checkCdTextData()) > err)
    err = e;

  return err;
}

int SonyCDU948::multiSession(int m)
{
  multiSession_ = m != 0 ? 1 : 0;

  return 0;
}

// sets speed
// return: 0: OK
//         1: illegal speed
int SonyCDU948::speed(int s)
{
  if (s == 0 || s == 1 || s == 2 || s == 4)
    speed_ = s;
  else if (s == 3)
    speed_ = 2;
  else if (s > 4)
    speed_ = 4;
  else
    return 1;

  return 0;
}

// sets read/write speed and simulation mode
// return: 0: OK
//         1: scsi command failed
int SonyCDU948::selectSpeed()
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
  case 4:
    mp[2] = 3;
    break;
  }

  if (setModePage6(mp, NULL, NULL, 1) != 0) {
    log_message(-2, "Cannot set speed mode page.");
    return 1;
  }

  return 0;
}

// Sets write parameters.
// return: 0: OK
//         1: scsi command failed
int SonyCDU948::setWriteParameters()
{
  unsigned char cmd[10];
  unsigned char data[52];

  if (SonyCDU920::setWriteParameters() != 0)
    return 1;

  memset(cmd, 0, 10);
  memset(data, 0, 52);

  cmd[0] = 0xf8; // SET WRITE PARAMETERS
  cmd[7] = 0;
  cmd[8] = 52;

  data[1] = 50;
  data[3] = (multiSession_ != 0 ? (3 << 6) : 0);
  
  if (sendCmd(cmd, 10, data, 52, NULL, 0, 1) != 0) {
    log_message(-1, "Cannot set write parameters.");
    return 1;
  }

  return 0;
}

int SonyCDU948::initDao(const Toc *toc)
{
  long n;

  delete cdTextEncoder_;
  cdTextEncoder_ = new CdTextEncoder(toc);
  if (cdTextEncoder_->encode() != 0) {
    log_message(-2, "CD-TEXT encoding failed.");
    return 1;
  }

  if (cdTextEncoder_->getSubChannels(&n) == NULL || n == 0) {
    // there is no CD-TEXT data to write
    delete cdTextEncoder_;
    cdTextEncoder_ = NULL;
  }

  return SonyCDU920::initDao(toc);
}

int SonyCDU948::startDao()
{
  unsigned char leadInDataForm = 0x00; // CD-DA, generate data by device
  scsiTimeout_ = scsiIf_->timeout(3 * 60);

  if (cdTextEncoder_ != NULL)
    leadInDataForm = 0xc0; // CD-DA with P-W sub-channel data

  if (setWriteParameters() != 0 ||
      sendCueSheet(leadInDataForm) != 0)
    return 1;

  if (writeCdTextLeadIn() != 0) {
    return 1;
  }

  long lba = -150;

  // write mandatory pre-gap after lead-in
  if (writeZeros(toc_->leadInMode(), TrackData::SUBCHAN_NONE, lba, 0, 150)
      != 0) {
    return 1;
  }
  
  return 0;
}

int SonyCDU948::writeCdTextLeadIn()
{
  unsigned char cmd[10];
  const PWSubChannel96 **cdTextSubChannels;
  long cdTextSubChannelCount;
  long channelsPerCmd = scsiIf_->maxDataLen() / 96;
  long scp = 0;
  long byteLen;
  long len = leadInLen_;
  long n;
  long i;
  unsigned char *p;

  if (cdTextEncoder_ == NULL)
    return 0;

  if (leadInLen_ == 0) {
    log_message(-2, "Cannot write CD-TEXT lead-in because lead-in length is not known.");
    return 1;
  }

  cdTextSubChannels = cdTextEncoder_->getSubChannels(&cdTextSubChannelCount);

  assert(channelsPerCmd > 0);
  assert(cdTextSubChannels != NULL);
  assert(cdTextSubChannelCount > 0);

  log_message(2, "Writing CD-TEXT lead-in...");

  memset(cmd, 0, 10);
  cmd[0] = 0xe1; // WRITE CONTINUE

  while (len > 0) {
    n = (len > channelsPerCmd) ? channelsPerCmd : len;

    byteLen = n * 96;

    cmd[1] = (byteLen >> 16) & 0x1f;
    cmd[2] = byteLen >> 8;
    cmd[3] = byteLen;

    p = transferBuffer_;
    
    for (i = 0; i < n; i++) {
      memcpy(p, cdTextSubChannels[scp]->data(), 96);
      p += 96;

      scp++;
      if (scp >= cdTextSubChannelCount)
	scp = 0;
    }

    if (sendCmd(cmd, 10, transferBuffer_, byteLen, NULL, 0) != 0) {
      log_message(-2, "Writing of CD-TEXT data failed.");
      return 1;
    }

    len -= n;
  }
    
  return 0;
}
