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

#ifndef __PLEXTOR_READER_H__
#define __PLEXTOR_READER_H__

#include "CdrDriver.h"

#define OPT_PLEX_USE_PARANOIA   0x0001 // always use paranoia method for DAE
#define OPT_PLEX_DAE_READ10     0x0002 // use READ10 for DAE
#define OPT_PLEX_DAE_D4_12      0x0004 // use 12 byte command 0xD4 for DAE


class Toc;
class Track;

class PlextorReader : public CdrDriver {
public:

  PlextorReader(ScsiIf *scsiIf, unsigned long options);
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  // not used for readers
  int bigEndianSamples() const { return 0;}

  int speed(int);

  int loadUnload(int) const { return 0; }

  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  DiskInfo *diskInfo();

  Toc *readDiskToc(int, const char *);
  Toc *readDisk(int, const char *);

protected:
  DiskInfo diskInfo_;
  int model_;

  CdRawToc *getRawToc(int sessionNr, int *len);

  int getTrackIndex(long lba, int *trackNr, int *indexNr, 
		    unsigned char *ctl);
  int readCatalog(Toc *, long startLba, long endLba);
  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);
  int readIsrc(int trackNr, char *);

  long readTrackData(TrackData::Mode mode, long lba, long len,
		     unsigned char *buf);
  int readSubChannels(long lba, long len, SubChannel ***, Sample *);
  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *trackInfo);


private:
  void playAudioBlock(long start, long len);
  int readSubChannelData(int *trackNr, int *indexNr, long *,
			 unsigned char *ctl);

  int readAudioRangePlextor(ReadDiskInfo *, int fd,  long start, long end,
			    int startTrack, int endTrack, TrackInfo *);

};

#endif
