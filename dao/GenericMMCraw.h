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
 * $Log: GenericMMCraw.h,v $
 * Revision 1.1.1.1  2000/02/05 01:35:04  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.7  1999/04/05 11:04:10  mueller
 * Added driver option flags.
 *
 * Revision 1.6  1999/03/27 20:51:05  mueller
 * Adapted to changed writing interface.
 *
 * Revision 1.5  1998/10/03 15:07:53  mueller
 * Moved 'writeZeros()' to base class 'CdrDriver'.
 *
 * Revision 1.4  1998/09/27 19:19:18  mueller
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 * Added multi session mode.
 *
 * Revision 1.3  1998/09/22 19:15:13  mueller
 * Removed memory allocations during write process.
 *
 * Revision 1.2  1998/08/30 19:16:51  mueller
 * Added support for different sub-channel data formats.
 * Now 16 byte PQ sub-channel and 96 byte raw P-W sub-channel is
 * supported.
 *
 * Revision 1.1  1998/08/25 19:29:03  mueller
 * Initial revision
 *
 */

#ifndef __GENERIC_MMC_RAW_H__
#define __GENERIC_MMC_RAW_H__

#include "GenericMMC.h"
#include "PQChannelEncoder.h"

class GenericMMCraw : public GenericMMC, private PQChannelEncoder {
public:

  GenericMMCraw(ScsiIf *scsiIf, unsigned long options);
  ~GenericMMCraw();

  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  int multiSession(int);

  int initDao(const Toc *);
  int startDao();
  int finishDao();

  int writeData(TrackData::Mode, long &lba, const char *buf, long len);

protected:
  
  int setWriteParameters(int);

private:  
  unsigned char *encodeBuffer_; // buffer for encoding sub-channels

  SubChannel *subChannel_; // sub channel template

  long nextWritableAddress();
};

#endif
