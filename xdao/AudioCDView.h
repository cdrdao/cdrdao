/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __AUDIO_CD_VIEW_H__
#define __AUDIO_CD_VIEW_H__

#include <gtk--.h>
#include <gtk/gtk.h>

#include <gnome--.h>

#include "GenericView.h"

class SampleDisplay;

enum {
  TARGET_URI_LIST,
};

class AudioCDView : public GenericView
{
public:
  AudioCDView(AudioCDChild *child);
  SigC::Signal0<void> add_view;

  void update(unsigned long level);

private:
  friend class AudioCDChild;

  enum Mode { ZOOM, SELECT };

  AudioCDChild *cdchild;

  TocEdit *tocEdit_;

  Mode mode_;

  SampleDisplay *sampleDisplay_;

  Gtk::RadioButton *zoomButton_;
  Gtk::RadioButton *selectButton_;
  Gtk::Button *playButton_;
  Gtk::Label *playLabel_;
    
  Gtk::Entry *markerPos_;
  Gtk::Entry *cursorPos_;
  Gtk::Entry *selectionStartPos_;
  Gtk::Entry *selectionEndPos_;

  void setMode(Mode);

  void markerSetCallback(unsigned long);
  void cursorMovedCallback(unsigned long);
  void selectionSetCallback(unsigned long, unsigned long);

  void trackMarkSelectedCallback(const Track *, int trackNr, int indexNr);

  void zoomIn();
  void zoomOut();
  void fullView();
  void play();

  void addTrackMark();
  void addIndexMark();

  int getMarker(unsigned long *sample);
  void markerSet();

  void selectionSet();

  void drag_data_received_cb(GdkDragContext *context, gint x, gint y,
         GtkSelectionData *selection_data, guint info, guint time);


// Allow a different selection on every view.
public:
  void sampleMarker(unsigned long);
  int sampleMarker(unsigned long *) const;

  void sampleSelection(unsigned long, unsigned long);
  int sampleSelection(unsigned long *, unsigned long *) const;

  void sampleViewFull();
  void sampleViewInclude(unsigned long, unsigned long);
  void sampleView(unsigned long *, unsigned long *) const;
  void sampleView(unsigned long smin, unsigned long smax);

  void trackSelection(int);
  int trackSelection(int *) const;

  void indexSelection(int);
  int indexSelection(int *) const;

private:
  int sampleMarkerValid_;
  unsigned long sampleMarker_;

  int sampleSelectionValid_;
  unsigned long sampleSelectionMin_;
  unsigned long sampleSelectionMax_;

  unsigned long sampleViewMin_;
  unsigned long sampleViewMax_;
  
  int trackSelectionValid_;
  int trackSelection_;

  int indexSelectionValid_;
  int indexSelection_;

//FIXME: This should be used!!
  unsigned long updateLevel_;

};

#endif

