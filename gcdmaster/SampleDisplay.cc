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
static const gchar *TRACK_MARKER_XPM_DATA[] = {
  "9 12 3 1",
  "       c None",
  "X      c #000000000000",
  ".      c #FFFFFFFFFFFF",
  "         ",
  " XXXXXXX ",
  " XXXXXXX ",
  " XXXXXXX ",
  " XXXXXXX ",
  " XXXXXXX ",
  " XXXXXXX ",
  "  XXXXX  ",
  "   XXX   ",
  "    X    ",
  "    X    ",
  "    X    "};

/* XPM data for index marker */
static const gchar *INDEX_MARKER_XPM_DATA[] = {
  "9 12 3 1",
  "       c None",
  "X      c #000000000000",
  ".      c #FFFFFFFFFFFF",
  "         ",
  " XXXXXXX ",
  " X.....X ",
  " X.....X ",
  " X.....X ",
  " X.....X ",
  " X.....X ",
  "  X...X  ",
  "   X.X   ",
  "    X    ",
  "    X    ",
  "    X    "};

/* XPM data for extend track marker */
static const gchar *TRACK_EXTEND_XPM_DATA[] = {
  "9 12 3 1",
  "       c None",
  "X      c #000000000000",
  ".      c #FFFFFFFFFFFF",
  "......XX.",
  ".....XXX.",
  "....XXXX.",
  "...XXXXX.",
  "..XXXXXX.",
  ".XXXXXXX.",
  "..XXXXXX.",
  "...XXXXX.",
  "....XXXX.",
  ".....XXX.",
  "......XX.",
  "........."};

/* XPM data for extend track marker */
static const gchar *INDEX_EXTEND_XPM_DATA[] = {
  "9 12 3 1",
  "       c None",
  "X      c #000000000000",
  ".      c #FFFFFFFFFFFF",
  "......XX.",
  ".....X.X.",
  "....X..X.",
  "...X...X.",
  "..X....X.",
  ".X.....X.",
  "..X....X.",
  "...X...X.",
  "....X..X.",
  ".....X.X.",
  "......XX.",
  "........."};


SampleDisplay::SampleDisplay() :
    trackMarkerPixmap_(NULL),
    indexMarkerPixmap_(NULL),
    trackMarkerSelectedPixmap_(NULL),
    indexMarkerSelectedPixmap_(NULL),
    trackExtendPixmap_(NULL),
    indexExtendPixmap_(NULL),
    sampleColor_("darkslateblue"),
    middleLineColor_("red3"),
    cursorColor_("gold2"),
    markerColor_("red"),
    selectionBackgroundColor_("#ffc0e0"),
    white_("white"),
    black_("black")
{
    adjustment_ = Gtk::Adjustment::create(0.0, 0.0, 1.0);
    adjustment_->signal_value_changed().connect(mem_fun(*this,
                                                        &SampleDisplay::scrollTo));

    trackManager_ = NULL;

    width_ = height_ = chanHeight_ = lcenter_ = rcenter_ = 0;
    timeLineHeight_ = timeLineY_ = 0;
    timeTickWidth_ = 0;
    timeTickSep_ = 20;
    sampleStartX_ = sampleEndX_ = sampleWidthX_ = 0;
    minSample_ = maxSample_ = resolution_ = 0;
    tocEdit_ = NULL;

    chanSep_ = 10;

    cursorControlExtern_ = false;
    cursorDrawn_ = false;
    cursorX_ = 0;

    markerSet_ = false;
    selectionSet_ = false;
    regionSet_ = false;
    dragMode_ = DRAG_NONE;

    pickedTrackMarker_ = NULL;
    selectedTrack_ = 0;
    selectedIndex_ = 0;

    signal_configure_event().
        connect(mem_fun(*this, &SampleDisplay::on_configure));
    signal_motion_notify_event().
        connect(mem_fun(*this, &SampleDisplay::on_motion_notify));
    signal_button_press_event().
        connect(mem_fun(*this, &SampleDisplay::on_button_press));
    signal_button_release_event().
        connect(mem_fun(*this,	&SampleDisplay::on_button_release));
    signal_enter_notify_event().
        connect(mem_fun(*this, &SampleDisplay::on_enter_notify));
    signal_leave_notify_event().
        connect(mem_fun(*this, &SampleDisplay::on_leave_notify));

    set_events(Gdk::EXPOSURE_MASK
               | Gdk::LEAVE_NOTIFY_MASK
               | Gdk::ENTER_NOTIFY_MASK
               | Gdk::BUTTON_PRESS_MASK
               | Gdk::BUTTON_RELEASE_MASK
               | Gdk::POINTER_MOTION_MASK
               | Gdk::POINTER_MOTION_HINT_MASK);

    trackMarkerPixmap_ =
        Gdk::Pixbuf::create_from_xpm_data(TRACK_MARKER_XPM_DATA);
    indexMarkerPixmap_ =
        Gdk::Pixbuf::create_from_xpm_data(INDEX_MARKER_XPM_DATA);
    trackMarkerSelectedPixmap_ =
            Gdk::Pixbuf::create_from_xpm_data(TRACK_MARKER_XPM_DATA);
    indexMarkerSelectedPixmap_ =
        Gdk::Pixbuf::create_from_xpm_data(INDEX_MARKER_XPM_DATA);
    trackExtendPixmap_ =
            Gdk::Pixbuf::create_from_xpm_data(TRACK_EXTEND_XPM_DATA);
    indexExtendPixmap_ =
        Gdk::Pixbuf::create_from_xpm_data(INDEX_EXTEND_XPM_DATA);

    trackManager_ = new TrackManager(TRACK_MARKER_XPM_WIDTH);
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
  }
  else {
    maxSample_ = 0;
  }
}

void SampleDisplay::updateToc(unsigned long smin, unsigned long smax)
{
  if (tocEdit_ == NULL)
    return;

  printf("UpdateToc %ld %ld\n", smin, smax);

  Toc *toc = tocEdit_->toc();

  if (smin <= smax) {
    minSample_ = smin;
    maxSample_ = smax;
  }

  if (toc->length().samples() == 0) {
    minSample_ = maxSample_ = 0;
  }
  else {
    if (maxSample_ >= toc->length().samples()) {
      // adjust 'maxSample_' to reduced length
      unsigned long len = maxSample_ - minSample_;

      maxSample_ = toc->length().samples() - 1;
      if (maxSample_ > len) {
	minSample_ = maxSample_ - len;
      }
      else {
	minSample_ = 0;
      }
    }
  }

  setView(minSample_, maxSample_);
}

void SampleDisplay::setView(unsigned long start, unsigned long end)
{
    printf("::setView %d %d\n", start, end);
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
    redraw(0, 0, width_, height_);

    if (toc == NULL) {
        adjustment_->configure(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    } else {
        adjustment_->configure(minSample_, 0.0, toc->length().samples(),
                               len / 4, len / 1.1, len);
    }
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

int SampleDisplay::get_marker(unsigned long *sample)
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
            set_cursor(x);
        else
            unset_cursor();
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

    minSample = (unsigned long)adjustment_->get_value();
    maxSample = (unsigned long)(adjustment_->get_value() +
                                adjustment_->get_page_size()) - 1;

    if (maxSample >= toc->length().samples()) {
        maxSample = toc->length().samples() - 1;
        if (maxSample <= (unsigned long)(adjustment_->get_page_size() - 1))
            minSample = 0;
        else
            minSample = maxSample - (unsigned long)(adjustment_->get_page_size() - 1);
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

  sample = (unsigned long)(minSample_ + res * x + 0.5);

  unsigned long round = 75 * 588; // 1 second
  unsigned long rest;

  if (res >=  2 * round) {
    if ((rest = sample % round) != 0)
      sample += round - rest;
  }
  else {
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

  return (gint)(sampleStartX_ + val + 0.5);
}

bool SampleDisplay::on_configure(GdkEventConfigure *event)
{
    auto w = get_allocation().get_width();
    auto h = get_allocation().get_height();
    if (w == width_ && h == height_)
        return false;
    width_ = w; height_ = h;
    printf("Windows resized to [%d,%d]\n", width_, height_);

    Glib::RefPtr<Pango::Context> context = get_pango_context();
    Pango::FontMetrics metrics = context->get_metrics(get_style_context()->get_font());

    timeLineHeight_ = ((metrics.get_ascent() + metrics.get_descent())
                       / Pango::SCALE);
    trackLineHeight_ = ((metrics.get_ascent() + metrics.get_descent())
                        / Pango::SCALE);

    trackMarkerWidth_ = ((metrics.get_approximate_digit_width() /
                          Pango::SCALE) * 5) + TRACK_MARKER_XPM_WIDTH + 2;

    // Don't even try to do anything smart if we haven't received a
    // reasonable window size yet. This will keep pixmap_ to NULL. This
    // is important because during startup we don't control how the
    // configure_event are timed wrt to gcdmaster bringup.
    draw_me_ = (width_ > 1 && height_ > 1);
    if (!draw_me_)
        return true;

    chanHeight_ = (height_ - timeLineHeight_ - trackLineHeight_ - 2) / 2;

    lcenter_ = chanHeight_ / 2 + trackLineHeight_;
    rcenter_ = lcenter_ + timeLineHeight_ + chanHeight_;

    trackLineY_ = trackLineHeight_ - 1;

    timeLineY_ = chanHeight_ + timeLineHeight_ + trackLineHeight_;
    timeTickWidth_ = ((metrics.get_approximate_digit_width() /
                       Pango::SCALE) * 13) + 3;

    sampleStartX_ = 10;
    sampleEndX_ = width_ - 10;
    sampleWidthX_ = sampleEndX_ - sampleStartX_ + 1;

    draw_samples_ = (width_ > 100 && height_ > 100);

    if (draw_samples_) {
        surface_ = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width_, height_);
        surface_cr_ = Cairo::Context::create(surface_);
        updateSamples();
    }

    return true;
}

bool SampleDisplay::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    const int w = get_allocation().get_width();
    const int h = get_allocation().get_height();

    if (draw_samples_) {
        draw_surface(cr);

        if (dragMode_ == DRAG_SAMPLE_MARKER) {
            cr->set_source_rgba(1.0, 0.0, 0.0, 0.5);
            cr->rectangle(selection_drag_x_, 0, selection_drag_w_, h - 1);
            cr->fill();
        }
    }

    draw_marker(cr);
    draw_cursor(cr);

    return false;
}

bool SampleDisplay::on_button_press(GdkEventButton *event)
{
  gint x = (gint)event->x;
  gint y = (gint)event->y;

  dragMode_ = DRAG_NONE;

  // e.g. if audio is playing
  if (cursorControlExtern_)
    return true;

  if (event->button == 1 && x >= sampleStartX_ && x <= sampleEndX_) {
    if (y > trackLineY_) {
      dragMode_ = DRAG_SAMPLE_MARKER;
      dragStart_ = dragEnd_ = x;
    }
    else {
      if ((pickedTrackMarker_ = trackManager_->pick(x - sampleStartX_ + 4,
						    &dragStopMin_,
						    &dragStopMax_)) != NULL) {
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

bool SampleDisplay::on_button_release(GdkEventButton *event)
{
    gint x = (gint)event->x;

    if (cursorControlExtern_)
        return false;

    if (x < sampleStartX_) {
        x = sampleStartX_;
    }
    else if (x > sampleEndX_) {
        x = sampleEndX_;
    }

    if (event->button == 1 && dragMode_ != DRAG_NONE) {
        if (dragMode_ == DRAG_SAMPLE_MARKER) {
            if (dragStart_ - x >= -5 && dragStart_ - x <= 5) {
                selectionSet_ = false;
                selectionCleared();
                markerSet(pixel2sample(dragStart_));
            }
            else {
                selectionSet_ = true;
                if (x > dragStart_) {
                    selectionStartSample_ = pixel2sample(dragStart_);
                    selectionEndSample_ = pixel2sample(x);
                    selectionStart_ = dragStart_;
                    selectionEnd_ = x;
                }
                else {
                    selectionEndSample_ = pixel2sample(dragStart_);
                    selectionStartSample_ = pixel2sample(x);
                    selectionEnd_ = dragStart_;
                    selectionStart_ = x;
                }
                selectionSet(selectionStartSample_, selectionEndSample_);
            }
        }
        else if (dragMode_ == DRAG_TRACK_MARKER) {
            if (dragStart_ - x >= -5 && dragStart_ - x <= 5) {
                trackManager_->select(pickedTrackMarker_);
                selectedTrack_ = pickedTrackMarker_->trackNr;
                selectedIndex_ = pickedTrackMarker_->indexNr;
                drawTrackLine();
                trackMarkSelected(pickedTrackMarker_->track, selectedTrack_,
                                  selectedIndex_);
            }
            else {
                selectedTrack_ = pickedTrackMarker_->trackNr;
                selectedIndex_ = pickedTrackMarker_->indexNr;
                trackMarkMoved(pickedTrackMarker_->track, selectedTrack_,
                               selectedIndex_, pixel2sample(x));
            }
            pickedTrackMarker_ = NULL;
        }

        dragMode_ = DRAG_NONE;
        set_cursor(x);
        redraw(0, 0, width_, height_);
    }

    return true;
}

bool SampleDisplay::on_motion_notify (GdkEventMotion *event)
{
    gint x, y;
    GdkModifierType state;

    if (cursorControlExtern_)
        return TRUE;

    x = (gint)event->x;
    y = (gint)event->y;
    state = (GdkModifierType) event->state;

    if (dragMode_ == DRAG_SAMPLE_MARKER) {
        gint dw = 0;
        gint dx = 0;

        if (x < sampleStartX_)
            x = sampleStartX_;
        else if (x > sampleEndX_)
            x = sampleEndX_;

        if (selectionEnd_ > dragStart_) {
            if (x < selectionEnd_) {
                if (x < dragStart_) {
                    dw = dragStart_ - x + 1;
                    dx = x;
                }
            }
            else {
                dw = x - selectionEnd_;
                dx = selectionEnd_ + 1;
            }
        }
        else if (selectionEnd_ < dragStart_) {
            if (x > selectionEnd_) {

                if (x > dragStart_) {
                    dw = x - dragStart_ + 1;
                    dx = dragStart_;
                }
            }
            else {
                dw = selectionEnd_ - x;
                dx = x;
            }
        }

        if (dw != 0) {
            selection_drag_x_ = dx;
            selection_drag_w_ = dw;
            redraw(dx, 0, dw, height_ - 1);
        }
        selectionEnd_ = x;

    } else if (dragMode_ == DRAG_TRACK_MARKER) {
        if (x < dragStopMin_)
            x = dragStopMin_;

        if (x > dragStopMax_)
            x = dragStopMax_;

        if (dragLastX_ > 0) {
            drawTrackMarker(2, dragLastX_, 0, 0, 0, 0);
        }
        drawTrackMarker(1, x, pickedTrackMarker_->trackNr,
                        pickedTrackMarker_->indexNr, 0, 0);
        dragLastX_ = x;
        set_cursor(x);
    }
    else {
        set_cursor(x);
    }

    return TRUE;
}

bool SampleDisplay::on_enter_notify(GdkEventCrossing *event)
{
    if (cursorControlExtern_)
        return true;

    set_cursor((gint)event->x);
    return true;
}

bool SampleDisplay::on_leave_notify(GdkEventCrossing *event)
{
    if (cursorControlExtern_)
        return true;

    unset_cursor();
    return true;
}

void SampleDisplay::redraw(gint x, gint y, gint width, gint height)
{
    get_window()->invalidate_rect(Gdk::Rectangle(x, y, width, height), true);
}

void SampleDisplay::draw_marker(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (markerSet_) {
        markerX_ = sample2pixel(markerSample_);
        if (markerX_ >= 0) {
            setColor(markerColor_, cr);
            drawLine(markerX_, trackLineY_, markerX_, height_ - 1, cr);
        }
    }
}

void SampleDisplay::set_marker(unsigned long sample)
{
    if (markerSet_)
        redraw(markerX_ > 1 ? markerX_ - 1 : 0, 0, 2, height_);

    markerSample_ = sample;
    markerSet_ = true;
    redraw(markerSample_ > 1 ? markerSample_ - 1 : 0, 0, 2, height_);
}

void SampleDisplay::clear_marker()
{
    if (markerSet_)
        redraw(markerX_ > 1 ? markerX_ - 1 : 0, 0, 2, height_);

    markerSet_ = false;
}


void SampleDisplay::updateSamples()
{
    if (tocEdit_ == NULL)
        return;

    Toc *toc = tocEdit_->toc();

    gint halfHeight = chanHeight_ / 2;

    setColor(white_);
    drawRectangle(0, 0, width_, height_);

    long res = (maxSample_ - minSample_ + 1)/sampleWidthX_;
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
        if (regionStartSample_ <= maxSample_ &&
            regionEndSample_ >= minSample_) {

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
            setColor(selectionBackgroundColor_);
            drawRectangle(regionStart, lcenter_ - halfHeight,
                          regionEnd - regionStart + 1, chanHeight_);
            drawRectangle(regionStart, rcenter_ - halfHeight,
                          regionEnd - regionStart + 1, chanHeight_);
        }
    }

    setColor(sampleColor_);

    if (bres > 0) {
        for (s = minSample_, i = sampleStartX_;
             s < maxSample_ && i <= sampleEndX_;
             s += res, i++) {
            lnegsum = lpossum = rnegsum = rpossum = 0;

            if (regionStart != -1 && i >= regionStart && regionActive == 0) {
                regionActive = 1;
                setColor(markerColor_);
            }
            else if (regionActive == 1 && i > regionEnd) {
                regionActive = 2;
                setColor(sampleColor_);
            }

            tocEdit_->sampleManager()->getPeak(s, s + res, &lnegsum, &lpossum,
                                               &rnegsum, &rpossum);

            pos = double(lnegsum) * halfHeight;
            pos /= SHRT_MAX;
            if (pos != 0)
                drawLine(i, lcenter_, i, lcenter_ - (gint)pos);

            pos = double(lpossum) * halfHeight;
            pos /= SHRT_MAX;
            if (pos != 0)
                drawLine(i, lcenter_, i, lcenter_ - (gint)pos);

            pos = double(rnegsum) * halfHeight;
            pos /= SHRT_MAX;
            if (pos != 0)
                drawLine(i, rcenter_, i, rcenter_ - (gint)pos);

            pos = double(rpossum) * halfHeight;
            pos /= SHRT_MAX;
            if (pos != 0)
                drawLine(i, rcenter_, i, rcenter_ - (gint)pos);
        }
    }
    else if (maxSample_ > 0 && res >= 1) {

        TocReader reader(toc);

        if (reader.openData() == 0) {
            Sample *sampleBuf = new Sample[tocEdit_->sampleManager()->blocking()];
            double dres = double(maxSample_ - minSample_ + 1)/double(sampleWidthX_);
            double ds;

            for (ds = minSample_, i = sampleStartX_;
                 ds < maxSample_ && i <= sampleEndX_;
                 ds += dres, i++) {

                lnegsum = lpossum = rnegsum = rpossum = 0;

                if (reader.seekSample((long)ds) == 0 &&
                    reader.readSamples(sampleBuf, res) == res) {
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
                    setColor(markerColor_);
                }
                else if (regionActive == 1 && i > regionEnd) {
                    regionActive = 2;
                    setColor(sampleColor_);
                }

                pos = double(lnegsum) * halfHeight;
                pos /= SHRT_MAX;
                if (pos != 0)
                    drawLine(i, lcenter_, i, lcenter_ - (gint)pos);

                pos = double(lpossum) * halfHeight;
                pos /= SHRT_MAX;
                if (pos != 0)
                    drawLine(i, lcenter_, i, lcenter_ - (gint)pos);

                pos = double(rnegsum) * halfHeight;
                pos /= SHRT_MAX;
                if (pos != 0)
                    drawLine(i, rcenter_, i, rcenter_ - (gint)pos);

                pos = double(rpossum) * halfHeight;
                pos /= SHRT_MAX;
                if (pos != 0)
                    drawLine(i, rcenter_, i, rcenter_ - (gint)pos);
            }

            delete[] sampleBuf;
            reader.closeData();
        }
    }
    else if (toc != NULL && maxSample_ > minSample_ + 1) {

        TocReader reader(toc);

        if (reader.openData() == 0) {
            long len = maxSample_ - minSample_ + 1;
            Sample *sampleBuf = new Sample[len];

            double pres = double(sampleWidthX_ - 1) / double(len - 1);
            double di;
            gint pos1;
            gint lastPosLeft, lastPosRight;

            if (reader.seekSample(minSample_) == 0 &&
                reader.readSamples(sampleBuf, len) == len) {

                for (j = 1, di = sampleStartX_ + pres;
                     j < len && di < sampleEndX_ + 1; j++, di += pres) {
                    if (regionStart != -1 && regionActive == 0 &&
                        minSample_ + j - 1 >= regionStartSample_ &&
                        minSample_ + j <= regionEndSample_) {
                        regionActive = 1;
                        setColor(markerColor_);
                    }
                    else if (regionActive == 1 && minSample_ + j > regionEndSample_) {
                        regionActive = 2;
                        setColor(sampleColor_);
                    }

                    pos = sampleBuf[j - 1].left() * halfHeight;
                    pos /= SHRT_MAX;

                    pos1 = sampleBuf[j].left() * halfHeight;
                    pos1 /= SHRT_MAX;
                    lastPosLeft = pos1;

                    if (pos != 0 || pos1 != 0)
                        drawLine(long(di - pres), lcenter_ - (gint)pos,
                                 long(di), lcenter_ - pos1);

                    pos = sampleBuf[j - 1].right() * halfHeight;
                    pos /= SHRT_MAX;

                    pos1 = sampleBuf[j].right() * halfHeight;
                    pos1 /= SHRT_MAX;
                    lastPosRight = pos1;

                    if (pos != 0 || pos1 != 0)
                        drawLine(long(di - pres), rcenter_ - (gint)pos,
                                 long(di), rcenter_ - pos1);
                }

                if (0 && (gint)di < sampleEndX_) {
                    pos = sampleBuf[len -1].left() * halfHeight;
                    pos /= SHRT_MAX;
                    if (pos != 0 || lastPosLeft != 0)
                        drawLine(long(di), lcenter_ - lastPosLeft,
                                 sampleEndX_, lcenter_ - (gint)pos);

                    pos = sampleBuf[len - 1].right() * halfHeight;
                    pos /= SHRT_MAX;
                    if (pos != 0 || lastPosRight != 0)
                        drawLine(long(di), rcenter_ - lastPosRight,
                                 sampleEndX_, rcenter_ - (gint)pos);
                }
            }
            delete[] sampleBuf;
        }
    }

    setColor(middleLineColor_);

    drawLine(sampleStartX_, lcenter_,	sampleEndX_, lcenter_);
    drawLine(sampleStartX_, rcenter_, sampleEndX_, rcenter_);

    setColor(black_);

    drawLine(sampleStartX_ - 1, lcenter_ - halfHeight,
             sampleEndX_ + 1, lcenter_ - halfHeight);
    drawLine(sampleStartX_ - 1, lcenter_ + halfHeight,
             sampleEndX_ + 1, lcenter_ + halfHeight);
    drawLine(sampleStartX_ - 1, lcenter_ - halfHeight,
             sampleStartX_ - 1, lcenter_ + halfHeight);
    drawLine(sampleEndX_ + 1, lcenter_ - halfHeight,
             sampleEndX_ + 1, lcenter_ + halfHeight);

    drawLine(sampleStartX_ - 1, rcenter_ - halfHeight,
             sampleEndX_ + 1, rcenter_ - halfHeight);
    drawLine(sampleStartX_ - 1, rcenter_ + halfHeight,
             sampleEndX_ + 1, rcenter_ + halfHeight);
    drawLine(sampleStartX_ - 1, rcenter_ + halfHeight,
             sampleStartX_ - 1, rcenter_ - halfHeight);
    drawLine(sampleEndX_ + 1, rcenter_ + halfHeight,
             sampleEndX_ + 1, rcenter_ - halfHeight);

    drawTimeLine();

    trackManager_->update(toc, minSample_, maxSample_, sampleWidthX_);
    if (selectedTrack_ > 0) {
        trackManager_->select(selectedTrack_, selectedIndex_);
    }
    drawTrackLine();
}

void SampleDisplay::draw_cursor(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!cursorDrawn_) {
        cursorDrawn_ = true;
        setColor(cursorColor_, cr);
        drawLine(cursorX_, trackLineY_, cursorX_, height_ - 1, cr);
    }
}

void SampleDisplay::set_cursor(gint x)
{
    if (x < sampleStartX_ || x > sampleEndX_)
        return;

    if (cursorDrawn_ && cursorX_ != x) {
        redraw(cursorX_ > 1 ? cursorX_ - 1 : 0, 0, 2, height_);
    }

    if (!cursorDrawn_ || cursorX_ != x) {
        cursorDrawn_ = false;
        cursorX_ = x;
        redraw(cursorX_ > 1 ? cursorX_ -1  : 0, 0, 2, height_);
    }

    cursorX_ = x;

    if (cursorControlExtern_ == false)
        cursorMoved(pixel2sample(x));
}

void SampleDisplay::unset_cursor()
{
    if (cursorDrawn_) {
        redraw(cursorX_, 0, 1, height_);
        cursorDrawn_ = false;
    }
}

void SampleDisplay::drawTimeTick(gint x, gint y, unsigned long sample)
{
    char buf[50];

    unsigned long min = sample / (60 * 44100);
    sample %= 60 * 44100;

    unsigned long sec = sample / 44100;
    sample %= 44100;

    unsigned long frame = sample / 588;
    sample %= 588;

    sprintf(buf, "%lu:%02lu:%02lu.%03lu", min, sec, frame, sample);

    setColor(black_);
    drawLine(x, y - timeLineHeight_, x, y);
    drawText(buf, x + 3, y - timeLineHeight_);
}

void SampleDisplay::drawTimeLine()
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
    }
    else if ((s = len / 44100) > 1) {
        dt = 44100;
    }
    else if ((s = len / 588) > 1) {
        dt = 588;
    }
    else {
        dt = 1;
        s = len;
    }

    if (s > maxNofTicks) {
        dtx = s / maxNofTicks;
        if (s % maxNofTicks != 0)
            dtx++;
        dtx *= dt;
    }
    else {
        dtx = dt;
    }

    if (dt > 1) {
        if (minSample_ % dt == 0) {
            startSample = minSample_;
        }
        else {
            startSample = minSample_ / dt;
            startSample = startSample * dt + dt;
        }
    }
    else {
        startSample = minSample_;
    }

    for (s = startSample; s < maxSample_; s += dtx) {
        x = sample2pixel(s);

        if (x + timeTickWidth_ <= sampleEndX_)
            drawTimeTick(x, timeLineY_, s);
    }
}

// Draws track marker.
// mode: 0: draw on 'pixmap_'
//       1: draw on window
//       2: redraw region at given position
void SampleDisplay::drawTrackMarker(int mode, gint x, int trackNr,
				    int indexNr, int selected, int extend)
{
    if (mode == 2) {
        redraw(x - 4, trackLineY_ - trackLineHeight_, trackMarkerWidth_,
               trackLineHeight_);
        return;
    }

    char buf[20];
    sprintf(buf, "%d.%d", trackNr, indexNr);

    Glib::RefPtr<Gdk::Pixbuf> marker;

    if (extend) {
        marker = (indexNr == 1 ? trackExtendPixmap_ : indexExtendPixmap_);
    } else {
        if (selected)
            marker = (indexNr == 1 ? trackMarkerSelectedPixmap_ :
                      indexMarkerSelectedPixmap_);
        else
            marker = (indexNr == 1 ? trackMarkerPixmap_ : indexMarkerPixmap_);
    }

    if (selected)
        setColor(markerColor_);
    else
        setColor(white_);

    drawRectangle(x-4, trackLineY_ - trackLineHeight_,
                  trackMarkerWidth_, trackLineHeight_);

    setColor(black_);
    drawPixmap(marker, x - 4, trackLineY_ - TRACK_MARKER_XPM_HEIGHT);
    setColor(black_);
    drawText(buf, x + TRACK_MARKER_XPM_WIDTH / 2 + 2,
             trackLineY_ - trackLineHeight_ + 2);
}

void SampleDisplay::drawTrackLine()
{
  const TrackManager::Entry *run;
  const TrackManager::Entry *selected = NULL;

  for (run = trackManager_->first(); run != NULL;
       run = trackManager_->next()) {
    if (run->selected != 0 && run->extend == 0) {
      selected = run;
    }
    else if (run->indexNr != 1 || run->extend != 0) {
      drawTrackMarker(0, sampleStartX_ + run->xpos, run->trackNr, run->indexNr,
		      0, run->extend);
    }
  }

  for (run = trackManager_->first(); run != NULL;
       run = trackManager_->next()) {
    if (run->indexNr == 1 && run->selected == 0 && run->extend == 0) {
      drawTrackMarker(0, sampleStartX_ + run->xpos, run->trackNr, run->indexNr,
		      0, run->extend);
    }
  }

  if (selected != NULL)
    drawTrackMarker(0, sampleStartX_ + selected->xpos, selected->trackNr,
		    selected->indexNr, 1, 0);
}

void SampleDisplay::updateTrackMarks()
{
    if (tocEdit_ == NULL)
        return;

    Toc *toc = tocEdit_->toc();

    setColor(white_);
    drawRectangle(sampleStartX_ - 4, trackLineY_ - trackLineHeight_,
                  width_ - sampleStartX_, trackLineHeight_);

    trackManager_->update(toc, minSample_, maxSample_, sampleWidthX_);
    if (selectedTrack_ > 0) {
        trackManager_->select(selectedTrack_, selectedIndex_);
    }
    drawTrackLine();

    redraw(0, 0, width_, height_);
}

void SampleDisplay::setColor(Gdk::RGBA c, Cairo::RefPtr<Cairo::Context> cr)
{
    if (!cr)
        cr = surface_cr_;
    cr->set_source_rgb(c.get_red(), c.get_green(), c.get_blue());
}

void SampleDisplay::drawLine(gint x1, gint y1, gint x2, gint y2,
                             Cairo::RefPtr<Cairo::Context> cr)
{
    if (!cr)
        cr = surface_cr_;
    cr->set_line_width(1.0);
    cr->move_to(x1, y1);
    cr->line_to(x2, y2);
    cr->stroke();
}

void SampleDisplay::drawRectangle(gint x1, gint y1, gint w, gint h)
{
    surface_cr_->rectangle((double)x1, (double)y1, (double)w, (double)h);
    surface_cr_->fill();
}

void SampleDisplay::drawPixmap(Glib::RefPtr<Gdk::Pixbuf> pixbuf,
                               gint x, gint y)
{
    Gdk::Cairo::set_source_pixbuf(surface_cr_, pixbuf, x, y);
    surface_cr_->paint();
}

void SampleDisplay::drawText(const char* text, gint x, gint y)
{
    Pango::FontDescription font;
    font.set_family("Monospace");

    auto layout = create_pango_layout(text);
    layout->set_font_description(font);
    surface_cr_->move_to(x, y);
    layout->show_in_cairo_context(surface_cr_);
}

void SampleDisplay::draw_surface(const Cairo::RefPtr<Cairo::Context>& cr)
{
    cr->set_source(surface_, 0.0, 0.0);
    cr->paint();
}
