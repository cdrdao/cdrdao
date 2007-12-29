/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999-2001  Andreas Mueller <mueller@daneb.ping.de>
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

#include <unistd.h>
#include <assert.h>

#include "ToshibaReader.h"

#include "Toc.h"
#include "log.h"

ToshibaReader::ToshibaReader(ScsiIf *scsiIf, unsigned long options)
  : PlextorReader(scsiIf, options | OPT_DRV_GET_TOC_GENERIC)
{
  driverName_ = "Toshiba CD-ROM Reader - Version 0.1";
  
  audioDataByteOrder_ = 0;
  model_ = 0;
}

// static constructor
CdrDriver *ToshibaReader::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new ToshibaReader(scsiIf, options);
}

int ToshibaReader::readSubChannels(TrackData::SubChannelMode,
				   long lba, long len, SubChannel ***chans,
				   Sample *audioData)
{
  unsigned char cmd[10];
  int tries = 5;
  int ret;

  if (setBlockSize(AUDIO_BLOCK_LEN, 0x82) != 0)
    return 1;

  cmd[0] = 0x28; // READ10
  cmd[1] = 0x08; // force unit access
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = 0;
  cmd[7] = len >> 8;
  cmd[8] = len;
  cmd[9] = 0;

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

