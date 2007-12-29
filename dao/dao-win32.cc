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

#include "config.h"

#include <stdio.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <sys/types.h>

#include "dao.h"
#include "log.h"

#define BUFSIZE 150

// buffer size in blocks
int BUFFER_SIZE = BUFSIZE;

static int TERMINATE = 0;
unsigned char rbuf1[BUFSIZE * AUDIO_BLOCK_LEN];
// unsigned char rbuf2[BUFSIZE * AUDIO_BLOCK_LEN];

/*
typedef struct uebergabe_write
{
   CdrDriver *cdr;
   int testmode;
   DWORD total;
} UWRITE;
*/

int writeDiskAtOnce (const Toc *toc, CdrDriver *cdr, int nofBuffers, int swap,
		     int testMode)
{
  long length  = toc->length().lba();
  long total   = length * AUDIO_BLOCK_LEN;
  long n, rn;
  int  err = 0;

  long cnt      = 0;
  long blkCount = 0;
  long lba      = 0;     // LBA for writing
  long encodeLba = 150;  // LBA for encoding data blocks

  long cntMb;
  long lastMb     = 0;

  const Track *track = NULL;
  int trackNr = 1;
  Msf tstart, tend;
  TrackData::Mode dataMode;
  int encodingMode = 0;


  /*
  DWORD thread;
  UWRITE uw;
  */

  TERMINATE = 0;

  if (cdr != NULL && cdr->bigEndianSamples() == 0) 
  {
     // swap samples for little endian recorders
     swap = !swap;
     encodingMode = cdr->encodingMode();
  }

  if (!testMode) 
  {
     if (cdr->initDao() != 0) 
     {
        err = 1;
        goto fail;
     }
  }

  TrackIterator itr(toc_);
  TrackReader reader;

  track = itr.first(tstart, tend);
  reader.init(track);

  if (reader.openData() != 0) {
    log_message(-2, "Opening of track data failed.");
    err = 1;
    goto fail;
  }

  dataMode = encodingMode == 0 ? TrackData::AUDIO : track->type();

  if (!testMode) {
    if (cdr->startDao() != 0)
    {
      err = 2;
      goto fail;
    }
     
    log_message(0, "Writing tracks...");
  }

  log_message(0, "Writing track %02d (mode %s/%s)...", trackNr,
	  TrackData::mode2String(track->type()),
	  TrackData::mode2String(dataMode));

  while (length > 0 && !TERMINATE) {
    n = (length > BUFFER_SIZE ? BUFFER_SIZE : length);

    do {
      rn = reader.readData(encodingMode, encodeLba, (char*)rbuf1, n);
    
      if (rn < 0) {
	log_message(-2, "Reading of track data failed.");
	err = 1;
	goto fail;
      }
      
      if (rn == 0) {
	track = itr.next(tstart, tend);

	reader.init(track);
	if (reader.openData() != 0) {
	  log_message(-2, "Opening of track data failed.");
	  err = 1;
	  goto fail;
	}
	trackNr++;
	if (encodingMode != 0)
	  dataMode = track->type();

	log_message(0, "Writing track %02d (mode %s/%s)...", trackNr,
		TrackData::mode2String(track->type()),
		TrackData::mode2String(dataMode));

      }
    } while (rn == 0);

    encodeLba += rn;

    if (track->type() == TrackData::AUDIO && swap) 
      swapSamples ((Sample *)rbuf1, rn * SAMPLES_PER_BLOCK);

    if (!testMode) 
    {
      if (cdr->writeData(dataMode, lba, (char *) rbuf1, rn) != 0) 
      {
	log_message(-2, "Write of audio data failed.");
	cdr->flushCache();
	err = 1;
	goto fail;
      }
      else 
      {
	cntMb = cnt >> 20;
	
	if (cntMb > lastMb)
	{
	  log_message(0, "Wrote %ld of %ld MB.\r", cnt >> 20, total >> 20);
	  lastMb = cntMb;
	}
      }
    }
    else 
    {
      log_message(0, "Read %ld of %ld MB.\r", cnt >> 20, total >> 20);
    }
    
    length   -= rn;
    cnt      += rn * AUDIO_BLOCK_LEN;
    blkCount += rn;

    /*
      uw.cdr      = cdr;
      uw.testmode = testMode; 
      uw.total    = total;
      
      if ((thread = _beginthread (writeSlave, 4096, &uw)) != -1)
      {

      
      }
      */
  }

  if (testMode)
    log_message(0, "Read %ld blocks.", blkCount);
  else
     log_message(0, "Wrote %ld blocks.", blkCount);

  if (!testMode && daoStarted) 
  {
     if (cdr->finishDao() != 0)
        err = 3;
  }

fail:

  return err;
}

