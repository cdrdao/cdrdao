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

  unsigned long getReadCapabilities(const CdToc *, int) const { return 0; }

  // CDD2600 takes big endian samples
  int bigEndianSamples() const { return 1; }

  int speed(int);

  DiskInfo *diskInfo();

  int loadUnload(int) const;
  
  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

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
  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);


private:
  long leadInLength_;
  long leadOutLength_;

  int modeSelectPlay(int immediate, int sotc, unsigned char volume);

  int readSubChannelData(int *trackNr, int *indexNr, long *,
			 unsigned char *ctl);

  void readBlock(unsigned long sector);

  long readTrackData(TrackData::Mode, TrackData::SubChannelMode, long lba,
		     long len, unsigned char *buf);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

  int nextWritableAddress(long *lba, int showError);


};

#endif
