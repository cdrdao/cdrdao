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

#ifndef __SAMPLE_DISPLAY_H
#define __SAMPLE_DISPLAY_H

#include <gtkmm.h>
#include <gdkmm.h>
#include <pangomm.h>
#include <gtk/gtk.h>

#include "TrackManager.h"

class Toc;
class Sample;
class TocEdit;

class SampleDisplay : public Gtk::DrawingArea
{
public:
  SampleDisplay();

  void setTocEdit(TocEdit *);
  bool getSelection(unsigned long* start, unsigned long* end);
  void setSelectedTrackMarker(int trackNr, int indexNr);
  void setMarker(unsigned long sample);
  void clearMarker();
  int  getMarker(unsigned long *);
  void setView(unsigned long start, unsigned long end);
  void getView(unsigned long *start, unsigned long *end);
  void setRegion(unsigned long start, unsigned long end);
  int  getRegion(unsigned long *start, unsigned long *end);
  void clearRegion();
  Gtk::Adjustment *getAdjustment() { return adjustment_; }
  void updateTrackMarks();
  void setCursor(int, unsigned long);

  void updateToc(unsigned long, unsigned long);

  sigc::signal1<void, unsigned long> markerSet;
  sigc::signal1<void, unsigned long> cursorMoved;
  sigc::signal2<void, unsigned long, unsigned long> selectionSet;
  sigc::signal0<void> selectionCleared;
  sigc::signal3<void, const Track *, int, int> trackMarkSelected;
  sigc::signal4<void, const Track *, int, int, unsigned long> trackMarkMoved;
  sigc::signal2<void, unsigned long, unsigned long> viewModified;
  
protected:
  bool handleConfigureEvent(GdkEventConfigure *);
  bool handleExposeEvent(GdkEventExpose *event);
  bool handleMotionNotifyEvent(GdkEventMotion *event);
  bool handleButtonPressEvent(GdkEventButton*);
  bool handleButtonReleaseEvent(GdkEventButton*);
  bool handleEnterEvent(GdkEventCrossing*);
  bool handleLeaveEvent(GdkEventCrossing*);

private:
  enum DragMode { DRAG_NONE, DRAG_SAMPLE_MARKER, DRAG_TRACK_MARKER };

  Gtk::Adjustment *adjustment_;

  Glib::RefPtr<Gdk::Pixmap> pixmap_;
  Glib::RefPtr<Gdk::Pixmap> trackMarkerPixmap_;
  Glib::RefPtr<Gdk::Pixmap> indexMarkerPixmap_;
  Glib::RefPtr<Gdk::Pixmap> trackMarkerSelectedPixmap_;
  Glib::RefPtr<Gdk::Pixmap> indexMarkerSelectedPixmap_;
  Glib::RefPtr<Gdk::Pixmap> trackExtendPixmap_;
  Glib::RefPtr<Gdk::Pixmap> indexExtendPixmap_;

  Glib::RefPtr<Gdk::GC> drawGc_;
  Gdk::Color sampleColor_;
  Gdk::Color middleLineColor_;
  Gdk::Color cursorColor_;
  Gdk::Color markerColor_;
  Gdk::Color selectionBackgroundColor_;

  gint width_;
  gint height_;
  gint timeLineHeight_;
  gint timeLineY_;
  gint timeTickWidth_;
  gint timeTickSep_;
  gint sampleStartX_;
  gint sampleEndX_;
  gint sampleWidthX_;

  gint trackLineHeight_;
  gint trackLineY_;
  gint trackMarkerWidth_;
  const TrackManager::Entry *pickedTrackMarker_;

  gint chanSep_;
  gint chanHeight_;
  gint lcenter_;
  gint rcenter_;

  TrackManager *trackManager_;

  TocEdit *tocEdit_;
  unsigned long minSample_;
  unsigned long maxSample_;
  unsigned long resolution_;

  bool cursorDrawn_;
  gint cursorX_;
  bool cursorControlExtern_;

  bool markerSet_;
  gint markerX_;
  unsigned long markerSample_;

  bool selectionSet_;
  unsigned long selectionStartSample_;
  unsigned long selectionEndSample_;
  gint selectionStart_;
  gint selectionEnd_;

  bool regionSet_;
  unsigned long regionStartSample_;
  unsigned long regionEndSample_;

  int selectedTrack_;
  int selectedIndex_;

  DragMode dragMode_;
  gint dragStart_, dragEnd_;
  gint dragStopMin_, dragStopMax_;
  gint dragLastX_;

  void scrollTo();
  void redraw(gint x, gint y, gint width, gint height, int);
  void readSamples(long startBlock, long endBlock);
  void updateSamples();
  void drawCursor(gint);
  void undrawCursor();
  void getColor(const char *, Gdk::Color *);
  unsigned long pixel2sample(gint x);
  gint sample2pixel(unsigned long);
  void drawMarker();
  void removeMarker();
  void drawTimeTick(gint x, gint y, unsigned long sample);
  void drawTimeLine();
  void drawTrackMarker(int mode, gint x, int trackNr, int indexNr,
		       int selected, int extend);
  void drawTrackLine();
};

#endif
