/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

#ifndef __TEAC_CDR55_H__
#define __TEAC_CDR55_H__

#include "CdrDriver.h"

class Toc;
class Track;

class TeacCdr55 : public CdrDriver {
public:

  TeacCdr55(ScsiIf *scsiIf, unsigned long options);
  ~TeacCdr55();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  unsigned long getReadCapabilities(const CdToc *, int) const { return 0; }

  // MMC compatible drives take little endian samples
  int bigEndianSamples() const { return 0; }

  int speed(int);

  DiskInfo *diskInfo();
  int driveInfo(DriveInfo *, bool showErrorMsg);

  Toc *readDiskToc(int, const char *audioFilename);
  Toc *readDisk(int, const char *audioFilename);

  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

  int loadUnload(int) const;

protected:
  DiskInfo diskInfo_;

  unsigned char modeSelectData_[12];
  TrackData::Mode actMode_; // actual writing mode, used to switch modes
                            // in 'writeData()'
  long writeEndLba_; // LBA at which writing ends, used in 'writeData()'

  int scsiTimeout_;

  int getModeSelectData();
  int setWriteSpeed();
  int setWriteParameters();
  int setSimulationMode();
  int setWriteDensity(TrackData::Mode);
  int executeOPC(int judge);
  int clearSubcode();
  int setSubcode(long start, long end, unsigned char ctl, int trackNr,
		 int indexNr, int pflag);
  int setSubcodes(const Track *track, int trackNr, long start, long end,
		  long nstart);
  int setSubcodes();
  int setToc();
  int setCatalog();
  int setIsrc();


  CdRawToc *getRawToc(int sessionNr, int *len);
  int analyzeTrack(TrackData::Mode mode, int trackNr, long startLba,
		   long endLba, Msf *indexIncrements, int *indexIncrementCnt,
		   long *pregap, char *isrcCode, unsigned char *ctl);

  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);
  long readTrackData(TrackData::Mode, TrackData::SubChannelMode,
		     long lba, long len, unsigned char *buf);

  int readIsrc(int trackNr, char *buf);
  int readCatalog(class Toc *, long startLba, long endLba);
  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

};

#endif
