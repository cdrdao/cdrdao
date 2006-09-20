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

/*! \def OPT_PLEX_NOSLOW_ON_ERR
    \brief Don't slow down read speed if any read error encountered
    
    By default is activated, so the unit slows down. Passing
    this option will disable it; unit won't slow down. Driver checks
    previous status and restores it.
    Mode page 0x31
    byte 3 [ x x x x x x x re ] 1 means don't slow down, 0 default
*/
#define OPT_PLEX_NOSLOW_ON_ERR    0x0010

/*! \def OPT_PLEX_TRANSF_BEF_MAX
    \brief Start to transfer data before max speed reached
    
    By default is disabled, so unit reaches max speed before beginning
    transfer. Passing this options will enable it; unit will transfer
    data before reaching max speed. Driver checks previous status and
    restores it.
    Mode page 0x31
    byte 3 [ x x x x x x td x ] 1 means don't wait for max speed, 0 default
*/
#define OPT_PLEX_TRANSF_BEF_MAX 0x0020

/*! \def OPT_PLEX_NOSLOW_ON_VIB
    \brief Don't slowdown to avoid vibrations
    
    By default is activated, so unit slows down. Passing this option
    will disable it; unit won't slow down. Driver checks previous status
    and restores it.
    Mode page 0x31
    byte 3 [ x x x x x sl x x ] 1 means don't slow down, 0 default
*/
#define OPT_PLEX_NOSLOW_ON_VIB    0x0040

class Toc;
class Track;

class PlextorReader : public CdrDriver {
public:

  PlextorReader(ScsiIf *scsiIf, unsigned long options);

  /*! 
   * Its only purpose is to reset Plextor special features to their value
   * before cdrdao initialization.
   */
  virtual ~PlextorReader();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  unsigned long getReadCapabilities(const CdToc *, int) const { return 0; }

  // not used for readers
  int bigEndianSamples() const { return 0;}

  int speed(int);

  int loadUnload(int) const { return 0; }

  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();
  
  inline int ReadErrorsSlowDown () {return slow_down_on_read_errors;}
  inline int VibrationsSlowDown () {return slow_down_on_vibrations;}
  inline int WaitMaxSpeed () {return transfer_data_before_max_speed;}
  /*! \brief Controls wheter drive slows down when a read error occurs
  
    Special plextor feature;
    \param slowdown 0 to disable slowdown, 1 to enable (default)
    \return 1 on success, -1 on error
  */
  int ReadErrorsSlowDown (int slowdown);
  /*! \brief Controls wheter drive slows down when paused to avoid
      vibrations
      
      Special plextor feature, available on PX-20 and later
      \param slowdown 0 to disable slowdown, 1 to enable (default)
      \return 1 on success, -1 on error
  */
  int VibrationsSlowDown (int slowdown);
  /*! \brief Controls wheter drive waits for max speed before transferring data
  
      Special plextor feature, available on PX-20 and later
      \param wait 0 to transfer before maximum speed, 1 to wait (default)
      \return 1 on success, -1 on error
  */
  int WaitMaxSpeed (int wait);

  DiskInfo *diskInfo();

  Toc *readDiskToc(int, const char *);
  Toc *readDisk(int, const char *);

protected:
  DiskInfo diskInfo_;
  /*! \brief Drive model, as index of following 
      
      { 1,"CD-ROM PX-4XCH" }, { 2,"CD-ROM PX-4XCS" },
      { 3,"CD-ROM PX-4XCE" }, { 4,"CD-ROM PX-6X" },
      { 5,"CD-ROM PX-8X" }, { 6,"CD-ROM PX-12" },
      { 7,"CD-ROM PX-20" }, { 8,"CD-ROM PX-32" },
      { 9,"CD-ROM PX-40" }
  */
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

  long readTrackData(TrackData::Mode mode, TrackData::SubChannelMode,
		     long lba, long len, unsigned char *buf);
  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);
  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *trackInfo);


private:
  void playAudioBlock(long start, long len);
  int readSubChannelData(int *trackNr, int *indexNr, long *,
			 unsigned char *ctl);

  int readAudioRangePlextor(ReadDiskInfo *, int fd,  long start, long end,
			    int startTrack, int endTrack, TrackInfo *);
    /* These can be -1 if not available, 0 or 1 */
    int slow_down_on_read_errors;
    int transfer_data_before_max_speed;
    int slow_down_on_vibrations;
    /* Original status of plextor special features */
    unsigned char orig_byte3;
};

#endif
