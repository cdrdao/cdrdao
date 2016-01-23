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

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <iostream>

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "config.h"
#include "TocEdit.h"
#include "SampleDisplay.h"
#include "SampleManager.h"
#include "TrackManager.h"

#include "Toc.h"
#include "util.h"
#include "log.h"

/* XPM data for track marker */
#define TRACK_MARKER_XPM_WIDTH 9
#define TRACK_MARKER_XPM_HEIGHT 12
static const gchar *TRACK_MARKER_XPM_DATA[] =
    { "9 12 3 1", "       c None", "X      c #000000000000",
        ".      c #FFFFFFFFFFFF", "         ", " XXXXXXX ", " XXXXXXX ",
        " XXXXXXX ", " XXXXXXX ", " XXXXXXX ", " XXXXXXX ", "  XXXXX  ",
        "   XXX   ", "    X    ", "    X    ", "    X    " };

/* XPM data for index marker */
static const gchar *INDEX_MARKER_XPM_DATA[] =
    { "9 12 3 1", "       c None", "X      c #000000000000",
        ".      c #FFFFFFFFFFFF", "         ", " XXXXXXX ", " X.....X ",
        " X.....X ", " X.....X ", " X.....X ", " X.....X ", "  X...X  ",
        "   X.X   ", "    X    ", "    X    ", "    X    " };

/* XPM data for extend track marker */
static const gchar *TRACK_EXTEND_XPM_DATA[] =
    { "9 12 3 1", "       c None", "X      c #000000000000",
        ".      c #FFFFFFFFFFFF", "......XX.", ".....XXX.", "....XXXX.",
        "...XXXXX.", "..XXXXXX.", ".XXXXXXX.", "..XXXXXX.", "...XXXXX.",
        "....XXXX.", ".....XXX.", "......XX.", "........." };

/* XPM data for extend track marker */
static const gchar *INDEX_EXTEND_XPM_DATA[] =
    { "9 12 3 1", "       c None", "X      c #000000000000",
        ".      c #FFFFFFFFFFFF", "......XX.", ".....X.X.", "....X..X.",
        "...X...X.", "..X....X.", ".X.....X.", "..X....X.", "...X...X.",
        "....X..X.", ".....X.X.", "......XX.", "........." };

static void draw_line(const Cairo::RefPtr<Cairo::Context>& cr, int x1, int y1,
    int x2, int y2)
{
  cr->move_to(x1, y1);
  cr->line_to(x2, y2);
  cr->stroke();
}

SampleDisplay::SampleDisplay() :
    pixmap_(NULL), trackMarkerPixmap_(NULL), indexMarkerPixmap_(NULL), trackMarkerSelectedPixmap_(
        NULL), indexMarkerSelectedPixmap_(NULL), trackExtendPixmap_(NULL), indexExtendPixmap_(
        NULL)
{
  adjustment_ = Gtk::Adjustment::create(0.0, 0.0, 1.0);
  adjustment_->signal_value_changed().connect(
      mem_fun(*this, &SampleDisplay::scrollTo));

  trackManager_ = new TrackManager(TRACK_MARKER_XPM_WIDTH);

  width_ = height_ = chanHeight_ = lcenter_ = rcenter_ = 0;
  timeLineHeight_ = timeLineY_ = 0;
  timeTickWidth_ = 0;
  timeTickSep_ = 20;
  sampleStartX_ = sampleEndX_ = sampleWidthX_ = 0;
  minSample_ = maxSample_ = resolution_ = 0;
  tocEdit_ = NULL;

  chanSep_ = 10;

  cursorControlExtern_ = false;
  drawCursor_ = false;
  cursorX_ = 0;

  markerSet_ = false;
  selectionSet_ = false;
  regionSet_ = false;
  dragMode_ = DRAG_NONE;

  pickedTrackMarker_ = NULL;
  selectedTrack_ = 0;
  selectedIndex_ = 0;

  signal_configure_event().connect(
      mem_fun(*this, &SampleDisplay::handleConfigureEvent));
  signal_motion_notify_event().connect(
      mem_fun(*this, &SampleDisplay::handleMotionNotifyEvent));
  signal_button_press_event().connect(
      mem_fun(*this, &SampleDisplay::handleButtonPressEvent));
  signal_button_release_event().connect(
      mem_fun(*this, &SampleDisplay::handleButtonReleaseEvent));
  signal_enter_notify_event().connect(
      mem_fun(*this, &SampleDisplay::handleEnterEvent));
  signal_leave_notify_event().connect(
      mem_fun(*this, &SampleDisplay::handleLeaveEvent));

  set_events(
      Gdk::EXPOSURE_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::ENTER_NOTIFY_MASK
          | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK
          | Gdk::POINTER_MOTION_MASK | Gdk::POINTER_MOTION_HINT_MASK);

  set_vexpand(true);
}

void SampleDisplay::setTocEdit(TocEdit *t)
{
  tocEdit_ = t;

  Toc *toc = tocEdit_->toc();

  markerSet_ = false;
  selectionSet_ = false;
  regionSet_ = false;

  minSample_ = 0;

  if (toc->length().samples() > 0) {
    maxSample_ = toc->length().samples() - 1;
  } else {
    maxSample_ = 0;
  }
}

void SampleDisplay::updateToc(unsigned long smin, unsigned long smax)
{
  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  if (smin <= smax) {
    minSample_ = smin;
    maxSample_ = smax;
  }

  if (toc->length().samples() == 0) {
    minSample_ = maxSample_ = 0;
  } else {
    if (maxSample_ >= toc->length().samples()) {
      // adjust 'maxSample_' to reduced length
      unsigned long len = maxSample_ - minSample_;

      maxSample_ = toc->length().samples() - 1;
      if (maxSample_ > len) {
        minSample_ = maxSample_ - len;
      } else {
        minSample_ = 0;
      }
    }
  }

  setView(minSample_, maxSample_);
}

void SampleDisplay::setView(unsigned long start, unsigned long end)
{
  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  if (end < start)
    end = start;

  unsigned long len = end - start + 1;

  if (len < 3 && toc != NULL) {
    end = start + 2;

    if (end >= toc->length().samples()) {
      if (toc->length().samples() != 0)
        end = toc->length().samples() - 1;
      else
        end = 0;

      if (end <= 2)
        start = 0;
      else
        start = end - 2;
    }

    len = (end == 0 ? 0 : 3);
  }

  minSample_ = start;
  maxSample_ = end;

  updateSamples();
  queue_draw();

  if (toc == NULL) {
    adjustment_->set_lower(0.0);
    adjustment_->set_upper(0.0);
    adjustment_->set_page_size(0.0);

    // important: set value after other values
    adjustment_->set_value(0.0);
  } else {
    adjustment_->set_lower(0.0);
    adjustment_->set_upper(toc->length().samples());

    adjustment_->set_step_increment(len / 4);

    if (adjustment_->get_step_increment() == 0.0)
      adjustment_->set_step_increment(1.0);

    adjustment_->set_page_increment(len / 1.1);
    adjustment_->set_page_size(len);

    // important: set value after other values
    adjustment_->set_value(minSample_);
  }
  adjustment_->changed();
}

void SampleDisplay::getView(unsigned long *start, unsigned long *end)
{
  *start = minSample_;
  *end = maxSample_;
}

bool SampleDisplay::getSelection(unsigned long *start, unsigned long *end)
{
  if (selectionSet_) {
    *start = selectionStartSample_;
    *end = selectionEndSample_;
    return true;
  }

  return false;
}

int SampleDisplay::getMarker(unsigned long *sample)
{
  if (markerSet_) {
    *sample = markerSample_;
    return 1;
  }

  return 0;
}

void SampleDisplay::setSelectedTrackMarker(int trackNr, int indexNr)
{
  selectedTrack_ = trackNr;
  selectedIndex_ = indexNr;
}

void SampleDisplay::setRegion(unsigned long start, unsigned long end)
{
  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  if (end <= start || end >= toc->length().samples()) {
    regionSet_ = false;
  } else {
    regionStartSample_ = start;
    regionEndSample_ = end;
    regionSet_ = true;
  }

  setView(minSample_, maxSample_);
}

void SampleDisplay::clearRegion()
{
  bool wasSet = regionSet_;
  regionSet_ = false;
  if (wasSet) {
    setView(minSample_, maxSample_);
  }
}

int SampleDisplay::getRegion(unsigned long *start, unsigned long *end)
{
  if (regionSet_) {
    *start = regionStartSample_;
    *end = regionEndSample_;
    return 1;
  }

  return 0;
}

void SampleDisplay::setCursor(int ctrl, unsigned long sample)
{
  if (ctrl == 0) {
    cursorControlExtern_ = false;
  } else {
    cursorControlExtern_ = true;

    gint x = sample2pixel(sample);
    if (x >= 0)
      drawCursor(x);
    else
      undrawCursor();
  }
}

void SampleDisplay::getColor(const char *colorName, Gdk::Color *color)
{
  if (!color->set(colorName)) {
    log_message(-1, _("Cannot allocate color \"%s\""), colorName);
  }
}

void SampleDisplay::scrollTo()
{
  unsigned long minSample, maxSample;

  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  if (adjustment_->get_page_size() == 0.0)
    return;

  minSample = (unsigned long) adjustment_->get_value();
  maxSample = (unsigned long) (adjustment_->get_value()
      + adjustment_->get_page_size()) - 1;

  if (maxSample >= toc->length().samples()) {
    maxSample = toc->length().samples() - 1;
    if (maxSample <= (unsigned long) (adjustment_->get_page_size() - 1))
      minSample = 0;
    else
      minSample = maxSample
          - (unsigned long) (adjustment_->get_page_size() - 1);
  }

  viewModified(minSample, maxSample);
}

unsigned long SampleDisplay::pixel2sample(gint x)
{
  if (tocEdit_ == NULL)
    return 0;

  Toc *toc = tocEdit_->toc();
  unsigned long sample;

  if (toc->length().lba() == 0)
    return 0;

  assert(x >= sampleStartX_ && x <= sampleEndX_);

  x -= sampleStartX_;

  double res = maxSample_ - minSample_;
  res /= sampleWidthX_ - 1;

  sample = (unsigned long) (minSample_ + res * x + 0.5);

  unsigned long round = 75 * 588; // 1 second
  unsigned long rest;

  if (res >= 2 * round) {
    if ((rest = sample % round) != 0)
      sample += round - rest;
  } else {
    round = 588; // 1 block
    if (res >= 2 * round) {
      if ((rest = sample % round) != 0)
        sample += round - rest;
    }
  }

  if (sample > maxSample_)
    sample = maxSample_;

  return sample;
}

gint SampleDisplay::sample2pixel(unsigned long sample)
{
  if (sample < minSample_ || sample > maxSample_)
    return -1;

  unsigned long len = maxSample_ - minSample_;
  double val = sample - minSample_;

  val *= sampleWidthX_ - 1;
  val /= len;

  return (gint) (sampleStartX_ + val + 0.5);
}

bool SampleDisplay::handleConfigureEvent(GdkEventConfigure *event)
{
  getColor("darkslateblue", &sampleColor_);
  getColor("red3", &middleLineColor_);
  getColor("gold2", &cursorColor_);
  getColor("red", &markerColor_);
  getColor("#ffc0e0", &selectionBackgroundColor_);

  Glib::RefPtr<Pango::Context> context = get_pango_context();
  Pango::FontMetrics metrics = context->get_metrics(
      get_style_context()->get_font());

  timeLineHeight_ = ((metrics.get_ascent() + metrics.get_descent())
      / Pango::SCALE);
  trackLineHeight_ = ((metrics.get_ascent() + metrics.get_descent())
      / Pango::SCALE);

  trackMarkerPixmap_ = Gdk::Pixbuf::create_from_xpm_data(TRACK_MARKER_XPM_DATA);
  indexMarkerPixmap_ = Gdk::Pixbuf::create_from_xpm_data(INDEX_MARKER_XPM_DATA);
  trackMarkerSelectedPixmap_ = Gdk::Pixbuf::create_from_xpm_data(
      TRACK_MARKER_XPM_DATA);
  indexMarkerSelectedPixmap_ = Gdk::Pixbuf::create_from_xpm_data(
      INDEX_MARKER_XPM_DATA);
  trackExtendPixmap_ = Gdk::Pixbuf::create_from_xpm_data(TRACK_EXTEND_XPM_DATA);
  indexExtendPixmap_ = Gdk::Pixbuf::create_from_xpm_data(INDEX_EXTEND_XPM_DATA);

  trackMarkerWidth_ = ((metrics.get_approximate_digit_width() / Pango::SCALE)
      * 5) + TRACK_MARKER_XPM_WIDTH + 2;

  width_ = get_width();
  height_ = get_height();

  // Don't even try to do anything smart if we haven't received a
  // reasonable window size yet. This will keep pixmap_ to NULL. This
  // is important because during startup we don't control how the
  // configure_event are timed wrt to gcdmaster bringup.
  if (width_ <= 1 || height_ <= 1)
    return true;

  chanHeight_ = (height_ - timeLineHeight_ - trackLineHeight_ - 2) / 2;

  lcenter_ = chanHeight_ / 2 + trackLineHeight_;
  rcenter_ = lcenter_ + timeLineHeight_ + chanHeight_;

  trackLineY_ = trackLineHeight_ - 1;

  timeLineY_ = chanHeight_ + timeLineHeight_ + trackLineHeight_;
  timeTickWidth_ = ((metrics.get_approximate_digit_width() / Pango::SCALE) * 13)
      + 3;

  sampleStartX_ = 10;
  sampleEndX_ = width_ - 10;
  sampleWidthX_ = sampleEndX_ - sampleStartX_ + 1;

  if (width_ > 100 && height_ > 100)
    updateSamples();

  return true;
}

bool SampleDisplay::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if (!pixmap_)
    return true;

  cr->save();

  cr->set_antialias(Cairo::ANTIALIAS_NONE);

  Gdk::Cairo::set_source_pixbuf(cr, pixmap_, 0, 0);
  cr->paint();

  if (drawCursor_)
  {
    cr->set_line_width(1.0);
    Gdk::Cairo::set_source_color(cr, cursorColor_);
    draw_line(cr, cursorX_, trackLineY_, cursorX_, height_ - 1);
  }
  drawMarker(cr);

  drawSelection(cr);

  cr->restore();

  return true;
}

bool SampleDisplay::handleButtonPressEvent(GdkEventButton *event)
{
  gint x = (gint) event->x;
  gint y = (gint) event->y;

  dragMode_ = DRAG_NONE;

  // e.g. if audio is playing
  if (cursorControlExtern_)
    return true;

  if (event->button == 1 && x >= sampleStartX_ && x <= sampleEndX_) {
    if (y > trackLineY_) {
      dragMode_ = DRAG_SAMPLE_MARKER;
      selectionStart_ = x;
      selectionEnd_= x;
    } else {
      if ((pickedTrackMarker_ = trackManager_->pick(x - sampleStartX_ + 4,
          &dragStopMin_, &dragStopMax_)) != NULL) {
        dragMode_ = DRAG_TRACK_MARKER;
        dragStart_ = dragEnd_ = x;
        dragLastX_ = -1;
        dragStopMin_ += sampleStartX_;
        dragStopMax_ += sampleStartX_;
      }
    }
  }

  return true;
}

bool SampleDisplay::handleButtonReleaseEvent(GdkEventButton *event)
{
  gint x = (gint) event->x;

  if (cursorControlExtern_)
    return false;

  if (x < sampleStartX_) {
    x = sampleStartX_;
  } else if (x > sampleEndX_) {
    x = sampleEndX_;
  }

  if (event->button == 1 && dragMode_ != DRAG_NONE) {
    if (dragMode_ == DRAG_SAMPLE_MARKER) {
      if (selectionStart_ - x >= -5 && selectionStart_ - x <= 5) {
        selectionSet_ = false;
        selectionCleared();
        markerSet(pixel2sample(selectionStart_));
      } else {
        selectionSet_ = true;
        selectionEnd_ = x;
        if (selectionStart_ > selectionEnd_) {
          std::swap(selectionStart_, selectionEnd_);
        }
        selectionStartSample_ = pixel2sample(selectionStart_);
        selectionEndSample_ = pixel2sample(selectionEnd_);

        selectionSet(selectionStartSample_, selectionEndSample_);
      }
    } else if (dragMode_ == DRAG_TRACK_MARKER) {
      if (dragStart_ - x >= -5 && dragStart_ - x <= 5) {
        trackManager_->select(pickedTrackMarker_);
        selectedTrack_ = pickedTrackMarker_->trackNr;
        selectedIndex_ = pickedTrackMarker_->indexNr;
        trackMarkSelected(pickedTrackMarker_->track, selectedTrack_,
            selectedIndex_);
      } else {
        selectedTrack_ = pickedTrackMarker_->trackNr;
        selectedIndex_ = pickedTrackMarker_->indexNr;
        trackMarkMoved(pickedTrackMarker_->track, selectedTrack_,
            selectedIndex_, pixel2sample(x));
      }
      pickedTrackMarker_ = NULL;
    }

    dragMode_ = DRAG_NONE;
    drawCursor(x);
    queue_draw();
  }

  return true;
}

bool SampleDisplay::handleMotionNotifyEvent(GdkEventMotion *event)
{
  gint x, y;
  GdkModifierType state;

  if (cursorControlExtern_)
    return TRUE;

  if (event->is_hint) {
    gdk_window_get_device_position(event->window, event->device, &x, &y,
        &state);
  } else {
    x = (gint) event->x;
    y = (gint) event->y;
    state = (GdkModifierType) event->state;
  }

  if (dragMode_ == DRAG_SAMPLE_MARKER) {

    // to delete previous selection
    queue_draw_area(std::min(selectionEnd_, selectionStart_) - 1, 0,
        abs(selectionEnd_ - selectionStart_) + 3, height_);

    // restrict x to samples
    x = std::max(x, sampleStartX_);
    x = std::min(x, sampleEndX_);
    selectionEnd_ = x;

    // to draw new selection
    queue_draw_area(std::min(selectionEnd_, selectionStart_) - 1, 0,
        abs(selectionEnd_ - selectionStart_) + 3, height_);

  } else if (dragMode_ == DRAG_TRACK_MARKER) {
    x = std::max(x, dragStopMin_);
    x = std::min(x, dragStopMax_);

   if (dragLastX_ > 0) {
      queue_draw_area(dragLastX_ - 4, trackLineY_ - trackLineHeight_, trackMarkerWidth_,
          trackLineHeight_);
    }
    dragLastX_ = x;
  }
  drawCursor(x);

  return TRUE;
}

void SampleDisplay::drawSelection(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if (dragMode_ == DRAG_SAMPLE_MARKER)
  {
    int xstart = std::min(selectionStart_, selectionEnd_);
    int width = abs(selectionEnd_ - selectionStart_);
    cr->set_source_rgba(0.0, 0.25, 0.5, 0.5);
    cr->rectangle(xstart, 0, width, height_ - 1);
    cr->fill();
  } else if (dragMode_ == DRAG_TRACK_MARKER) {
    drawTrackMarker(cr, 1, dragLastX_, pickedTrackMarker_->trackNr,
        pickedTrackMarker_->indexNr, 0, 0);
  }
}

bool SampleDisplay::handleEnterEvent(GdkEventCrossing *event)
{
  if (cursorControlExtern_)
    return true;

  drawCursor((gint) event->x);
  return true;
}

bool SampleDisplay::handleLeaveEvent(GdkEventCrossing *event)
{
  if (cursorControlExtern_)
    return true;

  undrawCursor();
  return true;
}

void SampleDisplay::drawMarker(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if (markerSet_) {
    cr->set_line_width(1.0);
    Gdk::Cairo::set_source_color(cr, markerColor_);

    markerX_ = sample2pixel(markerSample_);
    if (markerX_ >= 0)
      draw_line(cr, markerX_, trackLineY_, markerX_, height_ - 1);
  }
}

void SampleDisplay::setMarker(unsigned long sample)
{
  if (markerSet_)
    queue_draw_area(markerX_-1, 0, 3, height_);

  markerSample_ = sample;
  markerSet_ = true;
}

void SampleDisplay::clearMarker()
{
  if (markerSet_)
  {
    markerSet_ = false;
    queue_draw_area(markerX_-1, 0, 3, height_);
  }
}

void SampleDisplay::updateSamples()
{
  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  Cairo::RefPtr<Cairo::ImageSurface> imSur = Cairo::ImageSurface::create(
      Cairo::FORMAT_RGB24, width_, height_);
  Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(imSur);

  gint halfHeight = chanHeight_ / 2;

  cr->set_source_rgb(1.0, 1.0, 1.0);
  cr->paint();

  cr->translate(0.5, 0.5);
  cr->set_line_width(1.0);
  cr->set_antialias(Cairo::ANTIALIAS_NONE);

  long res = (maxSample_ - minSample_ + 1) / sampleWidthX_;
  long bres = res / tocEdit_->sampleManager()->blocking();
  gint i;
  double pos;
  long j;
  unsigned long s;
  short lnegsum, lpossum, rnegsum, rpossum;
  gint regionStart = -1;
  gint regionEnd = -1;
  int regionActive = 0;

  if (regionSet_) {
    if (regionStartSample_ <= maxSample_ && regionEndSample_ >= minSample_) {

      if (regionStartSample_ > minSample_)
        regionStart = sample2pixel(regionStartSample_);
      else
        regionStart = sampleStartX_;

      if (regionEndSample_ < maxSample_)
        regionEnd = sample2pixel(regionEndSample_);
      else
        regionEnd = sampleEndX_;
    }

    if (regionStart >= 0 && regionEnd >= regionStart) {
      Gdk::Cairo::set_source_color(cr, selectionBackgroundColor_);
      cr->rectangle(regionStart, lcenter_ - halfHeight,
          regionEnd - regionStart + 1, chanHeight_);
      cr->fill();
      cr->rectangle(regionStart, rcenter_ - halfHeight,
          regionEnd - regionStart + 1, chanHeight_);
      cr->fill();
    }
  }

  Gdk::Cairo::set_source_color(cr, sampleColor_);

  if (bres > 0) {
    for (s = minSample_, i = sampleStartX_; s < maxSample_ && i <= sampleEndX_;
        s += res, i++) {
      lnegsum = lpossum = rnegsum = rpossum = 0;

      if (regionStart != -1 && i >= regionStart && regionActive == 0) {
        regionActive = 1;
        Gdk::Cairo::set_source_color(cr, markerColor_);
      } else if (regionActive == 1 && i > regionEnd) {
        regionActive = 2;
        Gdk::Cairo::set_source_color(cr, sampleColor_);
      }

      tocEdit_->sampleManager()->getPeak(s, s + res, &lnegsum, &lpossum,
          &rnegsum, &rpossum);

      pos = double(lnegsum) * halfHeight;
      pos /= SHRT_MAX;
      if (pos != 0)
        draw_line(cr, i, lcenter_, i, lcenter_ - (gint) pos);

      pos = double(lpossum) * halfHeight;
      pos /= SHRT_MAX;
      if (pos != 0)
        draw_line(cr, i, lcenter_, i, lcenter_ - (gint) pos);

      pos = double(rnegsum) * halfHeight;
      pos /= SHRT_MAX;
      if (pos != 0)
        draw_line(cr, i, rcenter_, i, rcenter_ - (gint) pos);

      pos = double(rpossum) * halfHeight;
      pos /= SHRT_MAX;
      if (pos != 0)
        draw_line(cr, i, rcenter_, i, rcenter_ - (gint) pos);
    }
  } else if (maxSample_ > 0 && res >= 1) {

    TocReader reader(toc);

    if (reader.openData() == 0) {
      Sample *sampleBuf = new Sample[tocEdit_->sampleManager()->blocking()];
      double dres = double(maxSample_ - minSample_ + 1) / double(sampleWidthX_);
      double ds;

      for (ds = minSample_, i = sampleStartX_;
          ds < maxSample_ && i <= sampleEndX_; ds += dres, i++) {

        lnegsum = lpossum = rnegsum = rpossum = 0;

        if (reader.seekSample((long) ds) == 0
            && reader.readSamples(sampleBuf, res) == res) {
          for (j = 0; j < res; j++) {
            if (sampleBuf[j].left() < lnegsum)
              lnegsum = sampleBuf[j].left();

            if (sampleBuf[j].left() > lpossum)
              lpossum = sampleBuf[j].left();

            if (sampleBuf[j].right() < rnegsum)
              rnegsum = sampleBuf[j].right();

            if (sampleBuf[j].right() > rpossum)
              rpossum = sampleBuf[j].right();
          }
        }

        if (regionStart != -1 && i >= regionStart && regionActive == 0) {
          regionActive = 1;
          Gdk::Cairo::set_source_color(cr, markerColor_);
        } else if (regionActive == 1 && i > regionEnd) {
          regionActive = 2;
          Gdk::Cairo::set_source_color(cr, sampleColor_);
        }

        pos = double(lnegsum) * halfHeight;
        pos /= SHRT_MAX;
        if (pos != 0)
          draw_line(cr, i, lcenter_, i, lcenter_ - (gint) pos);

        pos = double(lpossum) * halfHeight;
        pos /= SHRT_MAX;
        if (pos != 0)
          draw_line(cr, i, lcenter_, i, lcenter_ - (gint) pos);

        pos = double(rnegsum) * halfHeight;
        pos /= SHRT_MAX;
        if (pos != 0)
          draw_line(cr, i, rcenter_, i, rcenter_ - (gint) pos);

        pos = double(rpossum) * halfHeight;
        pos /= SHRT_MAX;
        if (pos != 0)
          draw_line(cr, i, rcenter_, i, rcenter_ - (gint) pos);
      }

      delete[] sampleBuf;
      reader.closeData();
    }
  } else if (toc != NULL && maxSample_ > minSample_ + 1) {

    TocReader reader(toc);

    if (reader.openData() == 0) {
      long len = maxSample_ - minSample_ + 1;
      Sample *sampleBuf = new Sample[len];

      double pres = double(sampleWidthX_ - 1) / double(len - 1);
      double di;
      gint pos1;
      gint lastPosLeft, lastPosRight;

      if (reader.seekSample(minSample_) == 0
          && reader.readSamples(sampleBuf, len) == len) {

        for (j = 1, di = sampleStartX_ + pres; j < len && di < sampleEndX_ + 1;
            j++, di += pres) {
          if (regionStart != -1 && regionActive == 0
              && minSample_ + j - 1 >= regionStartSample_
              && minSample_ + j <= regionEndSample_) {
            regionActive = 1;
            Gdk::Cairo::set_source_color(cr, markerColor_);
          } else if (regionActive == 1 && minSample_ + j > regionEndSample_) {
            regionActive = 2;
            Gdk::Cairo::set_source_color(cr, sampleColor_);
          }

          pos = sampleBuf[j - 1].left() * halfHeight;
          pos /= SHRT_MAX;

          pos1 = sampleBuf[j].left() * halfHeight;
          pos1 /= SHRT_MAX;
          lastPosLeft = pos1;

          if (pos != 0 || pos1 != 0)
            draw_line(cr, long(di - pres), lcenter_ - (gint) pos, long(di),
                lcenter_ - pos1);

          pos = sampleBuf[j - 1].right() * halfHeight;
          pos /= SHRT_MAX;

          pos1 = sampleBuf[j].right() * halfHeight;
          pos1 /= SHRT_MAX;
          lastPosRight = pos1;

          if (pos != 0 || pos1 != 0)
            draw_line(cr, long(di - pres), rcenter_ - (gint) pos, long(di),
                rcenter_ - pos1);
        }

        if (&pixmap_ == 0)
          std::cout << "null !!" << std::endl;

        if (0 && (gint) di < sampleEndX_) {
          pos = sampleBuf[len - 1].left() * halfHeight;
          pos /= SHRT_MAX;
          if (pos != 0 || lastPosLeft != 0)
            draw_line(cr, long(di), lcenter_ - lastPosLeft, sampleEndX_,
                lcenter_ - (gint) pos);

          pos = sampleBuf[len - 1].right() * halfHeight;
          pos /= SHRT_MAX;
          if (pos != 0 || lastPosRight != 0)
            draw_line(cr, long(di), rcenter_ - lastPosRight, sampleEndX_,
                rcenter_ - (gint) pos);
        }
      }
      delete[] sampleBuf;
    }

  }

  Gdk::Cairo::set_source_color(cr, middleLineColor_);

  draw_line(cr, sampleStartX_, lcenter_, sampleEndX_, lcenter_);
  draw_line(cr, sampleStartX_, rcenter_, sampleEndX_, rcenter_);

  cr->set_source_rgb(0.0, 0.0, 0.0);

  draw_line(cr, sampleStartX_ - 1, lcenter_ - halfHeight, sampleEndX_ + 1,
      lcenter_ - halfHeight);
  draw_line(cr, sampleStartX_ - 1, lcenter_ + halfHeight, sampleEndX_ + 1,
      lcenter_ + halfHeight);
  draw_line(cr, sampleStartX_ - 1, lcenter_ - halfHeight, sampleStartX_ - 1,
      lcenter_ + halfHeight);
  draw_line(cr, sampleEndX_ + 1, lcenter_ - halfHeight, sampleEndX_ + 1,
      lcenter_ + halfHeight);

  draw_line(cr, sampleStartX_ - 1, rcenter_ - halfHeight, sampleEndX_ + 1,
      rcenter_ - halfHeight);
  draw_line(cr, sampleStartX_ - 1, rcenter_ + halfHeight, sampleEndX_ + 1,
      rcenter_ + halfHeight);
  draw_line(cr, sampleStartX_ - 1, rcenter_ + halfHeight, sampleStartX_ - 1,
      rcenter_ - halfHeight);
  draw_line(cr, sampleEndX_ + 1, rcenter_ + halfHeight, sampleEndX_ + 1,
      rcenter_ - halfHeight);

  drawTimeLine(cr);

  trackManager_->update(toc, minSample_, maxSample_, sampleWidthX_);
  if (selectedTrack_ > 0) {
    trackManager_->select(selectedTrack_, selectedIndex_);
  }
  drawTrackLine(cr);

  pixmap_ = Gdk::Pixbuf::create(imSur, 0, 0, width_, height_);
}

void SampleDisplay::drawCursor(gint x)
{
  if (x < sampleStartX_ || x > sampleEndX_)
    return;

  drawCursor_ = true;
  if (cursorX_ != x) {
    queue_draw_area(cursorX_-1, 0, 3, height_);
    cursorX_ = x;
    queue_draw_area(x-1, 0, 3, height_);
  }

  if (cursorControlExtern_ == false)
    cursorMoved(pixel2sample(x));
}

void SampleDisplay::undrawCursor()
{
  if (drawCursor_) {
    queue_draw_area(cursorX_-1, 0, 3, height_);
    drawCursor_ = false;
  }
}

void SampleDisplay::drawTimeTick(const Cairo::RefPtr<Cairo::Context>& cr,
    gint x, gint y, unsigned long sample)
{
  char buf[50];

  if (!pixmap_)
    return;

  unsigned long min = sample / (60 * 44100);
  sample %= 60 * 44100;

  unsigned long sec = sample / 44100;
  sample %= 44100;

  unsigned long frame = sample / 588;
  sample %= 588;

  sprintf(buf, "%lu:%02lu:%02lu.%03lu", min, sec, frame, sample);

  cr->set_source_rgb(0.0, 0.0, 0.0);
  draw_line(cr, x, y - timeLineHeight_, x, y);

  Glib::RefPtr<Pango::Layout> playout = create_pango_layout(buf);
  cr->move_to(x + 3, y - timeLineHeight_ + 1);
  playout->show_in_cairo_context(cr);
}

void SampleDisplay::drawTimeLine(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  if (toc->length().lba() == 0)
    return;

  gint sep = timeTickWidth_ + timeTickSep_;
  unsigned long maxNofTicks = (sampleWidthX_ + timeTickSep_) / sep;
  gint x;

  unsigned long len = maxSample_ - minSample_ + 1;
  unsigned long dt;
  unsigned long dtx;
  unsigned long startSample;
  unsigned long s;

  if ((s = len / (60 * 44100)) > 1) {
    dt = 60 * 44100;
  } else if ((s = len / 44100) > 1) {
    dt = 44100;
  } else if ((s = len / 588) > 1) {
    dt = 588;
  } else {
    dt = 1;
    s = len;
  }

  if (s > maxNofTicks) {
    dtx = s / maxNofTicks;
    if (s % maxNofTicks != 0)
      dtx++;
    dtx *= dt;
  } else {
    dtx = dt;
  }

  if (dt > 1) {
    if (minSample_ % dt == 0) {
      startSample = minSample_;
    } else {
      startSample = minSample_ / dt;
      startSample = startSample * dt + dt;
    }
  } else {
    startSample = minSample_;
  }

  for (s = startSample; s < maxSample_; s += dtx) {
    x = sample2pixel(s);

    if (x + timeTickWidth_ <= sampleEndX_)
      drawTimeTick(cr, x, timeLineY_, s);
  }
}

// Draws track marker.
// mode: 0: draw on 'pixmap_'
//       1: draw on window
void SampleDisplay::drawTrackMarker(const Cairo::RefPtr<Cairo::Context>& cr,
    int mode, gint x, int trackNr, int indexNr, int selected, int extend)
{
  char buf[20];

  sprintf(buf, "%d.%d", trackNr, indexNr);

  Glib::RefPtr<Gdk::Pixbuf> marker;

  if (extend) {
    marker = (indexNr == 1 ? trackExtendPixmap_ : indexExtendPixmap_);
  } else {
    if (selected)
      marker = (
          indexNr == 1 ?
              trackMarkerSelectedPixmap_ : indexMarkerSelectedPixmap_);
    else
      marker = (indexNr == 1 ? trackMarkerPixmap_ : indexMarkerPixmap_);
  }

  if (mode == 0) {
    if (selected)
      Gdk::Cairo::set_source_color(cr, markerColor_);
    else
      cr->set_source_rgb(1.0, 1.0, 1.0);

    cr->rectangle(x - 4, trackLineY_ - trackLineHeight_, trackMarkerWidth_,
        trackLineHeight_);
    cr->fill();
  }

  cr->set_source_rgb(0.0, 0.0, 0.0);

  cr->save();
  Gdk::Cairo::set_source_pixbuf(cr, marker, x - 4,
      trackLineY_ - TRACK_MARKER_XPM_HEIGHT);
  cr->rectangle(x - 4, trackLineY_ - TRACK_MARKER_XPM_HEIGHT,
      TRACK_MARKER_XPM_WIDTH, TRACK_MARKER_XPM_HEIGHT);
  cr->clip();
  cr->paint();
  cr->restore();

  Glib::RefPtr<Pango::Layout> playout = create_pango_layout(buf);
  cr->move_to(x + TRACK_MARKER_XPM_WIDTH / 2 + 2,
      trackLineY_ - trackLineHeight_ + 2);
  playout->show_in_cairo_context(cr);
}

void SampleDisplay::drawTrackLine(const Cairo::RefPtr<Cairo::Context>& cr)
{
  const TrackManager::Entry *run;
  const TrackManager::Entry *selected = NULL;

  for (run = trackManager_->first(); run != NULL; run = trackManager_->next()) {
    if (run->selected != 0 && run->extend == 0) {
      selected = run;
    } else if (run->indexNr != 1 || run->extend != 0) {
      drawTrackMarker(cr, 0, sampleStartX_ + run->xpos, run->trackNr,
          run->indexNr, 0, run->extend);
    }
  }

  for (run = trackManager_->first(); run != NULL; run = trackManager_->next()) {
    if (run->indexNr == 1 && run->selected == 0 && run->extend == 0) {
      drawTrackMarker(cr, 0, sampleStartX_ + run->xpos, run->trackNr,
          run->indexNr, 0, run->extend);
    }
  }

  if (selected != NULL)
    drawTrackMarker(cr, 0, sampleStartX_ + selected->xpos, selected->trackNr,
        selected->indexNr, 1, 0);
}

void SampleDisplay::updateTrackMarks()
{
  if (tocEdit_ == NULL)
    return;

  Toc *toc = tocEdit_->toc();

  trackManager_->update(toc, minSample_, maxSample_, sampleWidthX_);
  if (selectedTrack_ > 0) {
    trackManager_->select(selectedTrack_, selectedIndex_);
  }

  updateSamples();
  queue_draw_area(0, 0, width_, height_);
}
