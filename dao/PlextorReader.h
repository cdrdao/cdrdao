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
 * $Log: PlextorReader.h,v $
 * Revision 1.1  2000/02/05 01:35:06  llanero
 * Initial revision
 *
 * Revision 1.8  1999/04/05 18:48:37  mueller
 * Added driver options.
 * Added read-cd patch from Leon Woestenberg.
 *
 * Revision 1.7  1999/03/27 20:56:39  mueller
 * Changed toc analysis interface.
 * Added support for toc analysis of data disks.
 *
 * Revision 1.6  1998/09/27 19:20:05  mueller
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 *
 * Revision 1.5  1998/09/07 15:20:20  mueller
 * Reorganized read-toc related code.
 *
 * Revision 1.4  1998/08/30 19:25:43  mueller
 * Added function 'diskInfo()'.
 * Changed sub channel data field in 'readSubChannelData()' to be more
 * compatible to other drives.
 *
 * Revision 1.3  1998/08/25 19:26:07  mueller
 * Moved basic index extraction algorithm to class 'CdrDriver'.
 *
 */

#ifndef __PLEXTOR_READER_H__
#define __PLEXTOR_READER_H__

#include "CdrDriver.h"

#define OPT_PLEX_USE_PARANOIA   0x0001 // always use paranoia method for DAE
#define OPT_PLEX_DAE_READ10     0x0002 // use READ10 for DAE



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
  int readAudioRange(int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *trackInfo);


private:
  void playAudioBlock(long start, long len);
  int readSubChannelData(int *trackNr, int *indexNr, long *,
			 unsigned char *ctl);

  int readAudioRangePlextor(int fd,  long start, long end, int startTrack,
			    int endTrack, TrackInfo *);

};

#endif
