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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "PQChannelEncoder.h"

#include "Msf.h"
#include "Track.h"
#include "log.h"


PQChannelEncoder::PQChannelEncoder() 
{
  cueSheet_ = NULL;
  cueSheetLen_ = 0;
  toc_ = NULL;
  tocLen_ = 0;
  discType_ = 0;
  current_ = NULL;
  catalog_ = NULL;
  isrc_ = NULL;
}

PQChannelEncoder::~PQChannelEncoder()
{
  int i;

  for (i = 0; i < tocLen_; i++) {
    delete toc_[i];
  }
  delete[] toc_;

  delete[] cueSheet_;
  delete current_;
  delete catalog_;
  delete isrc_;
}


int PQChannelEncoder::setCueSheet(SubChannel *chan, unsigned char discType, 
				  const unsigned char *sheet, int len,
				  const Msf &leadInStart)
{
  int tocEnt, i;

  subChannel_ = chan;

  // Convert toc type to decimal numbers so that they look like the
  // corresponding hex value when stored as BCD in the sub-channel
  switch (discType) {
  case 0:
    break;
  case 0x10:
    discType = 10;
    break;
  case 0x20:
    discType = 20;
    break;
  default:
    log_message(-3, "Illegal disc type.");
    return 1;
  }

  if ((len % sizeof(CueSheetEntry)) != 0) {
    log_message(-3, "Illegal cue sheet length.");
    return 1;
  }

  discType_ = discType;

  cueSheetLen_ = len / sizeof(CueSheetEntry);
  cueSheet_ = new CueSheetEntry[cueSheetLen_ + 1];
  memcpy(cueSheet_, sheet, len);
  // mark end of queue sheet with a zeroed entry
  memset(cueSheet_ + cueSheetLen_, 0, sizeof(CueSheetEntry));

  // create some sub channel objects
  catalog_ = subChannel_->makeSubChannel(SubChannel::QMODE2);
  isrc_ = subChannel_->makeSubChannel(SubChannel::QMODE3);
  current_ = subChannel_->makeSubChannel(SubChannel::QMODE1DATA);
  
  if (analyzeCueSheet() != 0) {
    return 1;
  }

  // create PQ sub channels for toc in lead-in
  tocLen_ = lastTrackNr_ - firstTrackNr_ + 1 + 3/*A0, A1, A2*/;
  toc_ = new SubChannel*[tocLen_];

  for (i = 0; i < tocLen_; i++) {
    toc_[i] = subChannel_->makeSubChannel(SubChannel::QMODE1TOC);
  }

  CueSheetEntry *run;
  for (run = actCueSheetEntry_, tocEnt = 0; 
       (run->ctlAdr & 0x0f) != 1 || run->trackNr != 0xaa; run++) {
    if ((run->ctlAdr & 0x0f) == 1 && run->trackNr > 0 && run->trackNr <= 99 &&
	run->indexNr == 1) {
      toc_[tocEnt]->ctl(run->ctlAdr & 0xf0);
      toc_[tocEnt]->point(run->trackNr);
      toc_[tocEnt]->pmin(run->min);
      toc_[tocEnt]->psec(run->sec);
      toc_[tocEnt]->pframe(run->frame);
      tocEnt++;
    }
  }

  toc_[tocEnt]->point(0xa0);
  toc_[tocEnt]->pmin(firstTrackNr_);
  toc_[tocEnt]->psec(discType_);
  toc_[tocEnt]->ctl(firstTrackCtlAdr_ & 0xf0);
  tocEnt++;

  toc_[tocEnt]->point(0xa1);
  toc_[tocEnt]->pmin(lastTrackNr_);
  toc_[tocEnt]->ctl(lastTrackCtlAdr_ & 0xf0);
  tocEnt++;

  toc_[tocEnt]->point(0xa2);
  toc_[tocEnt]->pmin(leadOutStart_.min());
  toc_[tocEnt]->psec(leadOutStart_.sec());
  toc_[tocEnt]->pframe(leadOutStart_.frac());
  toc_[tocEnt]->ctl(leadOutCtlAdr_ & 0xf0);
  tocEnt++;

  assert(tocEnt == tocLen_);

  // setup encoder dynamic data
  abslba_ = leadInStart.lba() - 450150;
  trlba_ = 0;
  writeIsrc_ = 0;
  deferredCatalog_ = 0;
  deferredIsrc_ = 0;

  actTocEnt_ = actTocCount_ = 0;

  run = nextCueSheetEntry(actCueSheetEntry_, 1);
  assert(run != NULL);
  nextTransitionLba_ = Msf(run->min, run->sec, run->frame).lba() - 150;

  run = nextCueSheetEntry(actCueSheetEntry_, firstTrackNr_, 1);
  assert(run != NULL);
  nextTrackStartLba_ = Msf(run->min, run->sec, run->frame).lba() - 150;

  return 0;
}


int PQChannelEncoder::analyzeCueSheet()
{
  int i;
  CueSheetEntry *ent;
  int prevTrackNr = -1;
  long prevLba = -1;
  long lba;

  firstTrackNr_ = 0;
  firstTrackCtlAdr_ = 0;
  lastTrackNr_ = 0;
  lastTrackCtlAdr_ = 0;
  leadOutStart_ = 0;
  leadOutCtlAdr_ = 0;
  actCueSheetEntry_ = NULL;
  writeCatalog_ = 0;

  for (ent = cueSheet_, i = 0; i < cueSheetLen_; ent++, i++) {

    switch (ent->ctlAdr & 0x0f) {
    case 1:
      if (ent->min > 99 || ent->sec > 59 || ent->frame > 74) {
	log_message(-3, "Illegal time field value at cue sheet entry: %d",
		i);
	return 1;
      }
	
      if (ent->trackNr == 0) { // indicates lead-in
	if (i == 0 || (writeCatalog_ && i == 2)) { // must be first entry
	  actCueSheetEntry_ = ent;
	}
	else {
	  log_message(-3, "Illegal track number at cue sheet entry: %d", i);
	  return 1;
	}
      }
      else if (ent->trackNr == 0xaa) { // indicates lead-out
	if (i == cueSheetLen_ - 1) { // must be last entry
	  leadOutStart_ = Msf(ent->min, ent->sec, ent->frame);
	  leadOutCtlAdr_ = ent->ctlAdr;
	}
	else {
	  log_message(-3, "Illegal track number at cue sheet entry: %d", i);
	  return 1;
	}
      }
      else if (ent->trackNr <= 99) { // data track
	if (firstTrackNr_ == 0) {
	  firstTrackNr_ = ent->trackNr;
	  firstTrackCtlAdr_ = ent->ctlAdr;
	  prevTrackNr = ent->trackNr;
	}
	else {
	  if (ent->trackNr != prevTrackNr && ent->trackNr != prevTrackNr + 1) {
	    log_message(-3, 
		    "Wrong track number sequence at cue sheet entry: %d", i);
	    return 1;
	  }
	  prevTrackNr = ent->trackNr;
	}
	lastTrackNr_ = ent->trackNr;
	lastTrackCtlAdr_ = ent->ctlAdr;
      }
      else {
	log_message(-3, "Illegal track number at cue sheet entry: %d", i);
	return 1;
      }

      if (ent->trackNr != 0) {
	lba = Msf(ent->min, ent->sec, ent->frame).lba();
	if (lba <= prevLba) {
	  log_message(-3, 
		  "Time field does not increase at cue sheet entry: %d", i);
	  return 1;
	}
	prevLba = lba;
      }
      break;

    case 2:
      if (i != 0) {
	log_message(-3, "Catalog number must be first cue sheet entry.");
	return 1;
      }
      if ((cueSheet_[1].ctlAdr & 0x0f) != 2) {
	log_message(-3, "Missing second catalog number entry.");
	return 1;
      }
      writeCatalog_ = 1;
      catalog_->catalog(ent->trackNr, ent->indexNr, ent->dataForm, ent->scms,
			ent->min, ent->sec, ent->frame, cueSheet_[1].trackNr,
			cueSheet_[1].indexNr, cueSheet_[1].dataForm,
			cueSheet_[1].scms, cueSheet_[1].min, cueSheet_[1].sec);
      // skip next entry
      ent++;
      i++;
      break;

    case 3:
      if (((ent + 1)->ctlAdr & 0x0f) != 3) {
	log_message(-3, "Missing second ISRC code entry.");
	return 1;
      }

      // skip next entry
      ent++;
      i++;
      break;
    default:
      log_message(-3, "Illegal adr field at cue sheet entry: %d.", i);
      return 1;
      break;
    }
  }

  if (actCueSheetEntry_ == NULL) {
    log_message(-3, "Cue sheet contains no lead-in entry.");
    return 1;
  }

  if (leadOutStart_.lba() == 0) {
    log_message(-3, "Cue sheet contains no lead-out entry.");
    return 1;
  }

  if (firstTrackNr_ == 0) {
    log_message(-3, "Cue sheet contains no data track.");
    return 1;
  }

  return 0;
}


void PQChannelEncoder::encode(long lba, unsigned char *out, long blocks)
{
  long clen = subChannel_->dataLength();
  const SubChannel *chan;
  long i;
  
  for (i = 0; i < blocks; i++, lba++) {
    chan = encodeSubChannel(lba);
    memcpy(out, chan->data(), clen);
    out += clen;
  }
}



const SubChannel *PQChannelEncoder::encodeSubChannel(long lba)
{
  int newTransition = 0;
  SubChannel *chan = NULL;
  
  // check consistency of internal lba with external lba
  assert(lba == abslba_);

  if (lba == nextTransitionLba_) {
    // switch to next transition
    //log_message(3, "Switching to next transition at lba: %ld", lba);
    nextTransition();
    newTransition = 1;
  }

  if (actCueSheetEntry_->trackNr == 0) {
    // lead-in sector
    chan = toc_[actTocEnt_];

    Msf m(trlba_);
    chan->min(m.min());
    chan->sec(m.sec());
    chan->frame(m.frac());

    // advance to next to entry
    if (++actTocCount_ == 3) {
      actTocCount_ = 0;
      if (++actTocEnt_ == tocLen_) {
	actTocEnt_ = 0;
      }
    }
  }
  else {
    // data or lead-out sector

    // Q channel setup

    // catalog number
    if (writeCatalog_ && 
	(deferredCatalog_ || (abslba_ % 90) == 0)) {
      if (newTransition) {
	deferredCatalog_ = 1;
      }
      else {
	deferredCatalog_ = 0;
	chan = catalog_;
	chan->aframe(Msf(abslba_ + 150).frac());
      }
    }

    // ISRC code
    if (writeIsrc_ && 
	(deferredIsrc_ || (abslba_ % 90) == 50 || (abslba_ % 90) == -40)) {
      if (newTransition) {
	deferredIsrc_ = 1;
      }
      else {
	deferredIsrc_ = 0;
	chan = isrc_;
	chan->aframe(Msf(abslba_ + 150).frac());
      }
    }

    if (chan == NULL) {
      // Current position
      chan = current_;

      Msf m(trlba_ < 0 ? -trlba_ : trlba_);
      chan->min(m.min());
      chan->sec(m.sec());
      chan->frame(m.frac());
      
      m = Msf(abslba_ + 150);
      chan->amin(m.min());
      chan->asec(m.sec());
      chan->aframe(m.frac());
    }


    // P channel setup
    if (trlba_ <= 0 || 
	(abslba_ >= nextTrackStartLba_ - 150 && 
	 abslba_ <= nextTrackStartLba_)) {
      // set P channel flag in pre-gap and 2 seconds before next track starts
      chan->pChannel(1);
    }
    else if (abslba_ >= leadOutStart_.lba() ) {
      // P channel flag is 0 2 secs from start of lead-out, after that
      // flag alternates at 2 Hz.
      chan->pChannel(((trlba_ - 150) % 38) < 19 ? 1 : 0);
    }
    else {
      chan->pChannel(0);
    }
  }

  abslba_++;
  trlba_++;

  assert(chan != NULL);
  chan->calcCrc();

  return chan;
}

void PQChannelEncoder::nextTransition()
{
  CueSheetEntry *nextEnt, *ent , *ent1;

  nextEnt = nextCueSheetEntry(actCueSheetEntry_, 1);
  assert(nextEnt != NULL);

  if (nextEnt->trackNr != actCueSheetEntry_->trackNr) {
    // new track started

    // check for ISRC code cue sheet entry
    if ((ent = nextCueSheetEntry(actCueSheetEntry_, 3)) != NULL &&
	ent->trackNr == nextEnt->trackNr) {
      ent1 = ent + 1;
      isrc_->isrc(ent->indexNr, ent->dataForm, ent->scms, ent->min, ent->sec,
		  ent->frame, ent1->indexNr, ent1->dataForm, ent1->scms,
		  ent1->min, ent1->sec, ent1->frame);
      writeIsrc_ = 1;
    }
    else {
      writeIsrc_ = 0;
    }
    deferredIsrc_ = 0;

    // setup 'trlba'
    if (nextEnt->indexNr == 0) {
      // track has a pre-gap -> determine length
      ent = nextCueSheetEntry(nextEnt, nextEnt->trackNr, 1);
      assert(ent != NULL);
      
      trlba_ = Msf(nextEnt->min, nextEnt->sec, nextEnt->frame).lba() -
	Msf(ent->min, ent->sec, ent->frame).lba();
    }
    else {
      trlba_ = 0;
    }
  }
    
  actCueSheetEntry_ = nextEnt;
  
  current_->ctl(actCueSheetEntry_->ctlAdr & 0xf0);
  current_->trackNr(actCueSheetEntry_->trackNr);
  current_->indexNr(actCueSheetEntry_->indexNr);

  if (writeCatalog_)
    catalog_->ctl(actCueSheetEntry_->ctlAdr & 0xf0);

  if (writeIsrc_)
    isrc_->ctl(actCueSheetEntry_->ctlAdr & 0xf0);

  if (actCueSheetEntry_->trackNr != 0xaa) {
    // find next transition point

    ent = nextCueSheetEntry(actCueSheetEntry_, 1);
    assert(ent != NULL);
    nextTransitionLba_ = Msf(ent->min, ent->sec, ent->frame).lba() - 150;

    if (actCueSheetEntry_->indexNr == 1) {
      // find next track start lba
      if (actCueSheetEntry_->trackNr == lastTrackNr_)
	ent = nextCueSheetEntry(actCueSheetEntry_, 0xaa, 1);
      else
	ent = nextCueSheetEntry(actCueSheetEntry_,
				actCueSheetEntry_->trackNr + 1, 1);
      assert(ent != NULL);

      nextTrackStartLba_ = Msf(ent->min, ent->sec, ent->frame).lba() - 150;
    }
  }
}

CueSheetEntry *PQChannelEncoder::nextCueSheetEntry(CueSheetEntry *act,
						   int adr)
{
  if (act->trackNr == 0xaa) {
    return NULL;
  }

  act++;

  while (act->ctlAdr != 0) {
    if ((act->ctlAdr & 0x0f) == adr) {
      return act;
    }
    act++;
  }

  return NULL;
}

CueSheetEntry *PQChannelEncoder::nextCueSheetEntry(CueSheetEntry *act,
						   int trackNr, int indexNr)
{
  if (act->trackNr == 0xaa) {
    return NULL;
  }

  act++;

  while (act->ctlAdr != 0) {
    if ((act->ctlAdr & 0x0f) == 1 && act->trackNr == trackNr &&
	act->indexNr == indexNr) {
      return act;
    }
    act++;
  }

  return NULL;
}


