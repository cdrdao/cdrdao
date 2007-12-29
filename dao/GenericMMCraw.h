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

#ifndef __GENERIC_MMC_RAW_H__
#define __GENERIC_MMC_RAW_H__

#include "GenericMMC.h"
#include "PQChannelEncoder.h"
#include "PWSubChannel96.h"

class GenericMMCraw : public GenericMMC, private PQChannelEncoder {
public:

  GenericMMCraw(ScsiIf *scsiIf, unsigned long options);
  ~GenericMMCraw();

  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  int multiSession(bool);

  int subChannelEncodingMode(TrackData::SubChannelMode) const;

  int initDao(const Toc *);
  int startDao();
  int finishDao();

  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

protected:
  
  int setWriteParameters(int);

private:  
  unsigned char *encodeBuffer_; // buffer for encoding sub-channels
  unsigned char *encSubChannel_; // holds encoded sub-channels

  SubChannel *subChannel_; // sub channel template

  int subChannelMode_; /* selected sub-channel writing mode:
			  0: undefined
			  1: 16 byte PQ
			  2: 96 byte PW packed
			  3: 96 byte PW raw
		       */
  
  long cdTextStartLba_;
  long cdTextEndLba_;
  const PWSubChannel96 **cdTextSubChannels_;
  long cdTextSubChannelCount_;
  long cdTextSubChannelAct_;

  long nextWritableAddress();
  int getMultiSessionInfo(int sessionNr, int multi, SessionInfo *info);
  int getSubChannelModeFromToc();
  int setSubChannelMode();
};

#endif
