/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000 Andreas Mueller <mueller@daneb.ping.de>
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

#include "TocEditView.h"

#include <stddef.h>

#include "guiUpdate.h"
#include "TocEdit.h"

TocEditView::TocEditView(TocEdit *t)
{
  tocEdit_ = t;

  sampleMarkerValid_ = 0;
  sampleSelectionValid_ = 0;
  sampleViewMin_ = sampleViewMax_ = 0;
  trackSelectionValid_ = 0;
  indexSelectionValid_ = 0;
}

TocEditView::~TocEditView()
{
  tocEdit_ = 0;
}

TocEdit *TocEditView::tocEdit() const
{
  return tocEdit_;
}


void TocEditView::sampleMarker(unsigned long sample)
{
  if (sample < tocEdit_->toc()->length().samples()) {
    sampleMarker_ = sample;
    sampleMarkerValid_ = 1;
  }
  else {
    sampleMarkerValid_ = 0;
  }

  tocEdit_->updateLevel_ |= UPD_SAMPLE_MARKER;
}

int TocEditView::sampleMarker(unsigned long *sample) const
{
  if (sampleMarkerValid_)
    *sample = sampleMarker_;

  return sampleMarkerValid_;
}

void TocEditView::sampleSelection(unsigned long smin, unsigned long smax)
{
  unsigned long tmp;

  if (smin > smax) {
    tmp = smin;
    smin = smax;
    smax = tmp;
  }

  if (smax < tocEdit_->toc()->length().samples()) {
    sampleSelectionMin_ = smin;
    sampleSelectionMax_ = smax;

    sampleSelectionValid_ = 1;
  }
  else {
    sampleSelectionValid_ = 0;
  }
  
  tocEdit_->updateLevel_ |= UPD_SAMPLE_SEL;
}

void TocEditView::sampleSelectionClear()
{
  if (sampleSelectionValid_)
    tocEdit_->updateLevel_ |= UPD_SAMPLE_SEL;

  sampleSelectionValid_ = 0;
}

int TocEditView::sampleSelection(unsigned long *smin, unsigned long *smax) const
{
  if (sampleSelectionValid_) {
    *smin = sampleSelectionMin_;
    *smax = sampleSelectionMax_;
  }

  return sampleSelectionValid_;
}

void TocEditView::sampleView(unsigned long smin, unsigned long smax)
{
  if (smin <= smax && smax < tocEdit_->lengthSample()) {
    sampleViewMin_ = smin;
    sampleViewMax_ = smax;
    tocEdit_->updateLevel_ |= UPD_SAMPLES;
  }
}

void TocEditView::sampleView(unsigned long *smin, unsigned long *smax) const
{
  *smin = sampleViewMin_;
  *smax = sampleViewMax_;
}

void TocEditView::sampleViewFull()
{
  sampleViewMin_ = 0;

  if ((sampleViewMax_ = tocEdit_->lengthSample()) > 0)
    sampleViewMax_ -= 1;

  tocEdit_->updateLevel_ |= UPD_SAMPLES;
}

void TocEditView::sampleViewUpdate()
{
  if (sampleViewMax_ >= tocEdit_->lengthSample()) {
    unsigned long len = sampleViewMax_ - sampleViewMin_;

    if ((sampleViewMax_ = tocEdit_->lengthSample()) > 0)
      sampleViewMax_ -= 1;
    

    if (sampleViewMax_ >= len)
      sampleViewMin_ = sampleViewMax_ - len;
    else
      sampleViewMin_ = 0;

    tocEdit_->updateLevel_ |= UPD_SAMPLES;
  }
}

void TocEditView::sampleViewInclude(unsigned long smin, unsigned long smax)
{
  if (smin < sampleViewMin_) {
    sampleViewMin_ = smin;
    tocEdit_->updateLevel_ |= UPD_SAMPLES;
  }

  if (smax < tocEdit_->lengthSample() && smax > sampleViewMax_) {
    sampleViewMax_ = smax;
    tocEdit_->updateLevel_ |= UPD_SAMPLES;
  }
}

void TocEditView::trackSelection(int tnum)
{
  if (tnum > 0) {
    trackSelection_ = tnum;
    trackSelectionValid_ = 1;
  }
  else {
    trackSelectionValid_ = 0;
  }

  tocEdit_->updateLevel_ |= UPD_TRACK_MARK_SEL;

}

int TocEditView::trackSelection(int *tnum) const
{
  if (trackSelectionValid_)
    *tnum = trackSelection_;

  return trackSelectionValid_;
}

void TocEditView::indexSelection(int inum)
{
  if (inum >= 0) {
    indexSelection_ = inum;
    indexSelectionValid_ = 1;
  }
  else {
    indexSelectionValid_ = 0;
  }

  tocEdit_->updateLevel_ |= UPD_TRACK_MARK_SEL;
}

int TocEditView::indexSelection(int *inum) const
{
  if (indexSelectionValid_)
    *inum = indexSelection_;

  return indexSelectionValid_;
}
