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
 * $Log: CDD2600.h,v $
 * Revision 1.1  2000/02/05 01:34:48  llanero
 * Initial revision
 *
 * Revision 1.12  1999/04/05 11:04:10  mueller
 * Added driver option flags.
 *
 * Revision 1.11  1999/03/27 20:52:02  mueller
 * Adapted to changed writing interface.
 *
 * Revision 1.10  1998/10/24 14:30:40  mueller
 * Changed prototype of 'readDiskToc()'.
 *
 * Revision 1.9  1998/10/03 15:11:05  mueller
 * Moved basic writing methods to class 'CDD2600Base'.
 *
 * Revision 1.8  1998/09/27 19:17:46  mueller
 * Fixed 'disc-info' for CDD2000.
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 *
 * Revision 1.7  1998/09/22 19:15:13  mueller
 * Removed memory allocations during write process.
 *
 * Revision 1.6  1998/09/07 15:20:20  mueller
 * Reorganized read-toc related code.
 *
 * Revision 1.5  1998/08/25 19:22:42  mueller
 * Added index extraction for 'read-toc'.
 * Added disk info code.
 *
 * Revision 1.4  1998/08/07 12:44:30  mueller
 * Changed group 1 mode select commands to group 0 mode select commands.
 * Implement pre-gap detection in 'readDiscToc()'.
 *
 */

#ifndef __CDD2600_H__
#define __CDD2600_H__

#include "CdrDriver.h"
#include "CDD2600Base.h"
class Toc;
class Track;

class CDD2600 : public CdrDriver, private CDD2600Base {
public:

  CDD2600(ScsiIf *scsiIf, unsigned long options);
  ~CDD2600();

  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  // CDD2600 takes big endian samples
  int bigEndianSamples() const { return 1; }

  int speed(int);

  DiskInfo *diskInfo();

  int loadUnload(int) const;
  
  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  int writeData(TrackData::Mode, long &lba, const char *buf, long len);

  Toc *readDiskToc(int, const char *);
  Toc *readDisk(int, const char *);

protected:
  DiskInfo diskInfo_;

  CdRawToc *getRawToc(int sessionNr, int *len);
  int readCatalog(Toc *, long startLba, long endLba);
  int readIsrc(int, char *);
  int getTrackIndex(long lba, int *trackNr, int *indexNr, unsigned char *ctl);
  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);
  int readSubChannels(long lba, long len, SubChannel ***, Sample *);


private:
  long leadInLength_;
  long leadOutLength_;

  int modeSelectPlay(int immediate, int sotc, unsigned char volume);

  int readSubChannelData(int *trackNr, int *indexNr, long *,
			 unsigned char *ctl);

  void readBlock(unsigned long sector);

  long readTrackData(TrackData::Mode mode, long lba, long len,
		     unsigned char *buf);

  int readAudioRange(int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);



};

#endif
