/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999  Cameron G. MacKinnon <C_MacKinnon@yahoo.com>
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: YamahaCDR10x.h,v $
 * Revision 1.2  2000/10/08 16:39:41  andreasm
 * Remote progress message now always contain the track relative and total
 * progress and the total number of processed tracks.
 *
 * Revision 1.1.1.1  2000/02/05 01:35:15  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.3  1999/04/05 11:04:10  mueller
 * Added driver option flags.
 *
 * Revision 1.2  1999/03/27 14:35:17  mueller
 * Added data track support.
 *
 * Revision 1.1  1999/02/28 10:27:49  mueller
 * Initial revision
 *
 */

/* Driver for Yamaha CDR10X drives. 
 * Written by Cameron G. MacKinnon <C_MacKinnon@yahoo.com>.
 */

#ifndef __YAMAHACDR10X_H__
#define __YAMAHACDR10X_H__

#include "CdrDriver.h"
#include "TrackData.h"

class Toc;
class Track;

class YamahaCDR10x : public CdrDriver {
public:

  YamahaCDR10x(ScsiIf *scsiIf, unsigned long options);
  ~YamahaCDR10x();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  // FIXME: We say we're little endian, incurring work for the host
  // The drive has endian flags, but I don't want to experiment
  int bigEndianSamples() const { return 0; }

  int multiSession(int);
  int speed(int);
  int speed();

  DiskInfo *diskInfo();

  Toc *readDiskToc(int, const char *audioFilename);
  Toc *readDisk(int, const char *audioFilename);

  int loadUnload(int) const;
  
  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  int driveInfo(DriveInfo *, int showErrorMsg);
  virtual int writeData(TrackData::Mode, long &lba, const char *buf, long len);

protected:
  int scsiTimeout_;
  unsigned char audioModePage_[16]; // saved audio mode page
  Msf leadInStart_; // start of lead-in
  long leadInLen_;  // length of lead-in
  long leadOutLen_; // length if lead-out


  CdRawToc *getRawToc(int sessionNr, int *len);

  int readCatalog(Toc *, long startLba, long endLba);
  int readIsrc(int, char *);
  int readSubChannels(long lba, long len, SubChannel ***, Sample *);

  virtual int selectSpeed();
  virtual int setWriteParameters();

  void cueSheetDataType(TrackData::Mode mode, unsigned char *dataType,
			unsigned char *dataForm);
  unsigned char *createCueSheet(long *cueSheetLen);
  int sendCueSheet();

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index, int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);
  long readTrackData(TrackData::Mode mode, long lba, long len,
		     unsigned char *buf);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

};

#endif
