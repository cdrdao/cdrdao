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
 * $Log: PlextorReaderScan.h,v $
 * Revision 1.1.1.1  2000/02/05 01:35:11  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.4  1999/04/05 18:49:22  mueller
 * Added driver options.
 * Added option to read Q sub-channel data instead of raw PW sub-channel
 * data for 'read-toc'.
 *
 * Revision 1.3  1999/03/27 20:56:39  mueller
 * Changed toc analysis interface.
 * Added support for toc analysis of data disks.
 *
 * Revision 1.2  1998/09/27 19:20:05  mueller
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 *
 * Revision 1.1  1998/09/07 15:15:40  mueller
 * Initial revision
 *
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

protected:

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);

  int readSubChannels(long lba, long len, SubChannel ***, Sample *);

  int readAudioRange(int fd, long start, long end, int startTrack,
		     int endTrack, TrackInfo *);

};

#endif
