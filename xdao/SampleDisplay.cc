/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: SampleDisplay.cc,v $
 * Revision 1.5  2000/04/14 13:22:02  llanero
 * changed the MDI object to GnomeApp until gnome-- MDI is done.
 * Also catched a bug in SampleDisplay.cc:1000.
 *
 * Revision 1.4  2000/03/05 22:25:52  llanero
 * more code translated to gtk-- 1.1.8
 *
 * Revision 1.3  2000/03/04 01:28:52  llanero
 * SampleDisplay.{cc,h} are fixed now = gtk 1.1.8 compliant.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:50  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.5  1999/08/07 16:27:28  mueller
 * Applied patch from Yves Bastide:
 * * prefixing member function names with their class name in connect_to_method
 * * explicitly `const_cast'ing a cast to const
 *
 * Revision 1.4  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.3  1999/01/30 19:45:43  mueller
 * Fixes for compilation with Gtk-- 0.11.1.
 *
 * Revision 1.1  1998/11/20 18:55:22  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: SampleDisplay.cc,v 1.5 2000/04/14 13:22:02 llanero Exp $";

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "TocEdit.h"
#include "SampleDisplay.h"
#include "SampleManager.h"
#include "TrackManager.h"

#include "Toc.h"
#include "util.h"


/* XPM data for track marker */
#define TRACK_MARKER_XPM_WIDTH 9
#define TRACK_MARKER_XPM_HEIGHT 12
static gchar *TRACK_MARKER_XPM_DATA[] = {
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
static gchar *INDEX_MARKER_XPM_DATA[] = {
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
static gchar *TRACK_EXTEND_XPM_DATA[] = {
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
static gchar *INDEX_EXTEND_XPM_DATA[] = {
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


SampleDisplay::SampleDisplay()
{
  adjustment_ = new Gtk::Adjustment(0.0, 0.0, 1.0);
  adjustment_->value_changed.connect(slot(this, &SampleDisplay::scrollTo));

  trackManager_ = NULL;

  pixmap_ = NULL;
  trackMarkerPixmap_ = indexMarkerPixmap_ = NULL;
  trackMarkerSelectedPixmap_ = indexMarkerSelectedPixmap_ = NULL;
  trackExtendPixmap_ = indexExtendPixmap_ = NULL;

  drawGc_ = NULL;
  timeTickFont_ = NULL;
  trackMarkerFont_ = NULL;
  width_ = height_ = chanHeight_ = lcenter_ = rcenter_ = 0;
  timeLineHeight_ = timeLineY_ = 0;
  timeTickWidth_ = 0;
  timeTickSep_ = 20;
  sampleStartX_ = sampleEndX_ = sampleWidthX_ = 0;
  minSample_ = maxSample_ = resolution_ = 0;
  tocEdit_ = NULL;

  chanSep_ = 10;

  cursorControlExtern_ = 0;
  cursorDrawn_ = 0;
  cursorX_ = 0;

  markerSet_ = 0;
  selectionSet_ = 0;
  regionSet_ = 0;
  dragMode_ = DRAG_NONE;

  pickedTrackMarker_ = NULL;
  selectedTrack_ = 0;
  selectedIndex_ = 0;

  expose_event.connect(slot(this, &SampleDisplay::handle_expose_event));
  configure_event.connect(slot(this, &SampleDisplay::handle_configure_event));
  motion_notify_event.connect(slot(this,
  		&SampleDisplay::handle_motion_notify_event));
  button_press_event.connect(slot(this, &SampleDisplay::handleButtonPressEvent));
  button_release_event.connect(slot(this,
  		&SampleDisplay::handleButtonReleaseEvent));
  enter_notify_event.connect(slot(this, &SampleDisplay::handleEnterEvent));
  leave_notify_event.connect(slot(this, &SampleDisplay::handleLeaveEvent));

  //connect_to_method (motion_notify_event, this, &handle_motion_notify_event);
  //connect_to_method (button_press_event, this, &handle_button_press_event);

  set_events (GDK_EXPOSURE_MASK
	      | GDK_LEAVE_NOTIFY_MASK
	      | GDK_ENTER_NOTIFY_MASK
	      | GDK_BUTTON_PRESS_MASK
	      | GDK_BUTTON_RELEASE_MASK
	      | GDK_POINTER_MOTION_MASK
	      | GDK_POINTER_MOTION_HINT_MASK);
}

void SampleDisplay::setTocEdit(TocEdit *t)
{
  tocEdit_ = t;

  Toc *toc = tocEdit_->toc();

  markerSet_ = 0;
  selectionSet_ = 0;
  regionSet_ = 0;

  minSample_ = 0;

  if (toc->length().samples() > 0) {
    maxSample_ = toc->length().samples() - 1;
  }
  else {
    maxSample_ = 0;
  }
}

void SampleDisplay::updateToc()
{
  Toc *toc = tocEdit_->toc();

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
  redraw(0, 0, width_, height_, 0);

  GtkAdjustment *adjust = adjustment_->gtkobj();
  if (toc == NULL) {
    adjust->lower = 0.0;
    adjust->upper = 1.0;
    adjust->value = 0.0;
    adjust->page_size = 0.0;
  }
  else {
    adjust->lower = 0.0;
    adjust->upper = toc->length().samples();
    adjust->value = minSample_;

    adjust->step_increment = len / 4;
    if (adjust->step_increment == 0.0)
      adjust->step_increment = 1.0;

    adjust->page_increment = len / 1.1;
    adjust->page_size = len;
  }
  adjustment_->changed();
  
}

void SampleDisplay::getView(unsigned long *start, unsigned long *end)
{
  *start = minSample_;
  *end = maxSample_;
}

int SampleDisplay::getSelection(unsigned long *start, unsigned long *end)
{
  if (selectionSet_) {
    *start = selectionStartSample_;
    *end = selectionEndSample_;
    return 1;
  }
  
  return 0;
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
  Toc *toc = tocEdit_->toc();

  if (end <= start || end >= toc->length().samples()) {
    regionSet_ = 0;
  }
  else {
    regionStartSample_ = start;
    regionEndSample_ = end;
    regionSet_ = 1;
  }

  setView(minSample_, maxSample_);

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
    cursorControlExtern_ = 0;
  }
  else {
    cursorControlExtern_ = 1;

    gint x = sample2pixel(sample);
    if (x >= 0)
      drawCursor(x);
    else
      undrawCursor();
  }
}

void SampleDisplay::getColor(const char *colorName, Gdk_Color *color)
{
  if (!color->parse(colorName) || !get_colormap().alloc(*color)) {
    message(-1, "Cannot allocate color \"%s\"", colorName);
    *color = get_colormap().black();
  }
}

void SampleDisplay::scrollTo()
{
  Toc *toc = tocEdit_->toc();
  GtkAdjustment *adjust = adjustment_->gtkobj();

  if (adjust->page_size == 0.0)
    return;

  minSample_ = (unsigned long)adjust->value;
  maxSample_ = (unsigned long)(adjust->value + adjust->page_size) - 1;

  if (maxSample_ >= toc->length().samples()) {
    maxSample_ = toc->length().samples() - 1;
    if (maxSample_ <= (unsigned long)(adjust->page_size - 1))
      minSample_ = 0;
    else 
      minSample_ = maxSample_ - (unsigned long)(adjust->page_size - 1);
  }

  updateSamples();
  redraw(0, 0, width_, height_, 0);
}

unsigned long SampleDisplay::pixel2sample(gint x)
{
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

int SampleDisplay::handle_configure_event (GdkEventConfigure *event)
{
//llanero  if (drawGc_ == NULL) {
  if (!drawGc_) {
    Gdk_Bitmap mask;
    Gdk_Window window(get_window());

    drawGc_ = Gdk_GC(window);
        
    getColor("darkslateblue", &sampleColor_);
    getColor("red3", &middleLineColor_);
    getColor("gold2", &cursorColor_);
    getColor("red", &markerColor_);
    getColor("#ffc0e0", &selectionBackgroundColor_);

    timeTickFont_ = Gdk_Font("fixed");
    drawGc_.set_font(timeTickFont_);

    trackMarkerFont_ = timeTickFont_;

    timeLineHeight_ = timeTickFont_.char_height('0') + 6;
    trackLineHeight_ = timeTickFont_.char_height('0') + 10;

    trackMarkerPixmap_ = Gdk_Pixmap();
    trackMarkerPixmap_.create_from_xpm_d(window, mask,
					  get_colormap().white(),
					  TRACK_MARKER_XPM_DATA);

    indexMarkerPixmap_ = Gdk_Pixmap();
    indexMarkerPixmap_.create_from_xpm_d(window, mask,
					  get_colormap().white(),
					  INDEX_MARKER_XPM_DATA);

    trackMarkerSelectedPixmap_ = Gdk_Pixmap();
    trackMarkerSelectedPixmap_.create_from_xpm_d(window, mask,
						  markerColor_,
						  TRACK_MARKER_XPM_DATA);
						  
    indexMarkerSelectedPixmap_ = Gdk_Pixmap();
    indexMarkerSelectedPixmap_.create_from_xpm_d(window, mask,
						  markerColor_,
						  INDEX_MARKER_XPM_DATA);
    trackExtendPixmap_ = Gdk_Pixmap();
    trackExtendPixmap_.create_from_xpm_d(window, mask,
					  get_colormap().white(),
					  TRACK_EXTEND_XPM_DATA);

    indexExtendPixmap_ = Gdk_Pixmap();
    indexExtendPixmap_.create_from_xpm_d(window, mask,
					  get_colormap().white(),
					  INDEX_EXTEND_XPM_DATA);

    trackMarkerWidth_ = trackMarkerWidth();
    trackManager_ = new TrackManager(TRACK_MARKER_XPM_WIDTH);
}

  width_ = width();
  height_ = height();
  chanHeight_ = (height_ - timeLineHeight_ - trackLineHeight_ - 2) / 2;

  lcenter_ = chanHeight_ / 2 + trackLineHeight_;
  rcenter_ = lcenter_ + timeLineHeight_ + chanHeight_;

  trackLineY_ = trackLineHeight_ - 1;

  timeLineY_ = chanHeight_ + timeLineHeight_ + trackLineHeight_;
  timeTickWidth_ = timeTickWidth();

  sampleStartX_ = 10;
  sampleEndX_ = width_ - 10;
  sampleWidthX_ = sampleEndX_ - sampleStartX_ + 1;
  
  
//llanero  if (pixmap_ != NULL)
//llanero    delete pixmap_;
    if (pixmap_)
      pixmap_.release();
      
  pixmap_ = Gdk_Pixmap(get_window(), width(), height(), -1);

  //message(0, "handle_configure_event: %d\n", width_);

//FIXME: will crash if some .toc loaded!!!
//  updateSamples();

  return TRUE;
}


int SampleDisplay::handle_expose_event (GdkEventExpose *event)
{
  redraw(event->area.x, event->area.y, event->area.width, event->area.height,
	 0);

  return FALSE;
}

int SampleDisplay::handleButtonPressEvent(GdkEventButton *event)
{
  gint x = (gint)event->x;
  gint y = (gint)event->y;

  dragMode_ = DRAG_NONE;

  if (cursorControlExtern_)
    return TRUE;

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

  return TRUE;
}

int SampleDisplay::handleButtonReleaseEvent(GdkEventButton *event)
{
  gint x = (gint)event->x;

  if (cursorControlExtern_)
    return TRUE;

  if (x < sampleStartX_) {
    x = sampleStartX_;
  }
  else if (x > sampleEndX_) {
    x = sampleEndX_;
  }

  if (event->button == 1 && dragMode_ != DRAG_NONE) {
    if (dragMode_ == DRAG_SAMPLE_MARKER) {
      if (dragStart_ - x >= -5 && dragStart_ - x <= 5) {
	markerSet(pixel2sample(dragStart_));
      }
      else {
	selectionSet_ = 1;
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
    drawCursor(x);
    redraw(0, 0, width_, height_, 0);
  }

  return TRUE;
}

int SampleDisplay::handle_motion_notify_event (GdkEventMotion *event)
{
  gint x, y;
  GdkModifierType state;

  if (cursorControlExtern_)
    return TRUE;
  
  if (event->is_hint)
    gdk_window_get_pointer (event->window, &x, &y, &state);
  else {
    x = (gint)event->x;
    y = (gint)event->y;
    state = (GdkModifierType) event->state;
  }


  if (dragMode_ == DRAG_SAMPLE_MARKER) {
    gint dw = 0;
    gint dx;

    if (x < sampleStartX_)
      x = sampleStartX_;
    else if (x > sampleEndX_)
      x = sampleEndX_;
    
    if (selectionEnd_ > dragStart_) {
      if (x < selectionEnd_) {
	redraw(x + 1, 0, selectionEnd_ - x, height_, 0x01);
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
	redraw(selectionEnd_, 0, x - selectionEnd_, height_, 0x01);
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
      drawGc_.set_foreground(cursorColor_);
      drawGc_.set_function(GDK_XOR);
      get_window().draw_rectangle(drawGc_, TRUE, dx, 0, dw, height_ - 1);
      drawGc_.set_function(GDK_COPY);
    }      

    selectionEnd_ = x;
  }
  else if (dragMode_ == DRAG_TRACK_MARKER) {
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
    drawCursor(x);
  }
  else {
    drawCursor(x);
  }
  
  return TRUE;
}

int SampleDisplay::handleEnterEvent(GdkEventCrossing *event)
{
  if (cursorControlExtern_)
    return TRUE;

  drawCursor((gint)event->x);
  return TRUE;
}

int SampleDisplay::handleLeaveEvent(GdkEventCrossing *event)
{
  if (cursorControlExtern_)
    return TRUE;

  undrawCursor();
  return TRUE;
}

// drawMask: 0x01: do not draw cursor
//           0x02: do not draw marker
void SampleDisplay::redraw(gint x, gint y, gint width, gint height,
			   int drawMask)
{
  get_window().draw_pixmap(drawGc_, pixmap_, x, y, x, y, width, height);

  if ((drawMask & 0x02) == 0)
    drawMarker();

  if ((drawMask & 0x01) == 0 && cursorDrawn_) {
    cursorDrawn_ = 0;
    drawCursor(cursorX_);
  }
}

void SampleDisplay::drawMarker()
{
  if (markerSet_) {
    drawGc_.set_foreground(markerColor_);

    markerX_ = sample2pixel(markerSample_);
    if (markerX_ >= 0) 
      get_window().draw_line(drawGc_, markerX_, trackLineY_,
			     markerX_, height_ - 1);
  }
}

void SampleDisplay::setMarker(unsigned long sample)
{
  if (markerSet_)
    redraw(markerX_, 0, 1, height_, 0x02);

  markerSample_ = sample;
  markerSet_ = 1;
  drawMarker();
}

void SampleDisplay::clearMarker()
{
  if (markerSet_)
    redraw(markerX_, 0, 1, height_, 0x02);

  markerSet_ = 0;
}


void SampleDisplay::updateSamples()
{
  Toc *toc = tocEdit_->toc();

//llanero  if (pixmap_ == NULL)
  if (!pixmap_)
    return;


  gint halfHeight = chanHeight_ / 2;

  drawGc_.set_foreground(get_colormap().white());
  pixmap_.draw_rectangle(drawGc_, TRUE, 0, 0, width_, height_);
  
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
      drawGc_.set_foreground(selectionBackgroundColor_);
      pixmap_.draw_rectangle(drawGc_, TRUE,
			      regionStart, lcenter_ - halfHeight,
			      regionEnd - regionStart + 1, chanHeight_);
      pixmap_.draw_rectangle(drawGc_, TRUE,
			      regionStart, rcenter_ - halfHeight,
			      regionEnd - regionStart + 1, chanHeight_);
    }
  }

g_print("DEBUG: %s\n", "Inside UpdateSamples");

  drawGc_.set_foreground(sampleColor_);

g_print("DEBUG: %s %i\n", "bres =", bres);
g_print("DEBUG: %s %i\n", "maxSample =", maxSample_);
g_print("DEBUG: %s %i\n", "res =", res);
g_print("DEBUG: %s %i\n", "minSample =", minSample_);
g_print("DEBUG: %s %i\n", "toc =", toc);

  if (bres > 0) {
    //message(0, "Draw 1");

g_print("DEBUG: %s\n", "Inside Draw 1!?");

    for (s = minSample_, i = sampleStartX_;
	 s < maxSample_ && i <= sampleEndX_;
	 s += res, i++) {
      lnegsum = lpossum = rnegsum = rpossum = 0;

      if (regionStart != -1 && i >= regionStart && regionActive == 0) {
	regionActive = 1;
	drawGc_.set_foreground(markerColor_);
      }
      else if (regionActive == 1 && i > regionEnd) {
	regionActive = 2;
	drawGc_.set_foreground(sampleColor_);
      }

      tocEdit_->sampleManager()->getPeak(s, s + res, &lnegsum, &lpossum,
					 &rnegsum, &rpossum);

      pos = double(lnegsum) * halfHeight;
      pos /= SHRT_MAX;
      //pos /= bres;
      if (pos != 0)
	pixmap_.draw_line(drawGc_, i, lcenter_, i, lcenter_ - (gint)pos);
      
      pos = double(lpossum) * halfHeight;
      pos /= SHRT_MAX;
      //pos /= bres;
      if (pos != 0)
	pixmap_.draw_line(drawGc_, i, lcenter_, i, lcenter_ - (gint)pos);
      
      pos = double(rnegsum) * halfHeight;
      pos /= SHRT_MAX;
      //pos /= bres;
      if (pos != 0)
	pixmap_.draw_line(drawGc_, i, rcenter_, i, rcenter_ - (gint)pos);
      
      pos = double(rpossum) * halfHeight;
      pos /= SHRT_MAX;
      //pos /= bres;
      if (pos != 0)
	pixmap_.draw_line(drawGc_, i, rcenter_, i, rcenter_ - (gint)pos);
    }
  }
  else if (maxSample_ > 0 && res >= 1) {

g_print("DEBUG: %s\n", "Inside Draw 2!?");

    //message(0, "Draw 2: %ld < %ld", res,
    //        tocEdit_->sampleManager()->blocking());
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
	  drawGc_.set_foreground(markerColor_);
	}
	else if (regionActive == 1 && i > regionEnd) {
	  regionActive = 2;
	  drawGc_.set_foreground(sampleColor_);
	}

	pos = double(lnegsum) * halfHeight;
	pos /= SHRT_MAX;
	//pos /= bres;
	if (pos != 0)
	  pixmap_.draw_line(drawGc_, i, lcenter_, i, lcenter_ - (gint)pos);
      
	pos = double(lpossum) * halfHeight;
	pos /= SHRT_MAX;
	//pos /= bres;
	if (pos != 0)
	  pixmap_.draw_line(drawGc_, i, lcenter_, i, lcenter_ - (gint)pos);
	
	pos = double(rnegsum) * halfHeight;
	pos /= SHRT_MAX;
	//pos /= bres;
	if (pos != 0)
	  pixmap_.draw_line(drawGc_, i, rcenter_, i, rcenter_ - (gint)pos);
	
	pos = double(rpossum) * halfHeight;
	pos /= SHRT_MAX;
	//pos /= bres;
	if (pos != 0)
	  pixmap_.draw_line(drawGc_, i, rcenter_, i, rcenter_ - (gint)pos);
      }

      delete[] sampleBuf;
      reader.closeData();
    }
  }
  else if (toc != NULL && maxSample_ > minSample_ + 1) {
    //message(0, "Draw 3");

g_print("DEBUG: %s\n", "Inside Draw 3!?");

    TocReader reader(toc);

g_print("DEBUG: %s %i\n", "Readed toc =", toc);

    if (reader.openData() == 0) {
      long len = maxSample_ - minSample_ + 1;
g_print("DEBUG: %s\n", "we get HERE!!");
      Sample *sampleBuf = new Sample[len];
g_print("DEBUG: %s\n", "but NOT HERE!!");

      double pres = double(sampleWidthX_ - 1) / double(len - 1);
      double di;
      gint pos1;
      gint lastPosLeft, lastPosRight;


      //printf("Drawing exact, res=%ld pres=%g %ld-%ld\n", res, pres, minSample_, maxSample_);

      if (reader.seekSample(minSample_) == 0 &&
	  reader.readSamples(sampleBuf, len) == len) {
	//printf("Drawing exact1\n");

	for (j = 1, di = sampleStartX_ + pres;
	     j < len && di < sampleEndX_ + 1; j++, di += pres) {
	  if (regionStart != -1 && regionActive == 0 &&
	      minSample_ + j - 1 >= regionStartSample_ &&
	      minSample_ + j <= regionEndSample_) {
	    regionActive = 1;
	    drawGc_.set_foreground(markerColor_);
	  }
	  else if (regionActive == 1 && minSample_ + j > regionEndSample_) {
	    regionActive = 2;
	    drawGc_.set_foreground(sampleColor_);
	  }

	  pos = sampleBuf[j - 1].left() * halfHeight;
	  pos /= SHRT_MAX;

	  pos1 = sampleBuf[j].left() * halfHeight;
	  pos1 /= SHRT_MAX;
	  lastPosLeft = pos1;

	  if (pos != 0 || pos1 != 0)
	    pixmap_.draw_line(drawGc_, long(di - pres), lcenter_ - (gint)pos,
			       long(di), lcenter_ - pos1);

	  pos = sampleBuf[j - 1].right() * halfHeight;
	  pos /= SHRT_MAX;

	  pos1 = sampleBuf[j].right() * halfHeight;
	  pos1 /= SHRT_MAX;
	  lastPosRight = pos1;

	  if (pos != 0 || pos1 != 0)
	    pixmap_.draw_line(drawGc_, long(di - pres), rcenter_ - (gint)pos,
			       long(di), rcenter_ - pos1);
	}

	if (0 && (gint)di < sampleEndX_) {
	  pos = sampleBuf[len -1].left() * halfHeight;
	  pos /= SHRT_MAX;
	  if (pos != 0 || lastPosLeft != 0)
	    pixmap_.draw_line(drawGc_, long(di), lcenter_ - lastPosLeft, 
			       sampleEndX_, lcenter_ - (gint)pos);
	  
	  pos = sampleBuf[len - 1].right() * halfHeight;
	  pos /= SHRT_MAX;
	  if (pos != 0 || lastPosRight != 0)
	    pixmap_.draw_line(drawGc_, long(di), rcenter_ - lastPosRight,
			       sampleEndX_, rcenter_ - (gint)pos);
	}
      }

      delete[] sampleBuf;
    }

  }


  drawGc_.set_foreground(middleLineColor_);

  pixmap_.draw_line(drawGc_, sampleStartX_, lcenter_,	sampleEndX_, lcenter_);
  pixmap_.draw_line(drawGc_, sampleStartX_, rcenter_, sampleEndX_, rcenter_);

  drawGc_.set_foreground(get_colormap().black());

  pixmap_.draw_line(drawGc_, sampleStartX_ - 1, lcenter_ - halfHeight,
		     sampleEndX_ + 1, lcenter_ - halfHeight);
  pixmap_.draw_line(drawGc_, sampleStartX_ - 1, lcenter_ + halfHeight,
		     sampleEndX_ + 1, lcenter_ + halfHeight);
  pixmap_.draw_line(drawGc_, sampleStartX_ - 1, lcenter_ - halfHeight,
		sampleStartX_ - 1, lcenter_ + halfHeight);
  pixmap_.draw_line(drawGc_, sampleEndX_ + 1, lcenter_ - halfHeight,
		     sampleEndX_ + 1, lcenter_ + halfHeight);

  pixmap_.draw_line(drawGc_, sampleStartX_ - 1, rcenter_ - halfHeight,
		     sampleEndX_ + 1, rcenter_ - halfHeight);
  pixmap_.draw_line(drawGc_, sampleStartX_ - 1, rcenter_ + halfHeight,
		     sampleEndX_ + 1, rcenter_ + halfHeight);
  pixmap_.draw_line(drawGc_, sampleStartX_ - 1, rcenter_ + halfHeight,
		     sampleStartX_ - 1, rcenter_ - halfHeight);
  pixmap_.draw_line(drawGc_, sampleEndX_ + 1, rcenter_ + halfHeight,
		     sampleEndX_ + 1, rcenter_ - halfHeight);

  drawTimeLine();

  trackManager_->update(toc, minSample_, maxSample_, sampleWidthX_);
  if (selectedTrack_ > 0) {
    trackManager_->select(selectedTrack_, selectedIndex_);
  }
  drawTrackLine();
}

void SampleDisplay::drawCursor(gint x)
{
//llanero  if (pixmap_ == NULL)
  if (!pixmap_)
    return;

  if (x < sampleStartX_ || x > sampleEndX_)
    return;

  if (cursorDrawn_ && cursorX_ != x) {
    redraw(cursorX_, 0, 1, height_, 0x01);
  }
  
  if (!cursorDrawn_ || cursorX_ != x) {
    drawGc_.set_foreground(cursorColor_);
    get_window().draw_line(drawGc_, x, trackLineY_, x, height_ - 1);
  }
  
  cursorDrawn_ = 1;
  cursorX_ = x;

  if (cursorControlExtern_ == 0)
    cursorMoved(pixel2sample(x));
}

void SampleDisplay::undrawCursor()
{
  if (cursorDrawn_) {
    redraw(cursorX_, 0, 1, height_, 0x01);
    cursorDrawn_ = 0;
  }
}

void SampleDisplay::drawTimeTick(gint x, gint y, unsigned long sample)
{
  char buf[50];

//llanero  if (pixmap_ == NULL)
  if (!pixmap_)
    return;

  unsigned long min = sample / (60 * 44100);
  sample %= 60 * 44100;

  unsigned long sec = sample / 44100;
  sample %= 44100;

  unsigned long frame = sample / 588;
  sample %= 588;

  sprintf(buf, "%lu:%02lu:%02lu.%03lu", min, sec, frame, sample);

  drawGc_.set_foreground(get_colormap().black());

  pixmap_.draw_line(drawGc_, x, y - timeLineHeight_, x, y);
  pixmap_.draw_string(timeTickFont_, drawGc_, x + 3, y - 3, buf);
}

gint SampleDisplay::timeTickWidth()
{
  return timeTickFont_.string_width("000:00:00.000") + 3;
}

void SampleDisplay::drawTimeLine()
{
  Toc *toc = tocEdit_->toc();

  if (toc->length().lba() == 0)
    return;

  gint sep = timeTickWidth_ + timeTickSep_;
  int maxNofTicks = (sampleWidthX_ + timeTickSep_) / sep;
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

gint SampleDisplay::trackMarkerWidth()
{
  return trackMarkerFont_.string_width("00.00") + TRACK_MARKER_XPM_WIDTH + 2;
}


// Draws track marker.
// mode: 0: draw on 'pixmap_'
//       1: draw on window
//       2: redraw region at given position
void SampleDisplay::drawTrackMarker(int mode, gint x, int trackNr,
				    int indexNr, int selected, int extend)
{

//llanero: VERY IMPORTANT!!!
/*
  if (mode < 2) {
    char buf[20];

    sprintf(buf, "%d.%d", trackNr, indexNr);

//llanero    Gdk_Pixmap *marker;
//llanero Do we need to free() this??
    Gdk_Pixmap marker;

    if (extend) {
      marker = indexNr == 1 ? trackExtendPixmap_ : indexExtendPixmap_;
    }
    else {
      if (selected)
	marker = indexNr == 1 ? trackMarkerSelectedPixmap_ :
	                        indexMarkerSelectedPixmap_;
      else
	marker = indexNr == 1 ? trackMarkerPixmap_ : indexMarkerPixmap_;
    }

//FIXME: This make it compile but doesn't work!
//llanero    Gdk_Drawable dr(get_window());
    Gdk_DraPixmap dr(get_window());

    if (mode == 0)
      dr = pixmap_;

    if (mode == 0) {
      if (selected)
	drawGc_.set_foreground(markerColor_);
      else
	drawGc_.set_foreground(get_colormap().white());
	
      dr.draw_rectangle(drawGc_, TRUE,  x-4, trackLineY_ - trackLineHeight_,
			trackMarkerWidth_, trackLineHeight_);
    }

    drawGc_.set_foreground(get_colormap().black());

    dr.draw_pixmap(drawGc_, marker, 0, 0,
		   x - 4, trackLineY_ - TRACK_MARKER_XPM_HEIGHT,
		   TRACK_MARKER_XPM_WIDTH, TRACK_MARKER_XPM_HEIGHT);

    dr.draw_string(trackMarkerFont_, drawGc_, 
		    x + TRACK_MARKER_XPM_WIDTH / 2 + 2, trackLineY_ - 1, buf);
  }
  else {
    redraw(x - 4, trackLineY_ - trackLineHeight_, trackMarkerWidth_,
	   trackLineHeight_, 0x03);
  }
*/
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
  Toc *toc = tocEdit_->toc();

  drawGc_.set_foreground(get_colormap().white());
  pixmap_.draw_rectangle(drawGc_, TRUE,
			  sampleStartX_ - 4, trackLineY_ - trackLineHeight_,
			  width_ - sampleStartX_, trackLineHeight_);

  trackManager_->update(toc, minSample_, maxSample_, sampleWidthX_);
  if (selectedTrack_ > 0) {
    trackManager_->select(selectedTrack_, selectedIndex_);
  }
  drawTrackLine();
  
  redraw(0, 0, width_, height_, 0x03);
}
