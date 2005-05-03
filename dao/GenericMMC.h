/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002 Andreas Mueller <andreas@daneb.de>
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

#ifndef __GENERIC_MMC_H__
#define __GENERIC_MMC_H__

#include "CdrDriver.h"

class Toc;
class Track;
class CdTextEncoder;

#define OPT_MMC_USE_PQ       0x0001 // use PQ sub-channel data for scanning
#define OPT_MMC_PQ_BCD       0x0002 // PQ sub-channel contains BCD numbers
#define OPT_MMC_READ_ISRC    0x0004 // force reading of ISRC code with
                                    // READ SUB CHANNEL instead taking it from
                                    // the sub-channel data
#define OPT_MMC_SCAN_MCN     0x0008 // take MCN from the sub-channel data
                                    // instead using READ SUB CHANNEL
#define OPT_MMC_CD_TEXT      0x0010 // drive supports CD-TEXT writing
#define OPT_MMC_NO_SUBCHAN   0x0020 // drive does not support to read 
                                    // sub-channel data
#define OPT_MMC_NO_BURNPROOF 0x0040 // disable BURN-Proof
#define OPT_MMC_NO_RW_PACKED 0x0080 // drive does not support the packed R-W
                                    // sub-channel writing mode
#define OPT_MMC_USE_RAW_RW   0x0100 // use RW sub-channel data for scanning
#define OPT_MMC_YAMAHA_FORCE_SPEED 0x0200
                                    // drive supports Yamaha's Force Speed
                                    // feature

class GenericMMC : public CdrDriver {
public:

  GenericMMC(ScsiIf *scsiIf, unsigned long options);
  ~GenericMMC();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  unsigned long getReadCapabilities(const CdToc *, int) const;

  // MMC compatible drives take little endian samples
  int bigEndianSamples() const { return 0; }

  int checkToc(const Toc *);

  int speed(int);
  int speed();

  bool rspeed(int);
  int rspeed();

  DiskInfo *diskInfo();

  int loadUnload(int) const;

  int blankDisk(BlankingMode);

  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();
  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

  const DriveInfo *driveInfo(bool showErrorMsg);
  int subChannelEncodingMode(TrackData::SubChannelMode) const;

protected:
  int scsiTimeout_;
  Msf leadInStart_; // start of lead-in
  long leadInLen_;  // length of lead-in
  long leadOutLen_; // length if lead-out
  DiskInfo diskInfo_;
  DriveInfo *driveInfo_;

  CdTextEncoder *cdTextEncoder_;

  virtual int getSessionInfo();
  virtual int getNWA(long *);
  virtual int getStartOfSession(long *);

  virtual int getFeature(unsigned int feature, unsigned char *buf,
			 unsigned long bufLen, int showMsg);

  int readCatalog(Toc *, long startLba, long endLba);
  int readIsrc(int, char *);

  virtual int selectSpeed();
  virtual int setWriteParameters(unsigned long variant);
  int setSimulationMode(int showMessage);
  int performPowerCalibration();
  bool readBufferCapacity(long *capacity, long *available);

  unsigned char *createCueSheet(unsigned long variant, long *cueSheetLen);
  int sendCueSheet();
  unsigned char subChannelDataForm(TrackData::SubChannelMode,
				   int encodingMode);
  int writeCdTextLeadIn();

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);

  int getTrackIndex(long lba, int *trackNr, int *indexNr, unsigned char *ctl);

  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);

  TrackData::Mode getTrackMode(int, long trackStartLba);

  CdRawToc *getRawToc(int sessionNr, int *len);

  long readTrackData(TrackData::Mode, TrackData::SubChannelMode,
		     long lba, long len, unsigned char *buf);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

  int readCdTest(long lba, long len, int subChanMode) const;

  int checkDriveReady() const;

  int RicohGetWriteOptions();
  int RicohSetWriteOptions(const DriveInfo *);
};

#endif
