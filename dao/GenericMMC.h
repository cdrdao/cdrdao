/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
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
 * $Log: GenericMMC.h,v $
 * Revision 1.3  2000/10/08 16:39:40  andreasm
 * Remote progress message now always contain the track relative and total
 * progress and the total number of processed tracks.
 *
 * Revision 1.2  2000/06/06 22:26:13  andreasm
 * Updated list of supported drives.
 * Added saving of some command line settings to $HOME/.cdrdao.
 * Added test for multi session support in raw writing mode to GenericMMC.cc.
 * Updated manual page.
 *
 * Revision 1.1.1.1  2000/02/05 01:35:04  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.8  1999/04/05 18:47:40  mueller
 * Added driver options.
 * Added option to read Q sub-channel data instead raw PW sub-channel data
 * for 'read-toc'.
 *
 * Revision 1.7  1999/03/27 20:51:26  mueller
 * Added data track support.
 *
 * Revision 1.6  1998/10/24 14:48:55  mueller
 * Added retrieval of next writable address. The Panasonic CW-7502 needs it.
 *
 * Revision 1.5  1998/10/03 15:08:44  mueller
 * Moved 'writeZeros()' to base class 'CdrDriver'.
 * Takes 'writeData()' from base class now.
 *
 * Revision 1.4  1998/09/27 19:18:48  mueller
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 * Added multi session mode.
 *
 * Revision 1.3  1998/09/07 15:20:20  mueller
 * Reorganized read-toc related code.
 *
 * Revision 1.2  1998/08/30 19:17:56  mueller
 * Fixed cue sheet generation and first writable address after testing
 * with a Yamaha CDR400t.
 *
 * Revision 1.1  1998/08/15 20:45:26  mueller
 * Initial revision
 *
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

class GenericMMC : public CdrDriver {
public:

  GenericMMC(ScsiIf *scsiIf, unsigned long options);
  ~GenericMMC();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  // MMC compatible drives take little endian samples
  int bigEndianSamples() const { return 0; }

  int checkToc(const Toc *);

  int speed(int);
  int speed();

  DiskInfo *diskInfo();

  int loadUnload(int) const;

  int blankDisk();

  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();
  int writeData(TrackData::Mode mode, long &lba, const char *buf, long len);

  int driveInfo(DriveInfo *, int showErrorMsg);

protected:
  int scsiTimeout_;
  Msf leadInStart_; // start of lead-in
  long leadInLen_;  // length of lead-in
  long leadOutLen_; // length if lead-out
  DiskInfo diskInfo_;

  CdTextEncoder *cdTextEncoder_;

  virtual int getSessionInfo();
  virtual int getNWA(long *);
  virtual int getStartOfSession(long *);

  virtual int getFeature(unsigned int feature, unsigned char *buf,
			 unsigned long bufLen, int showMsg);

  int readCatalog(Toc *, long startLba, long endLba);
  int readIsrc(int, char *);

  virtual int selectSpeed(int readSpeed);
  virtual int setWriteParameters();
  int performPowerCalibration();
  int readBufferCapacity(long *capacity);

  unsigned char *createCueSheet(long *cueSheetLen);
  int sendCueSheet();

  int writeCdTextLeadIn();

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);

  int getTrackIndex(long lba, int *trackNr, int *indexNr, unsigned char *ctl);

  int readSubChannels(long lba, long len, SubChannel ***, Sample *);

  TrackData::Mode getTrackMode(int, long trackStartLba);

  CdRawToc *getRawToc(int sessionNr, int *len);

  long readTrackData(TrackData::Mode mode, long lba, long len,
		     unsigned char *buf);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

};

#endif
