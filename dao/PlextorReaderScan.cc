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

#include <config.h>

#include <string.h>
#include <assert.h>

#include "PlextorReaderScan.h"
#include "PWSubChannel96.h"
#include "PQSubChannel16.h"

#include "Toc.h"
#include "log.h"

PlextorReaderScan::PlextorReaderScan(ScsiIf *scsiIf, unsigned long options)
  : PlextorReader(scsiIf, options)
{
  int i;

  driverName_ = "Plextor CD-ROM Reader (scanning) - Version 1.0";

  for (i = 0; i < maxScannedSubChannels_; i++) {
    if (options_ & OPT_PLEX_USE_PQ)
      scannedSubChannels_[i] = new PQSubChannel16;
    else 
      scannedSubChannels_[i] = new PWSubChannel96;
  }
}

PlextorReaderScan::~PlextorReaderScan()
{
  int i;

  for (i = 0; i < maxScannedSubChannels_; i++) {
    delete scannedSubChannels_[i];
    scannedSubChannels_[i] = NULL;
  }
}

// static constructor
CdrDriver *PlextorReaderScan::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new PlextorReaderScan(scsiIf, options);
}

Toc *PlextorReaderScan::readDisk(int session, const char *fname)
{
  Toc *toc = CdrDriver::readDisk(session, fname);

  setBlockSize(MODE1_BLOCK_LEN);

  return toc;
}

int PlextorReaderScan::analyzeTrack(TrackData::Mode mode, int trackNr,
				    long startLba,
				    long endLba, Msf *indexIncrements,
				    int *indexIncrementCnt, long *pregap,
				    char *isrcCode, unsigned char *ctl)
{
  int ret = analyzeTrackScan(mode, trackNr, startLba, endLba,
			     indexIncrements, indexIncrementCnt, pregap,
			     isrcCode, ctl);

  if ((options_ & OPT_PLEX_READ_ISRC) ||
      ((options_ & OPT_PLEX_USE_PQ) && !(options_ & OPT_PLEX_PQ_BCD))) {
    // The ISRC code is usually not usable if the PQ channel data is
    // converted to hex numbers by the drive. Read them with the
    // appropriate command in this case

    *isrcCode = 0;
    if (mode == TrackData::AUDIO)
      readIsrc(trackNr, isrcCode);
  }

  return ret;
}

int PlextorReaderScan::readSubChannels(TrackData::SubChannelMode sm,
				       long lba, long len, SubChannel ***chans,
				       Sample *audioData)
{
  unsigned char cmd[12];
  int i;
  int retries = 5;
  long blockLen;

  if (options_ & OPT_PLEX_USE_PQ)
    blockLen = AUDIO_BLOCK_LEN + 16;
  else 
    blockLen = AUDIO_BLOCK_LEN + 96;
  
  cmd[0] = 0xd8;  // READ CDDA
  cmd[1] = 0;
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;
  cmd[6] = 0;
  cmd[7] = len >> 16;
  cmd[8] = len >> 8;
  cmd[9] = len;
  
  if (options_ & OPT_PLEX_USE_PQ)
    cmd[10] = 0x01;
  else
    cmd[10] = 0x02;
  
  cmd[11] = 0;

  while (1) {
    if (sendCmd(cmd, 12, NULL, 0, transferBuffer_, len * blockLen,
		retries == 0 ? 1 : 0) != 0) {
      if (retries == 0)
	return 1;
    }
    else {
      break;
    }

    retries--;
  }

#if 0
  if (lba > 5000) {
    char fname[200];
    sprintf(fname, "testout_%ld", lba);
    FILE *fp = fopen(fname, "w");
    fwrite(transferBuffer_, blockLen, len, fp);
    fclose(fp);
  }
#endif

  unsigned char *p =  transferBuffer_ + AUDIO_BLOCK_LEN;

  for (i = 0; i < len; i++) {
    if (options_ & OPT_PLEX_USE_PQ) {
      if (!(options_ & OPT_PLEX_PQ_BCD)) {
	// Numbers in sub-channel data are hex instead of BCD.
	// We have to convert them back to BCD for the 'SubChannel' class.
	p[1] = SubChannel::bcd(p[1]);
	p[2] = SubChannel::bcd(p[2]);
	p[3] = SubChannel::bcd(p[3]);
	p[4] = SubChannel::bcd(p[4]);
	p[5] = SubChannel::bcd(p[5]);
	p[6] = SubChannel::bcd(p[6]);
	p[7] = SubChannel::bcd(p[7]);
	p[8] = SubChannel::bcd(p[8]);
	p[9] = SubChannel::bcd(p[9]);
      }

      ((PQSubChannel16*)scannedSubChannels_[i])->init(p);

      if (scannedSubChannels_[i]->type() != SubChannel::QMODE_ILLEGAL) {
	// the CRC of the sub-channel data is usually invalid -> mark the
	// sub-channel object that it should not try to verify the CRC
	scannedSubChannels_[i]->crcInvalid();
      }
    }
    else {
      ((PWSubChannel96*)scannedSubChannels_[i])->init(p);
    }

    p += blockLen;
  }

  if (audioData != NULL) {
    p = transferBuffer_;

    for (i = 0; i < len; i++) {
      memcpy(audioData, p, AUDIO_BLOCK_LEN);

      p += blockLen;
      audioData += SAMPLES_PER_BLOCK;
    }
  }

  *chans = scannedSubChannels_;
  return 0;
}

int PlextorReaderScan::readAudioRange(ReadDiskInfo *info, int fd, long start,
				      long end, int startTrack, int endTrack, 
				      TrackInfo *trackInfo)
{
  if (!onTheFly_) {
    if ((options_ & OPT_PLEX_READ_ISRC) ||
	((options_ & OPT_PLEX_USE_PQ) && !(options_ & OPT_PLEX_PQ_BCD))) {
      int t;

      log_message(1, "Analyzing...");
      
      for (t = startTrack; t <= endTrack; t++) {
	long totalProgress;

	log_message(1, "Track %d...", t + 1);

	totalProgress = t * 1000;
	totalProgress /= info->tracks;
	sendReadCdProgressMsg(RCD_ANALYZING, info->tracks, t + 1, 0,
			      totalProgress);

	trackInfo[t].isrcCode[0] = 0;
	readIsrc(t + 1, trackInfo[t].isrcCode);
	if (trackInfo[t].isrcCode[0] != 0)
	  log_message(2, "Found ISRC code.");

	totalProgress = (t + 1) * 1000;
	totalProgress /= info->tracks;
	sendReadCdProgressMsg(RCD_ANALYZING, info->tracks, t + 1, 1000,
			      totalProgress);
      }

      log_message(1, "Reading...");
    }
  }

  return CdrDriver::readAudioRangeParanoia(info, fd, start, end, startTrack,
					   endTrack, trackInfo);
}
