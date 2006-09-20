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

/*! \def OPT_MMC_USE_PQ
    \brief use PQ sub-channel data for scanning
*/
#define OPT_MMC_USE_PQ       0x0001
/*! \def OPT_MMC_PQ_BCD
    \brief PQ sub-channel contains BCD numbers
*/
#define OPT_MMC_PQ_BCD       0x0002
/*! \def OPT_MMC_READ_ISRC
    \brief force reading of ISRC code with
    READ SUB CHANNEL instead taking it from
    the sub-channel data
*/
#define OPT_MMC_READ_ISRC    0x0004
/*! \def OPT_MMC_SCAN_MCN
    \brief take MCN from the sub-channel data
    instead using READ SUB CHANNEL
*/
#define OPT_MMC_SCAN_MCN     0x0008
/*! \def OPT_MMC_CD_TEXT
    \brief drive supports CD-TEXT writing
*/
#define OPT_MMC_CD_TEXT      0x0010
/*! \def OPT_MMC_NO_SUBCHAN
    \brief drive does not support
    reading sub-channel data
*/
#define OPT_MMC_NO_SUBCHAN   0x0020
/*! \def OPT_MMC_NO_BURNPROOF
    \brief disable BURN-Proof
*/
#define OPT_MMC_NO_BURNPROOF 0x0040
/*! \def OPT_MMC_NO_RW_PACKED
    \brief drive does not support the packed R-W
    sub-channel writing mode
*/
#define OPT_MMC_NO_RW_PACKED 0x0080
/*! \def OPT_MMC_USE_RAW_RW
    \brief use RW sub-channel data for scanning
*/
#define OPT_MMC_USE_RAW_RW   0x0100

#define OPT_MMC_YAMAHA_FORCE_SPEED 0x0200
                                    // drive supports Yamaha's Force Speed
                                    // feature

/*! \brief Driver for Generic MMC units
*/
class GenericMMC : public CdrDriver {
public:
/*! \brief Default constructor
    \param scsiIf As obtained from ScsiIf::ScsiIf
    \param options Binary AND of options like OPT_MMC_USE_PQ and subsequent
    \sa OPT_MMC_USE_PQ
*/
  GenericMMC(ScsiIf *scsiIf, unsigned long options);
/*! \brief Default destructor */
  ~GenericMMC();
/*! \brief Static constructor
    \sa GenericMMC::GenericMMC
*/
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

/*! \brief See what subchannel reading modes are available

    For each data/audio track it tries PQ subchannel (using GenericMMC::readCdTest);
    it may result in:
        - not supported
        - BCD format
        - HEX format
        - unknown (error printout)

    Then it tries
        - Raw PW
        - Cooked PW

    You may skip some tests if you have initialized the driver with something
    like OPT_MMC_USE_PQ.
    \param CdToc* Toc of the disc, used to compute track lengths
    \param int Number of tracks to use for the test
    \return Binary OR of the capabilities.
    \sa CDR_READ_CAP_DATA_PW_RAW and similar.
*/
  unsigned long getReadCapabilities(const CdToc *, int) const;

  /*! \brief MMC compatible drives take little endian samples
      \return 0
  */
  int bigEndianSamples() const { return 0; }

  /*! \brief Sanity checking of the TOC

    A first check is done using CdrDriver::checkToc.
    Then, if CD TEXT capabilities are available, it checks CD TEXT
    using Toc::checkCdTextData.

    \return 0 if OK, 1 if some warning, 2 on error
  */
  int checkToc(const Toc *);

  /*! \brief Sets read/write speed

    Uses SET CD SPEED command 0xBB.
    Speeds are specified as multipliers, using 0 as means max value.
    Write speed is set from protected variable GenericMMC::speed_.
    \param int Read speed as multiplier.
    \return 0 on success, 1 if SCSI command error
  */
  int speed(int);

  /*! \brief Returns current write speed

    Rebuilds the GenericMMC::driveInfo_ structure by reading
    mode page 0x2A. Information is extracted from this structure.
    \return Write speed (as multiplier) or 0 if SCSI error occurred.
  */
  int speed();

  bool rspeed(int);
  int rspeed();

  /*! \brief Gathers various info about inserted disk.

    - Issues READ DISK INFORMATION 0x51. Obtains number of tracks and
        sessions and disk completion status.

    - Issues READ TOC 0x43 with parameter 1 to get session info.

    - Reads ATIP data (READ TOC 0x43, parameter 4)
    \return Pointer to protected attribute GenericMMC::diskInfo_
  */
  DiskInfo *diskInfo();

  /*! \brief Loads or ejects tray

    Uses START/STOP unit command 0x1B.
    \param int 0 to load tray, 1 to eject
    \return 0 on success, 1 on error
  */
  int loadUnload(int) const;

  /*! \brief Blanks a CDRW

    First the drive is put in simulation or write mode, as specified in
    GenericMMC::simulate_ attribute. Then the disk is blanked with BLANK 0xA1.
    The IMMED bit is not set, and the call will
    block until the operation is completed. Drive status will be polled
    every 2 seconds.
    \param BlankingMode Either BLANK_FULL or BLANK_MINIMAL
    \return 0 on success or 1 on error
  */
  int blankDisk(BlankingMode);

  /*! \brief Initializes internal stuff before a write session.

    - Setup CD-TEXT encoder if specified in GenericMMC::options_

    - Gets inserted disk info with GenericMMC::diskInfo

    - Sets write speed with GenericMMC::selectSpeed

    - Allocates new data buffer into GenericMMC::zeroBuffer_

    \param Toc* TOC of the disk you want to write
    \return 0 on success, 1 on error
  */
  int initDao(const Toc *);

  /*! \brief Begins write process

    Sets write parameters (MP 0x05), performs power calibration if not
    in simulation mode, sends cue sheet and writes leadin. The session LBA
    is always -150. First 150 sectors are zeroed.
    \return 0 on success, 1 on error
  */
  int startDao();

  /*! \brief Finishes a writing

    Flush device cache and wait for device to become ready. Since some drives
    do not return to ready after writing a leadout the cache flush is done twice,
    before and after the wait unit ready.
    Then the CD-TEXT encoder and the zero buffer are deleted.
    \return 0 on success, 1 on error
  */
  int finishDao();

  /*! \brief Aborts a writing

    Flush device cache and delete CD-TEXT encoder.
  */
  void abortDao();

  /*! \brief Writes data to target

    The block length depends on the actual writing. Writing is done using
    WRITE10 (0x2A) command. Command is retried if device buffer is full.
    \param TrackData::Mode Main channel writing mode
    \param TrackData::SubChannelMode Subchannel writing mode
    \param lba specifies the next logical block address for writing and is updated
        by this function.
    \param buf Data to be written
    \param len Number of blocks to write
    \return 0 if OK, 1 if WRITE command failed
  */
  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

  /*! \brief Gets useful parameters about the device.

    Gets mode page 0x2A and info about
    - Burnproof
    - Accurate audio stream
    - Maximum read speed
    - Current read speed
    - Maximum write speed
    - Maximum read speed
    \param showErrorMsg If true print out errors
    \return Pointer to the protected variable driveInfo_
  */
  const DriveInfo *driveInfo(bool showErrorMsg);

    /*!
    Returns 0 or 1 depending on the SubChannelMode parameter.
    - 0 if SUBCHAN_NONE
    - 0 if SUBCHAN_RW
    - 1 if SUBCHAN_RW_RAW
    \param SubChannelMode One of TrackData::SubChannelMode
    \return 0 or 1 (see above)
  */
  int subChannelEncodingMode(TrackData::SubChannelMode) const;

protected:
  /*! \brief Timeout for commands on the target */
  int scsiTimeout_;
  /*! \brief  Start of lead-in */
  Msf leadInStart_;
  /*! \brief Length of lead-in */
  long leadInLen_;
  /*! \brief Length if lead-out */
  long leadOutLen_;
  /*! \brief Various info about the inserted media
      \sa diskInfo
  */
  DiskInfo diskInfo_;
  /*! \brief Various info about the device
      \sa driveInfo
  */
  DriveInfo *driveInfo_;
  /*! Called by GenericMMC::initDao if needed. */
  CdTextEncoder *cdTextEncoder_;

  /*! \brief Determines start and length of lead-in and length of lead-out.

    Uses READ DISK INFORMATION cmd 0x51.
    \return 0 on success, 1 on SCSI error
  */
  virtual int getSessionInfo();
  /*! \brief Gets next writable address

    Uses READ TRACK INFORMATION 0x52 on incomplete track 0xFF. Must return
    RT=1 and NWA_V=1. LBA of next writable address is taken from bytes 12-15.
    \param long* LBA of next writable address.
    \return 0 on success, 1 on SCSI error
  */
  virtual int getNWA(long *);
  /*! \brief Determines first writable address of data area of current empty session.

    Places the device in Session At Once write type and calls GenericMMC::getNWA
    \param long* LBA of the beginning of data area
    \return 0 on success, 1 on SCSI error.
  */
  virtual int getStartOfSession(long *);
  /*! \brief Still unused */
  virtual int getFeature(unsigned int feature, unsigned char *buf,
			 unsigned long bufLen, int showMsg);

  /*! \brief Reads Media Catalog Number

    If option OPT_MMC_SCAN_MCN is specified uses CdrDriver::readCatalogScan to
    scan subchannels of sectors from startlba to endlba to find MCN and (if valid)
    saves it into Toc. If OPT_MMC_SCAN_MCN isn't asserted uses proper command
    READ SUBCHANNEL 0x42 with parameter 0x02 to do the same (and startlba, endlba
    are unused).
    \param Toc* Toc of the disk. MCN can be saved here
    \param startLba Start subchannel scan here
    \param endLba End subchannel scan here
    \return 1 if valid MCN found, else 0
  */
  int readCatalog(Toc *, long startLba, long endLba);
  /*! \brief Reads media ISRC

    Uses READ SUBCHANNEL 0x42 with parameter 0x03. Track is specified
    by the int parameter.
    If TCVAL == 1 ISRC is valid and is copied to char* parameter.
    \param int Track to read ISRC of.
    \param char* Pre-allocated buffer to store ISRC (12 bytes)
    \return Always 0
  */
  int readIsrc(int, char *);

  /*! \brief Sets read/write speed

    Sets the read speed from readSpeed parameter, and write
    speed from GenericMMC::speed_ protected attribute.
    Both can be specified as 0 for maximum or as multiplier.
    \param readSpeed Read speed to set: use 0 for max or multiplier
    \todo This documentation needs to be updated
    \return 1 on SCSI error, 0 on success
  */
  virtual int selectSpeed();

  /*! \brief Sets write parameters in page 0x05

    - Write type is 0x02 (SAO).
    - Simulation is set from GenericMMC::simulate_ protected attrib.
    - BurnFree is set if drive supports it, unless explicitly disabled.
    - Next session opened if GenericMMC::multiSession_
    - Data block type ranges from 0 to 3, depending on the CD-TEXT encoder.
    \param variant Ranges from 0 to 3, defining modes 0-3 of Data Block Type
        Codes in Write Mode Page 0x05.
    \return 0 on success, 1 on SCSI error.
  */
  virtual int setWriteParameters(unsigned long variant);

  /*! \brief Puts drive in simulation/write mode

    Based on GenericMMC::simulate_ attrib.
    \param showMessage If asserted print out errors.
    \return 0 on success, 1 on SCSI errors
  */
  int setSimulationMode(int showMessage);

  /*! \brief Asks unit to perform power calibration

    Uses SEND OPC INFORMATION (0x54) command with DoOpc set to 1.
    \return 0 if command not supported or power calibration successful,
        1 on error
  */
  int performPowerCalibration();

  /*! \brief Free data bytes available in the device's buffer.

    Uses READ BUFFER CAPACITY 0x5C command.
    \param capacity Will hold number of bytes.
    \todo This documentation needs to be updated
    \return 0 on success, 1 on SCSI command error.
  */
  bool readBufferCapacity(long *capacity, long *available);

  /*! \brief Creates cue sheet for current toc

    Called by GenericMMC::sendCueSheet to build cuesheets.
    \param variant Ranges from 0 to 3, defining modes 0-3 of Data Block Type
        Codes in Write Mode Page 0x05.
    \param cueSheetLen Filled with cue sheet length in bytes
    \return Newly allocated cue sheet buffer or 'NULL' on error
  */
  unsigned char *createCueSheet(unsigned long variant, long *cueSheetLen);

  /*! \brief Builds and sends cuesheet

    For each of the 4 available variants builds (using GenericMMC::sendCueSheet)
    a cuesheet and sends to the device using SEND CUE SHEET 0x5D.
    Stops when device accepts a variant.
    \return 0 on success, 1 on error (no variants accepted)
  */
  int sendCueSheet();

  /*! \brief Returns subchannel SCSI standard code for cuesheet

    Used in GenericMMC::createCueSheet to obtain code for cuesheet.
    \param TrackData::SubChannelMode One of SUBCHAN_NONE,SUBCHAN_RW,SUBCHAN_RW_RAW
    \param encodingMode 0 or 1
    \return int
		- 0 if SUBCHAN_NONE
		- 0x40 if encodingMode==0 and SUBCHAN_RW(_RAW)
		- 0x40 if encodingMode==1 and SUBCHAN_RW(_RAW)
  */
  unsigned char subChannelDataForm(TrackData::SubChannelMode,
				   int encodingMode);

  /*! \brief Writes the subchannel data in the leadin section, used in CD-TEXT

	Issues WRITE (0x2A) commands to write PW subchannels starting
	at LBA = -150 - leadInLen_
	\return int
		- 0 if cdTextEncoder_ == NULL or everything OK
		- 1 if some error occurred (message output)
  */
  int writeCdTextLeadIn();

  int analyzeTrack(TrackData::Mode, int trackNr, long startLba, long endLba,
		   Msf *index,
		   int *indexCnt, long *pregap, char *isrcCode,
		   unsigned char *ctl);

  int getTrackIndex(long lba, int *trackNr, int *indexNr, unsigned char *ctl);

  int readSubChannels(TrackData::SubChannelMode, long lba, long len,
		      SubChannel ***, Sample *);

  /*!
  \brief retrieve mode of the track that starts at the specified trackStartLba.

  Uses READ CD (0xBE) command with byte 9 == 0xF8 (SYNC + Header + Userdata + EDC_ECC + C2 and block errors,
  No subchannels) to retrieve the sector addressed as trackStartLba. Uses
  CdrDriver::determineSectorMode to identify track mode.
  \return TrackData::Mode
  */
  TrackData::Mode getTrackMode(int, long trackStartLba);

  CdRawToc *getRawToc(int sessionNr, int *len);

  long readTrackData(TrackData::Mode, TrackData::SubChannelMode,
		     long lba, long len, unsigned char *buf);

  int readAudioRange(ReadDiskInfo *, int fd, long start, long end,
		     int startTrack, int endTrack, TrackInfo *);

  int readCdTest(long lba, long len, int subChanMode) const;

  /*!
  \brief Checks for ready status of the drive after a write operation. Disc must be inserted.

  Unit should return successfully from TEST UNIT READY command, and from READ DISC INFORMATION.
  \return int
	- 0 drive ready
	- 1 error occured at os level, no sense data
	- 2 NOT READY,LONG WRITE IN PROGRESS
  */
  int checkDriveReady() const;

  int RicohGetWriteOptions();
  int RicohSetWriteOptions(const DriveInfo *);
};

#endif
