/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#ifndef __CDRDRIVER_H__
#define __CDRDRIVER_H__

#include "ScsiIf.h"
#include "Msf.h"
#include "TrackData.h"
#include "SubChannel.h"
#include "remote.h"

class Toc;
class Track;

#define OPT_DRV_GET_TOC_GENERIC   0x00010000
#define OPT_DRV_SWAP_READ_SAMPLES 0x00020000
#define OPT_DRV_NO_PREGAP_READ    0x00040000
#define OPT_DRV_RAW_TOC_BCD       0x00080000
#define OPT_DRV_RAW_TOC_HEX       0x00100000
#define OPT_DRV_NO_CDTEXT_READ    0x00200000


// reading capabilities
#define CDR_READ_CAP_AUDIO_PW_RAW    0x001
#define CDR_READ_CAP_AUDIO_RW_COOKED 0x002
#define CDR_READ_CAP_AUDIO_RW_RAW    0x004
#define CDR_READ_CAP_AUDIO_PQ_BCD    0x008
#define CDR_READ_CAP_AUDIO_PQ_HEX    0x010
#define CDR_READ_CAP_DATA_PW_RAW     0x020
#define CDR_READ_CAP_DATA_RW_COOKED  0x040
#define CDR_READ_CAP_DATA_RW_RAW     0x080
#define CDR_READ_CAP_DATA_PQ_BCD     0x100
#define CDR_READ_CAP_DATA_PQ_HEX     0x200

#define CDR_AUDIO_SCAN_CAP (CDR_READ_CAP_AUDIO_PW_RAW|CDR_READ_CAP_AUDIO_PQ_BCD|CDR_READ_CAP_AUDIO_PQ_HEX)

struct DiskInfo {
  long capacity;          // recordable capacity of medium
  Msf  manufacturerId;    // disk identification
  int  recSpeedLow;       // lowest recording speed
  int  recSpeedHigh;      // highest recording speed

  int sessionCnt;         // number of closed sessions
  int lastTrackNr;        // number of last track on disk

  long lastSessionLba;    // start lba of first track of last closed session
  long thisSessionLba;    // start lba of this session

  int diskTocType;        // type of CD TOC, only valid if CD-R is not empty

  unsigned int empty  : 1; // 1 for empty disk, else 0
  unsigned int append : 1; // 1 if CD-R is appendable, else 0
  unsigned int cdrw   : 1; // 1 if disk is a CD-RW
  
  struct {
    unsigned int empty : 1;
    unsigned int append : 1;
    unsigned int cdrw : 1;
    unsigned int capacity : 1;
    unsigned int manufacturerId : 1;
    unsigned int recSpeed : 1;
  } valid;
};

struct DriveInfo {
  int maxReadSpeed;
  int currentReadSpeed;
  int maxWriteSpeed;
  int currentWriteSpeed;

  unsigned int accurateAudioStream : 1;
  unsigned int burnProof : 1;
  unsigned int ricohJustLink : 1;
  unsigned int ricohJustSpeed : 1;
};

struct CdTextPack {
  unsigned char packType;
  unsigned char trackNumber;
  unsigned char sequenceNumber;
  unsigned char blockCharacter;
  unsigned char data[12];
  unsigned char crc0;
  unsigned char crc1;
};

struct CdToc {
  int track;            // number
  long start;           // LBA of track start
  unsigned char adrCtl; // ADR/CTL field
};

struct CdRawToc {
  int sessionNr;
  int point;
  int min;
  int sec;
  int frame;
  int pmin;
  int psec;
  int pframe;
  unsigned char adrCtl;
};

struct TrackInfo {
  int trackNr;          // track number
  unsigned char ctl;    // flags
  TrackData::Mode mode; // track data mode
  long start;           // absolute start position from CD TOC
  long pregap;          // pre-gap length of track in blocks
  long fill;            // number of blocks to fill with zero data at end
  int indexCnt;         // number of index increments
  long index[98];       // index marks
  char isrcCode[13];    // ISRC code, valid if 'isrcCode[0] != 0'
  char *filename;       // data file name
  long bytesWritten;    // number of bytes written to file
};


class CdrDriver {
public:
  CdrDriver(ScsiIf *scsiIf, unsigned long options);
  virtual ~CdrDriver();

  // returns stored SCSI interface object
  virtual ScsiIf *scsiIf() const { return scsiIf_; }

  // sets SCSI interface object
  void scsiIf(ScsiIf *i) { scsiIf_ = i; }

  // returns name of driver
  virtual const char *driverName() const { return driverName_; }

  // returns options flags
  virtual unsigned long options() const { return options_; }

  // returns 1 if drive takes audio samples in big endian byte order or
  // 0 for little endian byte order
  virtual int bigEndianSamples() const = 0;

  // return information about drive
  virtual const DriveInfo *driveInfo(bool showErrorMsg) { return NULL; }

  // returns current writing speed
  virtual int speed() { return speed_; }
  
  // returns current reading speed
  virtual int rspeed() { return rspeed_; }

  // sets writing speed, returns 0 for OK or 1 for illegal speed,
  // this function may send SCSI commands to the drive
  virtual int speed(int) = 0;

  // sets reading speed, returns 0 for OK or 1 for illegal speed,
  // this function may send SCSI commands to the drive
  virtual bool rspeed(int);

  // sets/return buffer under run protection setting (if supported by
  // the drive: 1 = enabled, 0 = disbaled
  virtual int bufferUnderRunProtection() const {
    return enableBufferUnderRunProtection_;
  }

  virtual void bufferUnderRunProtection(int s) {
    enableBufferUnderRunProtection_ = s != 0 ? 1 : 0;
  }
    
  // sets/return writing speed control setting (if supported by
  // the drive: 1 = enabled, 0 = disbaled
  virtual int writeSpeedControl() const { return enableWriteSpeedControl_; }

  virtual void writeSpeedControl(int s) {
    enableWriteSpeedControl_ = s != 0 ? 1 : 0; }

  // returns 1 if simulation mode, 0 for real writing
  virtual bool simulate() const { return simulate_; }

  // sets simulation mode, returns 0 for OK, 1 if given mode is not supported
  virtual void simulate(bool s) { simulate_ = s; }

  // Sets multi session mode (0: close session, 1: open next session).
  // Returns 1 if multi session is not supported by driver, else 0
  virtual int multiSession(bool);

  // Returns mutli session mode.
  virtual bool multiSession() const { return multiSession_; }

  // Returns/sets fast toc reading flag (no sub-channel analysis)
  virtual bool fastTocReading() const { return fastTocReading_; }
  virtual void fastTocReading(bool f) { fastTocReading_ = f; }

  // Returns/sets raw data track reading flag
  virtual bool rawDataReading() const { return rawDataReading_; }
  virtual void rawDataReading(bool f) { rawDataReading_ = f; }

  // Returns/sets mode2 mixed track reading flag
  virtual bool mode2Mixed() const { return mode2Mixed_; }
  virtual void mode2Mixed(bool f) { mode2Mixed_ = f; }

  virtual TrackData::SubChannelMode subChanReadMode() const { return subChanReadMode_; }
  virtual void subChanReadMode(TrackData::SubChannelMode m) { subChanReadMode_ = m; }

  // Sets/returns the pad first pre-gap flag
  virtual int padFirstPregap() const { return padFirstPregap_; }
  virtual void padFirstPregap(int f) { padFirstPregap_ = f != 0 ? 1 : 0; }

  // Returns the on-thy-fly flag.
  virtual int onTheFly() const { return onTheFly_; }

  // Sets file descriptor for on the fly data and sets the on-the-fly flag
  // if 'fd' is >= 0 and clears it otherwise
  virtual void onTheFly(int fd);

  // Returns force flag
  virtual bool force() const { return force_; }

  // Sets force flag
  virtual void force(bool f) { force_ = f; }

  // Returns TAO source flag
  virtual bool taoSource() const { return taoSource_; }

  // Sets TAO source flag
  virtual void taoSource(bool f) { taoSource_ = f; }

  // Return number of adjust sectors for reading TAO source disks
  virtual int taoSourceAdjust() const { return taoSourceAdjust_; }

  // Sets number of adjust sectors for reading TAO source disks
  virtual void taoSourceAdjust(int val);

  // Sets remote mode
  virtual void remote(int flag, int fd);

  // Return remote mode flag
  virtual int remote() { return remote_; }
  
  // Sets cdda paranoia mode
  void paranoiaMode(int);
  
  // Sets user defined capacity
  virtual void userCapacity(int c) { userCapacity_ = c; }
  
  // Sets burning to the outer edge mode
  virtual void fullBurn(bool f) { fullBurn_ = f; }

  // Return byte order of host (0: little endian, 1: big endian)
  int hostByteOrder() const { return hostByteOrder_; }

  // general commands
  virtual int testUnitReady(int) const;

  virtual int startStopUnit(int) const;

  virtual int preventMediumRemoval(int) const;

  virtual int rezeroUnit(int showMessage = 1) const;

  virtual int loadUnload(int) const = 0;

  virtual int flushCache() const;

  virtual int readCapacity(long *length, int showMessage = 1);

  virtual bool readBufferCapacity(long* total, long* available) {
    return false;
  }

  // CD-RW specific commands
  
  enum BlankingMode { BLANK_FULL, BLANK_MINIMAL };
  virtual int blankDisk(BlankingMode);

  // disk at once recording related commands

  // Returns acceptable sub-channel encoding mode for given sub-channel type:
  //  -1: writing of sub-channel type not supported at all
  //   0: accepts plain data without encoding
  //   1: accepts only completely encoded data
  virtual int subChannelEncodingMode(TrackData::SubChannelMode) const;

  // Should check if toc is suitable for DAO writing with the actual driver.
  // Returns 0 if toc is OK, else 1.
  // Usually all tocs are suitable for writing so that the base class
  // implementation simply returns 0.
  virtual int checkToc(const Toc *);

  // Used to make necessary initializations but without touching the CD-R.
  // It should be possible to abort the writing process after this function
  // has been called without destroying the CD-R.
  virtual int initDao(const Toc *) = 0;

  // Performs all steps that must be done before the first user data block
  // is written, e.g. sending cue sheet, writing lead-in.
  virtual int startDao() = 0;

  // Performs all steps for successfully finishing the writing process,
  // e.g. writing lead-out, flushing the cache.
  virtual int finishDao() = 0;

  // Aborts writing process. Called if an error occurs or the user aborts
  // recording prematurely.
  virtual void abortDao() = 0;

  // Sends given data to drive. 'lba' should be the current writing address
  // and will be updated according to the written number of blocks.
  virtual int writeData(TrackData::Mode, TrackData::SubChannelMode sm,
			long &lba, const char *buf, long len);

  // returns mode for main channel data encoding, the value is used by
  // Track::readData()
  // 0: raw audio mode, all sectors must be encoded as audio sectors
  // 1: no encoding for MODE1 and MODE2 sectors, MODE2_FORM1 and MODE2_FORM2
  //    are extended by sub header and zero EDC/ECC data
  int encodingMode() const { return encodingMode_; }

  // disk read commands
  
  // analyzes the CD structure (Q sub-channels) of the inserted CD
  virtual Toc *readDiskToc(int session, const char *);

  // analyzes the CD structure and reads data
  virtual Toc *readDisk(int session, const char *);

  // returns information about inserted medium
  virtual DiskInfo *diskInfo() { return 0; }



  // Returns block size depending on given sector mode and 'encodingMode_'
  // that must be used to send data to the recorder.
  virtual long blockSize(TrackData::Mode, TrackData::SubChannelMode) const;

  // sends a status message to the driving application if in remote mode
  enum WriteCdProgressType { WCD_LEADIN = PGSMSG_WCD_LEADIN,
			     WCD_DATA = PGSMSG_WCD_DATA,
			     WCD_LEADOUT = PGSMSG_WCD_LEADOUT };
  int sendWriteCdProgressMsg(WriteCdProgressType type, int totalTracks,
			     int track, int trackProgress, int totalProgress,
			     int bufferFillRate, int writeBufferFill = 0);

  int sendBlankCdProgressMsg(int totalProgress);


  // static functions

  // Selects driver id for given vendor/model string. NULL is returned if
  // no driver could be selected.
  // readWrite: 0: select a driver for read operations
  //            1: select a driver for write operations
  // options: filled with option flags for vendor/model
  static const char *selectDriver(int readWrite, const char *vendor,
				  const char *model, unsigned long *options);

  // Creates instance of driver with specified id.
  static CdrDriver *createDriver(const char *driverId, unsigned long options,
				 ScsiIf *);

  // Try to autodetect a driver on given Scsi interface.
  static const char *detectDriver(ScsiIf *, unsigned long *options);

  // Prints list of all available driver ids.
  static void printDriverIds();

  // returns vendor/type of CD-R medium
  static int cdrVendor(Msf &, const char **vendor, const char** mediumType);

protected:
  struct ReadDiskInfo {
    int tracks;    // total number of tracks
    long startLba;      // LBA where extraction starts
    long endLba;        // LBA where extraction ends
  };

  unsigned long options_; // driver option flags
  ScsiIf *scsiIf_;
  int scsiMaxDataLen_;
  const char *driverName_;

  int hostByteOrder_; // 0: little endian, 1: big endian

  unsigned long readCapabilities_;

  int blockLength_; // length of data block for 'writeData' command
  long blocksPerWrite_; // number of blocks that can be written with a
                        // single SCSI WRITE command
  char *zeroBuffer_; // zeroed buffer for writing zeros

  int enableBufferUnderRunProtection_;
  int enableWriteSpeedControl_;
  int speed_;
  int rspeed_;
  bool simulate_;
  bool multiSession_;
  int encodingMode_; // mode for encoding data sectors
  bool fastTocReading_;
  bool rawDataReading_;
  int mode2Mixed_;
  TrackData::SubChannelMode subChanReadMode_;
  int padFirstPregap_; // used by 'read-toc': defines if the first audio 
                       // track's pre-gap is padded with zeros in the toc-file
                       // or if it is taken from the data file
  int onTheFly_; // 1 if operating in on-the-fly mode
  int onTheFlyFd_; // file descriptor for on the fly data
  bool force_; // force flag to allow certain operations
  int remote_; // 1 for remote mode, else 0
  int remoteFd_; // file descriptor for remote messages
  bool taoSource_; // 1 to indicate a TAO writting source CD for read-cd/read-toc
  int taoSourceAdjust_; // number of unreadable sectors between two tracks
                        // written in TAO mode
  const Toc *toc_;

  SubChannel **scannedSubChannels_;
  long maxScannedSubChannels_;

  unsigned char *transferBuffer_;

  // Byte order of audio samples read from the drive, e.g. with 
  // 'readSubChannels()'. 0: little endian, 1: big endian
  int audioDataByteOrder_; 
  
  int userCapacity_;
  bool fullBurn_;

  static unsigned char syncPattern[12];
  static char REMOTE_MSG_SYNC_[4];

  static int speed2Mult(int);
  static int mult2Speed(int);

  virtual int sendCmd(const unsigned char *cmd, int cmdLen,
		      const unsigned char *dataOut, int dataOutLen,
		      unsigned char *dataIn, int dataInLen,
		      int showErrorMsg = 1) const;

  virtual int getModePage(int pageCode, unsigned char *buf, long bufLen,
			  unsigned char *modePageHeader,
			  unsigned char *blockDesc, int showErrorMsg);
  virtual int setModePage(const unsigned char *buf,
			  const unsigned char *modePageHeader,
			  const unsigned char *blockDesc, int showErrorMsg);

  // some drives (e.g. Yamaha CDR100) don't implement mode sense/select(10)
  virtual int getModePage6(int pageCode, unsigned char *buf, long bufLen,
			   unsigned char *modePageHeader,
			   unsigned char *blockDesc, int showErrorMsg);
  virtual int setModePage6(const unsigned char *buf,
			   const unsigned char *modePageHeader,
			   const unsigned char *blockDesc, int showErrorMsg);

  virtual int writeZeros(TrackData::Mode, TrackData::SubChannelMode,
			 long &lba, long encLba, long count);


  // Returns track control flags for given track, bits 0-3 are always zero
  virtual unsigned char trackCtl(const Track *track);

  // Returns session format code for point A0 TOC entry, generated from
  // stored 'toc_' object.
  virtual unsigned char sessionFormat();

  // readToc related functions:

  // returns TOC data of specified session of inserted CD,
  // a generic function is implemented in 'CdrDriver.cc', it will return
  // the tracks of all session or of the first session depending on the
  // drive
  virtual CdToc *getToc(int sessionNr, int *nofTracks);

  // Generic function to retrieve basic TOC data. Cannot distinguish
  // between different sessions.
  CdToc *getTocGeneric(int *nofTracks);

  // Reads raw toc data of inserted CD. Used by base implementation of
  // 'getToc()' and must be implemented by the actual driver.
  virtual CdRawToc *getRawToc(int sessionNr, int *len) = 0;

  // Reads CD-TEXT packs from the lead-in of a CD. The base implementation
  // uses the SCSI-3/mmc commands.
  virtual CdTextPack *readCdTextPacks(long *);

  // reads CD-TEXT data and adds it to given 'Toc' object
  int readCdTextData(Toc *);

  // Tries to determine the data mode of specified track.
  virtual TrackData::Mode getTrackMode(int trackNr, long trackStartLba);

  // Determines mode of given sector, 'buf' should contain the sector header
  // at the first 4 bytes followed by the sub-header for XA tracks.
  // If an illegal mode is found in the sector header 'MODE0' will be
  // returned.
  TrackData::Mode determineSectorMode(unsigned char *buf);

  // analyzes given 8 byte sub header and returns wether the sector is
  // a MODE2, MODE2_FORM1 or MODE2_FORM2 sector
  TrackData::Mode analyzeSubHeader(unsigned char *);

  virtual unsigned long getReadCapabilities(const CdToc *, int) const = 0;

  // Called by 'readDiskToc()' to retrieve following information about
  // the track 'trackNr' with given start/end lba addresses:
  // - all index increments, filled into 'index'/'indexCnt'
  // - ISRC Code, filled into provided buffer 'isrcCode' (13 bytes)
  // - length of pre-gap of next track, filled into 'pregap'
  // - control nibbles read from track, filled into bits 0-3 of 'ctrl',
  //   bit 7 must be set to indicate valid data
  // This function must be overloaded by an actual driver.
  // return: 0: OK, 1: error occured
  virtual int analyzeTrack(TrackData::Mode, int trackNr, long startLba,
			   long endLba, Msf *index,
			   int *indexCnt, long *pregap, char *isrcCode,
			   unsigned char *ctl) = 0;

  // Track analysis algorithm using the binary search method. The base
  // class implements the basic algorithm. It uses 'findIndex()' which
  // can be implemented by an actual driver to get the track and index
  // number at a specific block address. This base class contains an
  // implementation of 'findIndex()', too, that can be usually used.
  // It'll be always better to use the linear scan algorithm (see below)
  // if possible.
  int analyzeTrackSearch(TrackData::Mode, int trackNr, long startLba,
			   long endLba, Msf *index,
			   int *indexCnt, long *pregap, char *isrcCode,
			   unsigned char *ctl);

  // finds position (lba) where index for given track number switches to
  // 'index' (binary search, base algorithm is implemented in 'CdrDriver').
  // It uses the method 'getTrackIndex()' which must be overloaded by
  // the actual driver.
  virtual long findIndex(int track, int index, long trackStart, long trackEnd);

  // Retrieves track, index and control nibbles at given lba address. Must
  // be implemented by the driver if the binary search method 
  // ('analyzeTrackSearch()') should be used.
  virtual int getTrackIndex(long lba, int *trackNr, int *indexNr, 
			    unsigned char *ctl);

  // Basic track analyzis using the linear scan algorithm. The base class
  // implements the basic algorithm which calls 'readSubChannels()' to
  // read the sub-channel data. Actual drivers should overload the
  // 'readSubChannels()' function.
  int analyzeTrackScan(TrackData::Mode, int trackNr, long startLba,
		       long endLba, Msf *index, int *indexCnt, long *pregap,
		       char *isrcCode, unsigned char *ctl);

  // Reads 'len' sub-channels from sectors starting  at 'lba'.
  // The returned vector contains 'len' pointers to 'SubChannel' objects.
  // Audio data that is usually retrieved with the sub-channels is placed
  // in 'buf' if it is not NULL.
  // Used by 'analyzeTrackScan()' and 'readAudioRangeParanoia()'.
  virtual int readSubChannels(TrackData::SubChannelMode, long lba, long len,
			      SubChannel ***, Sample *buf) = 0;

  // Determines the readable length of a data track and the pre-gap length
  // of the following track. The implementation in the base class should
  // be suitable for all drivers.
  virtual int analyzeDataTrack(TrackData::Mode mode, int trackNr,
			       long startLba, long endLba, long *pregap);

  // Reads 'len' data sectors starting at 'lba' and returns the number of
  // successfully read sectors. If the end of the current track is encountered
  // the returned value will be smaller than 'len' down to 0. If a read
  // error occus -1 is returned. If a L-EC error occures -2 is returned.
  // This method is used by 'readDataTrack'/'analyzeDataTrack' and must be
  // overloaded by the driver.
  virtual long readTrackData(TrackData::Mode, TrackData::SubChannelMode,
			     long lba, long len, unsigned char *buf) = 0;

  // Reads a complete data track and saves data to a file.
  virtual int readDataTrack(ReadDiskInfo *, int fp, long start, long end,
			    TrackInfo *trackInfo);

  // Reads the audio data of given audio track range 'startTrack', 'endTrack'.
  // 'trackInfo' is am array of TrackInfo structures for all tracks. 
  // This function is called by 'readDisk()' and must be overloaded by the
  // actual driver.
  virtual int readAudioRange(ReadDiskInfo *, int fp, long start, long end,
			     int startTrack, int endTrack, 
			     TrackInfo *) = 0;

  virtual int readAudioRangeStream(ReadDiskInfo *, int fd, long start,
				   long end, int startTrack, int endTrack, 
				   TrackInfo *);

  // Reads catalog number by scanning the sub-channels.
  // Uses 'readSubChannels()' to read the the sub-channels.
  int readCatalogScan(char *mcnCode, long startLba, long endLba);

  // Reads catalog number and stores it in given 'Toc' object. Must be
  // implemented by the actual driver. 'startLba' and 'endLba' specify
  // the allowed range for sub-channel scanning.
  virtual int readCatalog(Toc *toc, long startLba, long endLba) = 0;

  // Reads ISRC code and writes into provided 13 bytes buffer. Must be
  // implemented by the actual driver.
  virtual int readIsrc(int trackNr, char *) = 0;

  // Build Toc object from gathered TrackInfo data
  Toc *buildToc(TrackInfo *trackInfos, long nofTrackInfos, int padFirstPregap);

  // sets block size for read/write operations
  virtual int setBlockSize(long blocksize, unsigned char density = 0);

  // checks if drive capabilities support requested sub-channel reading mode
  int checkSubChanReadCaps(TrackData::Mode, unsigned long caps);

  void printCdToc(CdToc *toc, int tocLen);

  enum ReadCdProgressType { RCD_ANALYZING = PGSMSG_RCD_ANALYZING,
			    RCD_EXTRACTING = PGSMSG_RCD_EXTRACTING };
  void sendReadCdProgressMsg(ReadCdProgressType, int totalTracks, int track,
			     int trackProgress, int totalProgress);


public:
  // function to read audio data and also the sub-channel data from
  // specified lba,
  // this function is called from 'cdda_read()', so that it is currently
  // public because I did not manage to define a friend function that has
  // C linkage :)
  long audioRead(TrackData::SubChannelMode, int byteOrder,
		 Sample *buffer, long startLba, long len);


  // Interface for Monty's paranoia library:
protected:
  // Extracts audio data for given track range with the help of 
  // Monty's paranoia library.
  int readAudioRangeParanoia(ReadDiskInfo *, int fp, long start, long end,
			     int startTrack, int endTrack, 
			     TrackInfo *trackInfo);

private:
  // dynamic data
  void *paranoia_;                    // paranoia structure
  struct cdrom_drive *paranoiaDrive_; // paranoia device
  int paranoiaMode_;                  // paranoia mode
  ReadDiskInfo *audioReadInfo_;
  TrackInfo *audioReadTrackInfo_;
  int audioReadStartTrack_;
  int audioReadEndTrack_;
  long audioReadLastLba_;
  long audioReadActLba_;
  int audioReadActTrack_;
  int audioReadActIndex_;
  long audioReadCrcCount_;
  int audioReadError_;
  long audioReadProgress_;
  
  // callback for the paranoia library, does nothing, currently
  static void paranoiaCallback(long, int);


  // friend classes:
  friend class CDD2600Base;
};

#endif
