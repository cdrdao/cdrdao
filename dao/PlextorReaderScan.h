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

#ifndef __PLEXTOR_READER_SCAN_H__
#define __PLEXTOR_READER_SCAN_H__

#include "PlextorReader.h"

#define OPT_PLEX_USE_PQ       0x0001 // use PQ sub-channel data for scanning
#define OPT_PLEX_PQ_BCD       0x0002 // PQ sub-channel contains BCD numbers
#define OPT_PLEX_READ_ISRC    0x0004 // force reading of ISRC code with
                                     // READ SUB CHANNEL instead taking it from
                                     // the sub-channel data

class PlextorReaderScan : public PlextorReader {
public:

  PlextorReaderScan(ScsiIf *scsiIf, unsigned long options);
  ~PlextorReaderScan();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  Toc *readDisk(int session, const char *);

  unsigned long getReadCapabilites(const CdToc *, int) const { return 0; }

protected:

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);

  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

};

#endif
