/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999-2001  Andreas Mueller <andreas@daneb.de>
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

#ifndef __SONY_CDU920_H__
#define __SONY_CDU920_H__

#include "CdrDriver.h"

class Toc;
class Track;

class SonyCDU920 : public CdrDriver {
public:

  SonyCDU920(ScsiIf *scsiIf, unsigned long options);
  ~SonyCDU920();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  unsigned long getReadCapabilities(const CdToc *, int) const { return 0; }

  int bigEndianSamples() const;

  int multiSession(int);
  int speed(int);

  int loadUnload(int) const;
  
  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();
  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

  DiskInfo *diskInfo();
  Toc *readDiskToc(int session, const char *audioFilename);

protected:
  int scsiTimeout_;
  Msf leadInStart_; // start of lead-in
  long leadInLen_;  // length of lead-in
  
  DiskInfo diskInfo_;

  int getSessionInfo();

  int readCatalog(Toc *, long startLba, long endLba);
  int readIsrc(int, char *);

  virtual int selectSpeed();        // overloaded by 'SonyCDU948'
  virtual int setWriteParameters(); // overloaded by 'SonyCDU948'

  unsigned char *createCueSheet(unsigned char leadInDataForm,
				long *cueSheetLen);
  int sendCueSheet(unsigned char leadInDataForm);

  int readBufferCapacity(long *capacity);

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);

  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);

  CdRawToc *getRawToc(int sessionNr, int *len);

  long readTrackData(TrackData::Mode, TrackData::SubChannelMode,
		     long lba, long len, unsigned char *buf);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

};

#endif
