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
/*
 * $Log: SampleManager.cc,v $
 * Revision 1.6  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.5.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.3  2004/01/01 00:04:58  denis
 * Made scan progress use the Gnome APP progressbar
 *
 * Revision 1.2  2003/12/29 09:31:48  denis
 * fixed all dialogs
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.5  2002/01/20 20:43:37  andreasm
 * Added support for sub-channel reading and writing.
 * Adapted to autoconf-2.52.
 * Adapted to gcc-3.0.
 *
 * Revision 1.4  2000/09/24 17:39:07  andreasm
 * Fixed length of processed data per idle signal call so that playback does
 * not jump when audio data scanning is active.
 *
 * Revision 1.3  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:52  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.6  1999/08/07 16:27:28  mueller
 * Applied patch from Yves Bastide:
 * * prefixing member function names with their class name in connect_to_method
 * * explicitly `const_cast'ing a cast to const
 *
 * Revision 1.5  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.4  1999/03/06 13:55:18  mueller
 * Adapted to Gtk-- version 0.99.1
 *
 * Revision 1.3  1999/01/30 19:45:43  mueller
 * Fixes for compilation with Gtk-- 0.11.1.
 *
 * Revision 1.1  1998/11/20 18:56:06  mueller
 * Initial revision
 *
 */

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include <gtkmm.h>
#include <gtk/gtk.h>

#include "SampleManager.h"

#include "TocEdit.h"
#include "guiUpdate.h"
#include "Toc.h"
#include "util.h"

#include "TrackDataScrap.h"

class SampleManagerImpl : public SigC::Object {
public:
  SampleManagerImpl(unsigned long);
  ~SampleManagerImpl();

  unsigned long blocking_;
  TocEdit *tocEdit_;
  TocReader tocReader_;

  short *leftNegSamples_;
  short *leftPosSamples_;
  short *rightNegSamples_;
  short *rightPosSamples_;
  long samplesSize_;
  long blocks_;
  unsigned long slength_;
  long chunk_;

  Sample *block_;
  long actBlock_;
  long endBlock_;
  long burstBlock_;
  unsigned long length_;
  gfloat percent_;
  gfloat percentStep_;

  bool withGui_;
  bool aborted_;

  Gtk::ProgressBar* progressBar_;
  Gtk::Button* abortButton_;
  
  void getPeak(unsigned long start, unsigned long end,
	       short *leftNeg, short *leftPos,
	       short *rightNeg, short *rightPos);
  int scanToc(unsigned long start, unsigned long end);
  bool readSamples();
  void abortAction();
  void reallocSamples(unsigned long maxSample);
  void removeSamples(unsigned long start, unsigned long end, TrackDataScrap *);
  void insertSamples(unsigned long pos, unsigned long len,
		     const TrackDataScrap *);
  void setProgressBar(Gtk::ProgressBar* bar) { progressBar_ = bar; }
  void setAbortButton(Gtk::Button*);
};

SampleManager::SampleManager(unsigned long blocking)
{
  impl_ = new SampleManagerImpl(blocking);
}

SampleManager::~SampleManager()
{
  delete impl_;
  impl_ = NULL;
}

unsigned long SampleManager::blocking() const
{
  return impl_->blocking_;
}

void SampleManager::setTocEdit(TocEdit *t)
{
  impl_->tocEdit_ = t;
  impl_->tocReader_.init(t->toc());

  impl_->blocks_ = 0;
}


int SampleManager::scanToc(unsigned long start, unsigned long end)
{
  return impl_->scanToc(start, end);
}

void SampleManager::getPeak(unsigned long start, unsigned long end,
			    short *leftNeg, short *leftPos,
			    short *rightNeg, short *rightPos)
{
  impl_->getPeak(start, end, leftNeg, leftPos, rightNeg, rightPos);
}

void SampleManager::removeSamples(unsigned long start, unsigned long end,
				  TrackDataScrap *scrap)
{
  impl_->removeSamples(start, end, scrap);
}

void SampleManager::insertSamples(unsigned long pos, unsigned long len,
				  const TrackDataScrap *scrap)
{
  impl_->insertSamples(pos, len, scrap);
}

void SampleManager::setProgressBar(Gtk::ProgressBar* b)
{
  impl_->setProgressBar(b);
}

void SampleManager::setAbortButton(Gtk::Button* b)
{
  impl_->setAbortButton(b);
}

SampleManagerImpl::SampleManagerImpl(unsigned long blocking) : tocReader_(NULL)
{
  blocking_ = blocking;
  tocEdit_ = NULL;

  leftNegSamples_ = leftPosSamples_ = NULL;
  rightNegSamples_ = rightPosSamples_ = NULL;
  samplesSize_ = 0;
  blocks_ = 0;
  slength_ = 0;

  block_ = new Sample[blocking_];
  actBlock_ = endBlock_ = burstBlock_ = 0;
  length_ = 0;
  withGui_ = false;

  // allocate space in chunks of 40 minutes
  chunk_ = 40 * 60 * 75 * 588 / blocking;

  progressBar_ = NULL;
  abortButton_ = NULL;
}

void SampleManagerImpl::setAbortButton(Gtk::Button* button)
{
  abortButton_ = button;
  button->signal_clicked().connect(slot(*this,
                                        &SampleManagerImpl::abortAction));
}

SampleManagerImpl::~SampleManagerImpl()
{
  delete[] block_;

  delete[] leftNegSamples_;
  delete[] leftPosSamples_;
  delete[] rightNegSamples_;
  delete[] rightPosSamples_;

  tocEdit_ = NULL;
}

void SampleManagerImpl::getPeak(unsigned long start, unsigned long end,
				short *leftNeg, short *leftPos,
				short *rightNeg, short *rightPos)
{
  *leftNeg = *leftPos = 0;
  *rightNeg = *rightPos = 0;

  long startBlock = start / blocking_;
  long endBlock = end / blocking_;
  long i;

  if (startBlock >= blocks_ || endBlock >= blocks_)
    return;

  for (i = startBlock; i <= endBlock; i++) {
    assert(leftNegSamples_[i] <= 0);
    assert(rightNegSamples_[i] <= 0);
    assert(leftPosSamples_[i] >= 0);
    assert(rightPosSamples_[i] >= 0);
	
    if (leftNegSamples_[i] < *leftNeg)
      *leftNeg = leftNegSamples_[i];
	
    if (leftPosSamples_[i] > *leftPos)
      *leftPos = leftPosSamples_[i];
	
    if (rightNegSamples_[i] < *rightNeg)
      *rightNeg = rightNegSamples_[i];
	
    if (rightPosSamples_[i] > *rightPos)
      *rightPos = rightPosSamples_[i];
  }
}


int SampleManagerImpl::scanToc(unsigned long start, unsigned long end)
{
  long i;
  const Toc *toc;

  actBlock_ = start / blocking_;
  endBlock_ = end / blocking_;

  if (tocEdit_ == NULL || endBlock_ < actBlock_)
    return 1;

  toc = tocEdit_->toc();

  length_ = toc->length().samples();

  if (end >= length_)
    return 1;

  length_ -= actBlock_ * blocking_;

  reallocSamples(end);

  for (i = actBlock_; i <= endBlock_; i++) {
    leftNegSamples_[i] = rightNegSamples_[i] = -16000;
    leftPosSamples_[i] = rightPosSamples_[i] = 16000;
  }

  //message(0, "readSamples: %ld\n", endBlock_ - actBlock_ + 1);
  
  if (tocReader_.openData() != 0)
    return 2;

  if (tocReader_.seekSample(actBlock_ * blocking_) != 0) {
    tocReader_.closeData();
    return 2;
  }

  long len = endBlock_ - actBlock_ + 1;

  if (len < 2000) {
    burstBlock_ = len;
    percentStep_ = 1.0;
    withGui_ = false;
  }
  else if (len < 10000) {
    burstBlock_ = len / 100;
    percentStep_ = 0.01;
    withGui_ = true;
  }
  else {
    burstBlock_ = 75;
    percentStep_ = gfloat(burstBlock_) / gfloat(len);
    withGui_ = true;
  }

  if (burstBlock_ == 0) 
    burstBlock_ = 1;

  percent_ = 0;
  aborted_ = false;

  if (withGui_) {
    if (progressBar_) progressBar_->set_fraction(0.0);
    if (abortButton_) abortButton_->set_sensitive(true);
    Glib::signal_idle().connect(slot(*this, &SampleManagerImpl::readSamples));
    tocEdit_->blockEdit();
  } else {
    while (readSamples());
  }

  return 0;
}

bool SampleManagerImpl::readSamples()
{
  int j;
  long n;
  short lpossum, rpossum, lnegsum, rnegsum;
  int ret;
  long burstEnd = actBlock_ + burstBlock_;

  if (withGui_ && aborted_) {
    tocReader_.closeData();
    if (abortButton_) abortButton_->set_sensitive(false);
    if (progressBar_) progressBar_->set_fraction(0.0);
    tocEdit_->unblockEdit();
    guiUpdate(UPD_SAMPLES);
    return 0;
  }

  for (; actBlock_ <= endBlock_ && actBlock_ < burstEnd && length_ > 0;
       actBlock_++) {
    n = length_ > blocking_ ? blocking_ : length_;
    if ((ret = tocReader_.readSamples(block_, n)) == n) {
      lpossum = lnegsum = rpossum = rnegsum = 0;
      for (j = 0; j < n; j++) {
	short d = block_[j].left();
	if (d > lpossum)
	  lpossum = d;
	if (d < lnegsum)
	  lnegsum = d;

	d = block_[j].right();
	if (d > rpossum)
	  rpossum = d;
	if (d < rnegsum)
	  rnegsum = d;
      }
      leftNegSamples_[actBlock_] = lnegsum;
      leftPosSamples_[actBlock_] = lpossum;
      rightNegSamples_[actBlock_] = rnegsum;
      rightPosSamples_[actBlock_] = rpossum;
    }
    else {
      message(-2, "Cannot read audio data: %ld - %ld.", n, ret);
      tocReader_.closeData();
      if (withGui_) {
        if (abortButton_) abortButton_->set_sensitive(false);
        if (progressBar_) progressBar_->set_fraction(0.0);
      }
      tocEdit_->unblockEdit();
      guiUpdate(UPD_SAMPLES);
      return 0;
    }
    length_ -= n;
  }

  
  if (actBlock_ >= endBlock_ && actBlock_ < burstEnd) {
    tocReader_.closeData();
    if (withGui_) {
      if (abortButton_) abortButton_->set_sensitive(false);
      if (progressBar_) progressBar_->set_fraction(0.0);
    }
    tocEdit_->unblockEdit();
    guiUpdate(UPD_SAMPLES);
    return 0;
  }

  percent_ += percentStep_;
  if (percent_ > 1.0) 
    percent_ = 1.0;

  if (withGui_ && progressBar_) {
    progressBar_->set_fraction(percent_);
  }

  return 1;
}

void SampleManagerImpl::abortAction()
{
  aborted_ = true;
}

void SampleManagerImpl::reallocSamples(unsigned long maxSample)
{
  long i;
  long maxBlock = (maxSample / blocking_) + 1;

  if (maxSample >= slength_)
    slength_ = maxSample + 1;

  if (maxBlock > blocks_)
    blocks_ = maxBlock;

  if (blocks_ > samplesSize_) {
    long newSize = samplesSize_ + chunk_;
    while (newSize < blocks_)
      newSize += chunk_;

    short *newLeftNeg = new short[newSize];
    short *newLeftPos = new short[newSize];
    short *newRightNeg = new short[newSize];
    short *newRightPos = new short[newSize];

    for (i = 0; i < samplesSize_; i++) {
      newLeftNeg[i] = leftNegSamples_[i];
      newLeftPos[i] = leftPosSamples_[i];
      newRightNeg[i] = rightNegSamples_[i];
      newRightPos[i] = rightPosSamples_[i];
    }
    samplesSize_ = newSize;

    delete[] leftNegSamples_;
    delete[] leftPosSamples_;
    delete[] rightNegSamples_;
    delete[] rightPosSamples_;
    leftNegSamples_ = newLeftNeg;
    leftPosSamples_ = newLeftPos;
    rightNegSamples_ = newRightNeg;
    rightPosSamples_ = newRightPos;
  }
}

void SampleManagerImpl::removeSamples(unsigned long start, unsigned long end,
				      TrackDataScrap *scrap)
{
  long i;
  long bstart;
  long oldBlocks = blocks_;
  long blen;
  unsigned long slen;

  if (start > end || end >= slength_)
    return;

  slen = end - start + 1;
  
  slength_ -= slen;

  if (slength_ == 0) {
    blocks_ = 0;
    return;
  }

  blocks_ = ((slength_ - 1) / blocking_) + 1;
  
  blen = oldBlocks - blocks_;
  
  bstart = start / blocking_;

  if (scrap != NULL)
    scrap->setPeaks(blen, &(leftNegSamples_[bstart]),
		    &(leftPosSamples_[bstart]), &(rightNegSamples_[bstart]),
		    &(rightPosSamples_[bstart]));

  if (blen > 0) {
    for (i = bstart; i < blocks_; i++) {
      leftNegSamples_[i] = leftNegSamples_[i + blen];
      leftPosSamples_[i] = leftPosSamples_[i + blen];
      rightNegSamples_[i] = rightNegSamples_[i + blen];
      rightPosSamples_[i] = rightPosSamples_[i + blen] ;
    }
  }
}

void SampleManagerImpl::insertSamples(unsigned long pos, unsigned long len,
				      const TrackDataScrap *scrap)
{
  long blen;
  long bpos;
  long oldBlocks;
  long i;

  if (len == 0)
    return;

  bpos = pos / blocking_;

  if (bpos > blocks_)
    bpos = blocks_;

  oldBlocks = blocks_;

  reallocSamples(slength_ + (len - 1));

  blen = blocks_ - oldBlocks;

  if (blen > 0) {
    for (i = blocks_ - 1; i >= bpos + blen; i--) {
      leftNegSamples_[i] = leftNegSamples_[i - blen];
      leftPosSamples_[i] = leftPosSamples_[i - blen];
      rightNegSamples_[i] = rightNegSamples_[i - blen];
      rightPosSamples_[i] = rightPosSamples_[i - blen] ;
    }

    // initialize the new region
    for (i = bpos; i < bpos + blen; i++) {
      leftNegSamples_[i] = -16000;
      leftPosSamples_[i] = 16000;
      rightNegSamples_[i] = -16000;
      rightPosSamples_[i] = 16000;
    }

    if (scrap != NULL)
      scrap->getPeaks(blen, &(leftNegSamples_[bpos]), &(leftPosSamples_[bpos]),
		      &(rightNegSamples_[bpos]), &(rightPosSamples_[bpos]));
  }
}
