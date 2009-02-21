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

#include <config.h>

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "PlextorReader.h"

#include "Toc.h"
#include "util.h"
#include "log.h"

#include "PWSubChannel96.h"

PlextorReader::PlextorReader(ScsiIf *scsiIf, unsigned long options)
  : CdrDriver(scsiIf, options)
{
  driverName_ = "Plextor CD-ROM Reader - Version 1.3";
  
  speed_ = 0;
  simulate_ = false;
  audioDataByteOrder_ = 0;
  
  slow_down_on_read_errors = -1;
  transfer_data_before_max_speed = -1;
  slow_down_on_vibrations = -1;
  
  memset(&diskInfo_, 0, sizeof(DiskInfo));
  diskInfo_.valid.empty = 1;

  {
    struct
    {   
      int number;
      const char *productid; // as obtained through INQUIRY
    } models[] =
      {
      { 1,"CD-ROM PX-4XCH" },
      { 2,"CD-ROM PX-4XCS" },
      { 3,"CD-ROM PX-4XCE" },
      { 4,"CD-ROM PX-6X" },
      { 5,"CD-ROM PX-8X" },
      { 6,"CD-ROM PX-12" },
      { 7,"CD-ROM PX-20" },
      { 8,"CD-ROM PX-32" },
      { 9,"CD-ROM PX-40" },
      { 0,NULL }
    };
    int m = 0;
    while (models[m].number)
    {
      if (strncmp(scsiIf_->product(), models[m].productid,
		  strlen(models[m].productid)) == 0)
        break;
      else
        m++;
    }
    model_ = models[m].number; // zero if not found
   
	if (model_ > 0)	{
    /* Plextor special features initialization */
        unsigned char buf[32];
        unsigned char header[8];
        unsigned char blockdesc[8];
        struct plex_msg {
            int value;
            const char *msg;
        };
        struct plex_msg slowdown_msg[] = {
            { -1, "Unsupported"},
            { 0, "Enabled, unit will slow down"},
            { 1, "Disabled, unit won't slow down"}
        };
        struct plex_msg trbefmax_msg[] = {
            { -1, "Unsupported"},
            { 0, "Disabled, unit will wait for max speed"},
            { 1, "Enabled, unit will transfer as soon as possible"}
        };
        /* Read error slowdown seems available for all drives, while
        vibration and transfer control only for PX-20 and later
        Data is taken from
        Mode page 0x31 byte 3 [ x x x x x sl td re ]
        re=read errors , if 0 slow down on read errors, 0 default
        td=transfer data, if 0 wait for max speed before transfer data, 0 default
        sl=if 0 slowdown to avoid vibrations, 0 default
        */
        slow_down_on_read_errors = -1;
        transfer_data_before_max_speed = -1;
        slow_down_on_vibrations = -1;
        orig_byte3 = 0;
        if (getModePage6 (0x31, buf, 32, header, blockdesc, 1) == 0)    {
            orig_byte3 = buf[3];
            slow_down_on_read_errors = buf[3] & 0x01;
            if (model_ >= 7)    {
                transfer_data_before_max_speed = (buf[3] & 0x02) >> 1;
                slow_down_on_vibrations = (buf[3] & 0x04) >> 2;
            }
        }
        if (options & OPT_PLEX_NOSLOW_ON_ERR)
            ReadErrorsSlowDown (0);
        if (options & OPT_PLEX_TRANSF_BEF_MAX)
            WaitMaxSpeed (0);
        if (options & OPT_PLEX_NOSLOW_ON_VIB)
            VibrationsSlowDown (0);
        log_message(4, "Plextor features status:");
        log_message(4, "  Slowdown on read errors: %s",
            slowdown_msg[slow_down_on_read_errors+1].msg);
        log_message(4, "  Slowdown to avoid vibrations: %s",
            slowdown_msg[slow_down_on_vibrations+1].msg);
        log_message(4, "  Transfer data before max speed: %s",
            trbefmax_msg[transfer_data_before_max_speed+1].msg);
    }
    log_message(4, "model number %d\n",model_);
    log_message(4, "PRODUCT ID: '%s'\n", scsiIf_->product());
  }
}

PlextorReader::~PlextorReader ()        {
	/* Only for Plextor units */
	if (model_ > 0)	{
    	unsigned char buf[32];
    	unsigned char header[8];
    	unsigned char blockdesc[8];
    	if (getModePage6 (0x31, buf, 32, header, blockdesc, 1) != 0) 
        	return;
    	buf[3] = orig_byte3;
    	setModePage6 (buf, header, blockdesc, 1);
	}
}

int PlextorReader::ReadErrorsSlowDown (int slowdown)    {
    /* Not supported */
    if (slow_down_on_read_errors == -1 ) return -1;
    slowdown ? slowdown = 0 : slowdown = 1;

    unsigned char buf[32];
    unsigned char header[8];
    unsigned char blockdesc[8];
    if (getModePage6 (0x31, buf, 32, header, blockdesc, 1) != 0) 
        return -1;
    if (slowdown) 
        buf[3] |= 0x01;
    if (setModePage6 (buf, header, blockdesc, 1) != 0) 
        return -1;
    slow_down_on_read_errors = slowdown;
    return 1;
}

int PlextorReader::VibrationsSlowDown (int slowdown)    {
    if (slow_down_on_vibrations == -1) return -1;
    slowdown ? slowdown = 0 : slowdown = 1;
    
    unsigned char buf[32];
    unsigned char header[8];
    unsigned char blockdesc[8];
    if (getModePage6 (0x31, buf, 32, header, blockdesc, 1) != 0) 
        return -1;
    if (slowdown) 
        buf[3] |= 0x04;
    if (setModePage6 (buf, header, blockdesc, 1) != 0) 
        return -1;
    slow_down_on_vibrations = slowdown;
    return 1;
}

int PlextorReader::WaitMaxSpeed (int wait)      {
    if (transfer_data_before_max_speed == -1) return -1;
    wait ? wait = 0 : wait = 1;
    
    unsigned char buf[32];
    unsigned char header[8];
    unsigned char blockdesc[8];
    if (getModePage6 (0x31, buf, 32, header, blockdesc, 1) != 0) 
        return -1;
    if (wait) 
        buf[3] |= 0x02;
    if (setModePage6 (buf, header, blockdesc, 1) != 0) 
        return -1;
    transfer_data_before_max_speed = wait;
    return 1;
}

// static constructor
CdrDriver *PlextorReader::instance(ScsiIf *scsiIf, unsigned long options)
{
  return new PlextorReader(scsiIf, options);
}

// sets speed
// return: 0: OK
//         1: illegal speed (n/a; the strategy is to map to a valid speed)
int PlextorReader::speed(int speed)
{
  unsigned short dataLen = 56;
  unsigned char data[56];
  char speedvalue=0;

  if (model_ == 0) // not a Plextor device
    return 0;

  switch (model_) {
  case 4:
    speedvalue = (speed-1)/2;
    break;
  default:
    speedvalue = speed/2;
    break;
  }
  
  getModePage6((long)0x31,data,dataLen,NULL,NULL,1);
  //log_message(0,"page code %0x\n",data[0]);
  //log_message(0,"speed value was %d\n",data[2]);
  data[2]=speedvalue;
  //log_message(0,"new speedvalue %d\n",speedvalue);    
  setModePage6(data,NULL,NULL,1);

  return 0;  
}

DiskInfo *PlextorReader::diskInfo()
{
  return &diskInfo_;
}

int PlextorReader::initDao(const Toc *toc)
{
  log_message(-2, "Writing is not supported by this driver.");
  return 1;
}

int PlextorReader::startDao()
{
  return 1;
}

int PlextorReader::finishDao()
{
  return 1;
}

void PlextorReader::abortDao()
{
}

// tries to read catalog number from disk and adds it to 'toc'
// return: 1 if valid catalog number was found, else 0

int PlextorReader::readCatalog(Toc *toc, long startLba, long endLba)
{
  unsigned char cmd[10];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];
  char catalog[14];
  int i;

  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x02; // get media catalog number
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read sub channel data.");
    return 0;
  }

  if (data[0x08] & 0x80) {
    for (i = 0; i < 13; i++) {
      catalog[i] = data[0x09 + i];
    }
    catalog[13] = 0;

    if (toc->catalog(catalog) == 0) {
      return 1;
    }
  }

  return 0;
}

// plays one audio block starting from given position
void PlextorReader::playAudioBlock(long start, long len)
{
  unsigned char cmd[10];

  // play one audio block
  memset(cmd, 0, 10);
  cmd[0] = 0x45; // PLAY AUDIO
  cmd[2] = start >> 24;
  cmd[3] = start >> 16;
  cmd[4] = start >> 8;
  cmd[5] = start;
  cmd[7] = len >> 8;
  cmd[8] = len;

  if (sendCmd(cmd, 10, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot play audio block.");
    return;
  }
}

int PlextorReader::analyzeTrack(TrackData::Mode mode, int trackNr,
				long startLba, long endLba,
				Msf *index, int *indexCnt, long *pregap,
				char *isrcCode, unsigned char *ctl)
{
  int ret = analyzeTrackSearch(mode, trackNr, startLba, endLba,
			       index, indexCnt, pregap, isrcCode, ctl);

  if (mode == TrackData::AUDIO)
    readIsrc(trackNr, isrcCode);
  else
    *isrcCode = 0;

  return ret;
}

// tries to read ISRC code of given track number and stores it in
// given buffer.
// return: 1 if valid ISRC code was found, else 0
int PlextorReader::readIsrc(int trackNr, char *buf)
{
  unsigned char cmd[10];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];
  int i;

  // time to finish the last audio play operation
  sleep(1);

  buf[0] = 0;

  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x03; // get ISRC code
  cmd[6] = trackNr;
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-1, "Cannot read ISRC code.");
    return 0;
  }

  if (data[0x08] & 0x80) {
    for (i = 0; i < 12; i++) {
      buf[i] = data[0x09 + i];
    }
    buf[12] = 0;

  }

  return 0;
}

// Reads actual track number, index and relative position from sub channel.
// return: 0 OK, 1: SCSI command failed
int PlextorReader::readSubChannelData(int *trackNr, int *indexNr,
				      long *relPos, unsigned char *ctl)
{
  unsigned char cmd[10];
  unsigned short dataLen = 0x30;
  unsigned char data[0x30];

  // read sub channel information
  memset(cmd, 0, 10);
  cmd[0] = 0x42; // READ SUB CHANNEL
  cmd[2] = 0x40; // get sub channel data
  cmd[3] = 0x01; // get sub Q channel data
  cmd[6] = 0;
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read sub Q channel data.");
  }

  *trackNr = data[6];
  *indexNr = data[7];
  *relPos = 0;
  *relPos |= data[0x0c] << 24;
  *relPos |= data[0x0d] << 16;
  *relPos |= data[0x0e] << 8;
  *relPos |= data[0x0f];
  
  if (ctl != NULL) {
    *ctl = data[5] & 0x0f;
  }

  return 0;
}

// Retrieves track and index at given block address 'lba'.
// Return: 0: OK, 1: SCSI command failed
int PlextorReader::getTrackIndex(long lba, int *trackNr, int *indexNr,
				 unsigned char *ctl)
{
  long relPos;

  playAudioBlock(lba, 1);

  return readSubChannelData(trackNr, indexNr, &relPos, ctl);
}

CdRawToc *PlextorReader::getRawToc(int sessionNr, int *len)
{
  unsigned char cmd[10];
  unsigned short dataLen;
  unsigned char *data = NULL;;
  unsigned char reqData[4]; // buffer for requestion the actual length
  unsigned char *p = NULL;
  int i, entries;
  CdRawToc *rawToc;
  
  assert(sessionNr >= 1);

  // read disk toc length
  memset(cmd, 0, 10);
  cmd[0] = 0x43; // READ TOC
  cmd[6] = sessionNr;
  cmd[8] = 4;
  cmd[9] |= 2 << 6; // get Q subcodes

  if (sendCmd(cmd, 10, NULL, 0, reqData, 4) != 0) {
    log_message(-2, "Cannot read raw disk toc.");
    return NULL;
  }

  dataLen = ((reqData[0] << 8) | reqData[1]) + 2;

  log_message(4, "Raw toc data len: %d", dataLen);

  data = new unsigned char[dataLen];
  
  // read disk toc
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read raw disk toc.");
    delete[] data;
    return NULL;
  }

  entries = (((data[0] << 8) | data[1]) - 2) / 11;


  rawToc = new CdRawToc[entries];

  for (i = 0, p = data + 4; i < entries; i++, p += 11 ) {
#if 0
    log_message(0, "%d %02x %02d %2x %02x:%02x:%02x %02x %02x:%02x:%02x",
	    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10]);
#endif
    rawToc[i].sessionNr = p[0];
    rawToc[i].adrCtl = p[1];
    rawToc[i].point = p[3];
    rawToc[i].pmin = p[8];
    rawToc[i].psec = p[9];
    rawToc[i].pframe = p[10];
  }

  delete[] data;

  *len = entries;

  return rawToc;
}

long PlextorReader::readTrackData(TrackData::Mode mode,
				  TrackData::SubChannelMode,
				  long lba, long len, unsigned char *buf)
{
  unsigned char cmd[10];
  long blockLen = 2340;
  long i;
  TrackData::Mode actMode;
  int ok = 0;
  const unsigned char *sense;
  int senseLen;
  int softError;

  if (setBlockSize(blockLen) != 0)
    return 0;

  memset(cmd, 0, 10);

  cmd[0] = 0x28; // READ10
  cmd[2] = lba >> 24;
  cmd[3] = lba >> 16;
  cmd[4] = lba >> 8;
  cmd[5] = lba;

  while (len > 0 && !ok) {
    cmd[7] = len >> 8;
    cmd[8] = len;

    memset(transferBuffer_, 0, len * blockLen);
    switch (sendCmd(cmd, 10, NULL, 0, transferBuffer_, len * blockLen, 0)) {
    case 0:
      ok = 1;
      break;

    case 2:
      softError = 0;
      sense = scsiIf_->getSense(senseLen);

      if (senseLen > 0x0c) {
	if ((sense[2] &0x0f) == 5) { // Illegal request
	  switch (sense[12]) {
	  case 0x63: // End of user area encountered on this track
	  case 0x64: // Illegal mode for this track
	    softError = 1;
	    break;
	  }
	}
	else if ((sense[2] & 0x0f) == 3) { // Medium error
	  switch (sense[12]) {
	  case 0x02: // No seek complete, sector not found
	  case 0x11: // L-EC error
	    return -2;
	    break;
	  }
	}
      }

      if (!softError) {
	scsiIf_->printError();
	return -1;
      }
      break;

    default:
      log_message(-2, "Read error at LBA %ld, len %ld", lba, len);
      return -1;
      break;
    }

    if (!ok) {
      len--;
    }
  }

  unsigned char *sector = transferBuffer_;
  for (i = 0; i < len; i++) {
    actMode = determineSectorMode(sector);

    if (!(actMode == mode ||
	  (mode == TrackData::MODE2_FORM_MIX &&
	   (actMode == TrackData::MODE2_FORM1 ||
	    actMode == TrackData::MODE2_FORM2)) ||

	  (mode == TrackData::MODE1_RAW && actMode == TrackData::MODE1) ||

	  (mode == TrackData::MODE2_RAW &&
	   (actMode == TrackData::MODE2 ||
	    actMode == TrackData::MODE2_FORM1 ||
	    actMode == TrackData::MODE2_FORM2)))) {
      log_message(4, "Stopped because sector with not matching mode %s found.",
	      TrackData::mode2String(actMode));
      log_message(4, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
	      sector[0], sector[1], sector[2], sector[3],  sector[4],
	      sector[5], sector[6], sector[7], sector[8], sector[9],
	      sector[10], sector[11]);
      return i;
    }

    if (buf != NULL) {
      switch (mode) {
      case TrackData::MODE1:
	memcpy(buf, sector + 4, MODE1_BLOCK_LEN);
	buf += MODE1_BLOCK_LEN;
	break;
      case TrackData::MODE2:
      case TrackData::MODE2_FORM_MIX:
	memcpy(buf, sector + 4, MODE2_BLOCK_LEN);
	buf += MODE2_BLOCK_LEN;
	break;
      case TrackData::MODE2_FORM1:
	memcpy(buf, sector + 12, MODE2_FORM1_DATA_LEN);
	buf += MODE2_FORM1_DATA_LEN;
	break;
      case TrackData::MODE2_FORM2:
	memcpy(buf, sector + 12, MODE2_FORM2_DATA_LEN);
	buf += MODE2_FORM2_DATA_LEN;
	break;
      case TrackData::MODE1_RAW:
      case TrackData::MODE2_RAW:
	memcpy(buf, syncPattern, 12);
	memcpy(buf + 12, sector, 2340);
	buf += AUDIO_BLOCK_LEN;
	break;
      case TrackData::MODE0:
      case TrackData::AUDIO:
	log_message(-3, "PlextorReader::readTrackData: Illegal mode.");
	return 0;
	break;
      }
    }

    sector += blockLen;
  }  

  return len;
}

Toc *PlextorReader::readDiskToc(int session, const char *audioFilename)
{
  Toc *toc = CdrDriver::readDiskToc(session, audioFilename);

  setBlockSize(MODE1_BLOCK_LEN);
  
  return toc;
}

Toc *PlextorReader::readDisk(int session, const char *dataFilename)
{
  Toc *toc = CdrDriver::readDisk(session, dataFilename);

  setBlockSize(MODE1_BLOCK_LEN);
  
  return toc;
}

// Creates 'Toc' object for inserted CD.
// audioFilename: name of audio file that is placed into TOC
// Return: newly allocated 'Toc' object or 'NULL' on error
int PlextorReader::readAudioRangePlextor(ReadDiskInfo *rinfo, int fd,
					 long startLba, long endLba,
					 int startTrack, int endTrack, 
					 TrackInfo *info)
{
  int i;
  int ret;
  //unsigned char trackCtl; // control nibbles of track
  //int ctlCheckOk;
 
  int blockLength = AUDIO_BLOCK_LEN + 1 + 294 + 96;
  long  blocksPerRead = scsiIf_->maxDataLen() / blockLength;
  long stampLba;

  // first, last, current and number of recorded tracks
  int fat=-1,lat=-1,cat,nat=0;

  // first, last, number, current, remaining and read (audio blocks)
  long fab,lab,nab,cab,rab,n;
  
  // SCSI command 
  unsigned char cmd[12];   
  unsigned char *data;

  // number of byte errors in current audio block (cab)
  int cabFaulty;

  // number of retries and goods of current audio block (cab)
  unsigned long cabRetries=0,cabGoods=0,scsiRetries=0;

  // audio repair block
  unsigned char repairBlock[AUDIO_BLOCK_LEN + 1 + 294 + 96];
  //unsigned char referBlock[AUDIO_BLOCK_LEN + 1 + 294 + 96];
  int repairErrors=-1;  

  int overspeed,cai=-1;

  data = new unsigned char[blocksPerRead * blockLength];  

  fat = startTrack;
  lat = endTrack;
  nat = endTrack - startTrack + 1;

  info[lat].bytesWritten = 0;

  // set first, last and number of audio blocks
  fab = startLba;
  lab = endLba;

  nab=lab-fab;

  log_message(1, "Reading CDDA tracks in range [%lu,%lu].", fat+1,lat+1);

  cat = fat + 1;
 
  log_message(2, "Reading CDDA blocks in range [%lu,%lu>"
    " (%lu blocks=%lu bytes).",fab,lab,nab,nab*2352);

  memset(cmd,0,12);
  cmd[0]=0xd8;
  cmd[10]=0x09; 
 
#if 0 // for testing purposes I set the position/length of a scratch here
  fab+=3000;
  nab=921;
#endif

  // set current audio block
  cab=fab;
  // set remaining audio blocks
  rab=nab;

  stampLba = cab;

  // start optimistic
  speed(overspeed=20);


  // while still audio blocks left to read
  while (rab != 0)
  { 
    // the maximum number of blocks to read, or just those which are left
    n=(rab > blocksPerRead ? blocksPerRead : rab);
    
    // set starting audio block
    cmd[2] = cab >> 24;
    cmd[3] = cab >> 16;
    cmd[4] = cab >> 8;
    cmd[5] = cab;
    // set number of blocks
    cmd[6] = n >> 24;
    cmd[7] = n >> 16;
    cmd[8] = n >> 8;
    cmd[9] = n;

    log_message(1, "block %6ld\r",cab);

    if (remote_) {
      if (cab > stampLba + 75) {
	long totalLen = info[cat + 1].start - info[cat].start;
	long progress = cab - info[cat].start;
	long totalProgress;

	if (progress > 0) {
	  progress *= 1000;
	  progress /= totalLen;
	}
	else {
	  progress = 0;
	}

	totalProgress = cab - rinfo->startLba;

	if (totalProgress > 0) {
	  totalProgress *= 1000;
	  totalProgress /= rinfo->endLba - rinfo->startLba;
	}
	else {
	  totalProgress = 0;
	}

	sendReadCdProgressMsg(RCD_EXTRACTING, rinfo->tracks, cat + 1, progress,
			      totalProgress);

	stampLba = cab;
      }
    }

    // SCSI command failed?
    while (sendCmd(cmd,12,NULL,0,data,n*blockLength))
    {
      scsiRetries++;
      if (cabRetries==0)
      {
        if (scsiRetries==10)
        {
          delete[] data;
          return 1;
        }
      }
      else
      {
        scsiRetries=0;
        //cabRetries++;
        break;
      }
    }

    // iterate through the read audio blocks
    for (i=0;i<n;i++)
    {
      // create subchannel object
      PWSubChannel96 chan(data + i*blockLength + AUDIO_BLOCK_LEN + 1 + 294); 
 
      // q subchannel error?
      if (!chan.checkCrc())
      {
        // TODO: Implement heuristic subchannel error correction here
      }
      // q subchannel ok?
      if (chan.checkCrc() && chan.checkConsistency())
      {
        // q subchannel contains mode 1 data?
        if(chan.type() == SubChannel::QMODE1DATA)
	{
          // track relative time as it occurs in subchannel
          Msf qRelTime(chan.min(),chan.sec(),chan.frame());
          // disc absolute time as it occurs in subchannel
          Msf qAbsTime(chan.amin(),chan.asec(),chan.aframe());

          if ((chan.indexNr()!=cai) || (chan.trackNr()!=cat))
          {
            log_message(2, "(track,index,absolute)=(%2d,%2d,%s)",
              chan.trackNr(),chan.indexNr(),qAbsTime.str());

            // first track start?
            if ((chan.trackNr()==1) && (chan.indexNr()==1))
            {
              log_message(1, "Found track number %02d at %s (block %d).",
		      chan.trackNr(),qAbsTime.str(),cab);
              if ((qAbsTime.lba()-150)!=info[0].start)
              {
                log_message(2, "TOC SAYS TRACK STARTS AT %s!",
                  Msf(info[0].start).str());
              }
            }
            // next track start?
            else if ((chan.trackNr()==(cat+1)) && (chan.indexNr()==1))
            {
              log_message(1, "Found track number %02d at %s.",
                chan.trackNr(),qAbsTime.str());
              if ((qAbsTime.lba()-150)!=info[cat].start)
              {
                log_message(2, "TOC SAYS TRACK STARTS AT %s!",
			Msf(info[cat].start).str());
              }
            }
            // pregap of next track?
            else if ((chan.trackNr()==(cat+1)) && (chan.indexNr()==0))
            {
              log_message(2, "Found pregap of track %02d with size %s at ",
		      chan.trackNr(), qRelTime.str());
	      log_message(2, "%s", qAbsTime.str());
	      if (cat <= lat) {
		info[cat].pregap = qRelTime.lba();
	      }
            }
            // index increment?
            else if ((chan.trackNr()==cat) && (chan.indexNr()==(cai+1)))
            {
              if (chan.indexNr() > 1)
              {
		log_message(2, "Found index number %02d at %s.",
			chan.indexNr(),qAbsTime.str());

		int indexCnt = info[cat - 1].indexCnt;
		if (cat - 1 <= lat && indexCnt < 98) {
		  info[cat - 1].index[indexCnt] = qRelTime.lba();
		  info[cat - 1].indexCnt += 1;
		}
              }
            }
            else if (chan.trackNr()==0xAA)
            {
              log_message(2, "LEAD OUT TRACK ENCOUNTERED!");
              n=0;
              i=0;
              rab=0;
              cab-=1;
              lab=cab;
              break;
            }
            else if ((chan.trackNr()==cat) && (chan.indexNr()==cai))
            {
            }
            else
            {
              log_message(1, "UNEXPECTED!");
            }
            cat=chan.trackNr();
            cai=chan.indexNr();
          }
        }
	else if (chan.type() == SubChannel::QMODE3)
        {
	  // subchannel contains ISRC code
	  if (info[cat - 1].isrcCode[0] == 0) {
	    memcpy(info[cat - 1].isrcCode, chan.isrc(), 13);
	    log_message(2, "Found ISRC code of track %02d.", cat);
	  }
	}
      }
      // erronous subchannel data     
      else
      {
      }
 
      // read CIRC error indicator
      cabFaulty=data[i*blockLength+AUDIO_BLOCK_LEN];

      // only errorless blocks or often retried blocks are accepted
      if ((cabFaulty==0) || (repairErrors==0) || (cabRetries>80))
      {
        unsigned char *block;
        // the current audio block in the data buffer is perfect
        if (cabFaulty==0)
        {
          block=data + i*blockLength;
          // write this block's audio data
          //fwrite(data + i*blockLength,1,AUDIO_BLOCK_LEN,fd);
          // increment number of consecutive good blocks
          cabGoods++;  
          if (cabRetries)
          {
            log_message(1,"block %6ld read and written without errors now. \n", cab);
          }
        }
        // we have a fully repaired block
        else if (repairErrors==0)
        {
          // write this block's audio data
          block=repairBlock;
          //fwrite(repairBlock,1,AUDIO_BLOCK_LEN,fd);
          log_message(1,"block %6ld written after fully being repaired...", cab);
        }
        // we have a partially repaired block
        else if (cabRetries>=0)
        {
          // write this block's audio data
          block=repairBlock;
	  //fwrite(repairBlock,1,AUDIO_BLOCK_LEN,fd);
          log_message(1,"block %6ld written with %4ld erronous bytes in it!", cab, repairErrors);
        }
        else
        {
           log_message(1,"PROGRAM BUG!!!! BUG #1! ENTERED ILLEGAL STATE\n");
        }

#if 1 // not if we test please
	if (options_ & OPT_DRV_SWAP_READ_SAMPLES)
	  swapSamples((Sample*)block, SAMPLES_PER_BLOCK);

	if ((ret = fullWrite(fd, block, AUDIO_BLOCK_LEN)) != AUDIO_BLOCK_LEN) {
	  if (ret < 0)
	    log_message(-2, "Writing of data failed: %s", strerror(errno));
	  else
	    log_message(-2, "Writing of data failed: Disk full");

          delete[] data;
          return 1;
	}
	info[lat].bytesWritten += AUDIO_BLOCK_LEN;
#endif

        repairErrors=-1;
        cabRetries=0;
        // proceed to next audio block
        //log_message(0,"PROCEEDING TO NEXT BLOCK");
        cab++;
        rab--;
        if (cabGoods == 1000)
        {
          if (overspeed<20)
          {
            overspeed=((overspeed*2)>=20?20:overspeed*2);
            // crank up the speed again
            // log_message(0,"Speeding up to %ld overspeed...\r",overspeed);
            speed(overspeed);
            cabGoods=0;
          }
        }
      }
      else
      // the current block contains errors
      {
        int j;
        // first time error on this block?
        if (cabRetries==0)
        {
          // copy faulty block into our repair buffer
          memcpy(repairBlock,data+i*blockLength,AUDIO_BLOCK_LEN+1+294+96);
          // slow down to lower error rate (we hope so!?)
          if (overspeed>1) overspeed/=2;
          // log_message(0,"Slowing down to %ld...\r",overspeed);
          speed(overspeed);
        }
        // iterate through all audio samples, merge all good samples into
        // a repair audio block

        // Implementor's note: The following info is NOT mentioned in the
        // official Plextor documentation I have, but IS confirmed by
        // Plextor. -- Leon Woestenberg <leon@stack.nl>.

        // NOTE: Any two bytes of a sample (or both) can be damaged.
        // Although the error bit flag can be set for just one byte, the
        // other byte can be changed due to the fact that DSP interpolation
        // is applied to the whole (two-byte) sample.
        // THEREFORE, any of the two bits relating to a two-byte sample
        // indicate the WHOLE sample is incorrect, and we will try to
        // repair the whole sample, not just the one byte.
        // THIS will ensure we have a 100% bit-wise accurate copy of
        // the audio data. 

        repairErrors=0;
        for (j=0;j<AUDIO_BLOCK_LEN;j+=2) // iterate by steps of 1 sample
        {
          // sample erronous in repair block? (bits 7-j and/or 6-j set?)
          if (repairBlock[AUDIO_BLOCK_LEN+1+(j/8)] & (192>>(j%8)))
          {
            // sample good in fresh data block?
            if (!(data[i*blockLength+AUDIO_BLOCK_LEN+1+(j/8)] & (192>>(j%8))))
            {
#if 0
              printf("\nerrenous sample %d (bytes %d/%d) being repaired with %04x",
                j/2,j,j+1,data[i*blockLength+j]*256+data[i*blockLength+j+1]);
#endif
              // then copy it into the repairBlock;
              repairBlock[j]=data[i*blockLength+j];
              repairBlock[j+1]=data[i*blockLength+j+1];
              // clear error bit in repair block
              repairBlock[AUDIO_BLOCK_LEN+1+(j/8)] &= (~(192>>(j%8)));
            }
            // erronous sample could not be replaced by the good one
            else
            {
#if 0
              printf("%d, ",j/2);
#endif
              // increment number of repair block erronous samples
              repairErrors++; 
            }
#if 0
              log_message(0,"bits %ld/%ld (bits %ld/%ld of byte %ld) was 1",
                j,j+1,7-(j%8),6-(j%8),j/8);
#endif
          }
#if 0
          else
          {
            log_message(0,"bits %ld/%ld (bits %ld/%ld of byte %ld) were 0",
              j,j+1,7-(j%8),6-(j%8),j/8);
          }
#endif
        }
        if (cabRetries==0)
        {
          log_message(1,"\nblock %6ld has %3ld error samples\r",
            cab,repairErrors);
        }
        else
        {
          log_message(1,"block %6ld has %3ld error samples left after %3ld retries\r",
            cab,repairErrors,cabRetries);
        }
        // increase number of retries
        cabRetries++;
        cabGoods=0;
        // break out of for-loop (into while-loop) to re-read current block
        break;
      }
    }
  }  
 
  delete[] data;

  return 0;
}

int PlextorReader::readSubChannels(TrackData::SubChannelMode,
				   long lba, long len, SubChannel ***chans,
				   Sample *audioData)
{
  unsigned char cmd[12];
  int cmdLen;
  int tries = 5;
  int ret;


  memset(cmd, 0, 12);

  if (options_ & OPT_PLEX_DAE_READ10) {
    if (setBlockSize(AUDIO_BLOCK_LEN) != 0)
      return 1;

    cmd[0] = 0x28; // READ10
    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;
    cmd[7] = len >> 8;
    cmd[8] = len;

    cmdLen = 10;
  }
  else if (options_ & OPT_PLEX_DAE_D4_12) {
    cmd[0] = 0xd4;
    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;
    cmd[8] = len >> 8;
    cmd[9] = len;

    cmdLen = 12;
  }
  else {
    cmd[0]=0xd8; // READ CDDA
    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;
    cmd[6] = len >> 24;
    cmd[7] = len >> 16;
    cmd[8] = len >> 8;
    cmd[9] = len;

    cmdLen = 12;
  }

  do {
    ret = sendCmd(cmd, cmdLen, NULL, 0, 
		  (unsigned char*)audioData, len * AUDIO_BLOCK_LEN,
		  (tries == 1) ? 1 : 0);

    if (ret != 0 && tries == 1) {
      log_message(-2, "Reading of audio data failed at sector %ld.", lba);
      return 1;
    }
    
    tries--;
  } while (ret != 0 && tries > 0);

  *chans = NULL;
  return 0;
}

int PlextorReader::readAudioRange(ReadDiskInfo *rinfo, int fd, long start,
				  long end, int startTrack, int endTrack, 
				  TrackInfo *info)
{
  if (model_ != 0 && (options_ & OPT_PLEX_USE_PARANOIA) == 0) {
    // we have a plextor drive -> use the special plextor method which
    // will also detect pre-gaps, index marks and ISRC codes
    return readAudioRangePlextor(rinfo, fd, start, end, startTrack, endTrack,
				 info);
  }
  
  if (!onTheFly_) {
    int t, i;
    long pregap = 0;
    int indexCnt = 0;
    Msf index[98];
    unsigned char ctl;
    long slba, elba;

    log_message(1, "Analyzing...");
    
    for (t = startTrack; t <= endTrack; t++) {
      long totalProgress;

      log_message(1, "Track %d...", t + 1);

      totalProgress = t * 1000;
      totalProgress /= rinfo->tracks;
      sendReadCdProgressMsg(RCD_ANALYZING, rinfo->tracks, t + 1, 0,
			    totalProgress);


      if (!fastTocReading_) {
	if (pregap != 0)
	  log_message(2, "Found pre-gap: %s", Msf(pregap).str());

	slba = info[t].start;
	if (info[t].mode == info[t + 1].mode)
	  elba = info[t + 1].start;
	else
	  elba = info[t + 1].start - 150;
	
	pregap = 0;
	if (analyzeTrackSearch(TrackData::AUDIO, t + 1, slba, elba,
			       index, &indexCnt, &pregap, info[t].isrcCode,
			       &ctl) != 0)
	  return 1;
	
	for (i = 0; i < indexCnt; i++)
	  info[t].index[i] = index[i].lba();
	
	info[t].indexCnt = indexCnt;
	
	if (t < endTrack)
	  info[t + 1].pregap = pregap;
      }
      else {
	info[t].indexCnt = 0;
	info[t + 1].pregap = 0;
      }
      
      info[t].isrcCode[0] = 0;
      readIsrc(t + 1, info[t].isrcCode);
      if (info[t].isrcCode[0] != 0)
	log_message(2, "Found ISRC code.");

      totalProgress = (t + 1) * 1000;
      totalProgress /= rinfo->tracks;

      sendReadCdProgressMsg(RCD_ANALYZING, rinfo->tracks, t + 1, 1000,
			    totalProgress);
    }

    log_message(1, "Reading...");
  }

  return CdrDriver::readAudioRangeParanoia(rinfo, fd, start, end, startTrack,
					   endTrack, info);
}
