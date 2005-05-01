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

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <libgnomeuimm.h>

#include "AddFileDialog.h"
#include "GenericView.h"
#include <list>

class SampleDisplay;
class Project;
class TrackInfoDialog;
class AddFileDialog;
class AddSilenceDialog;
class Track;

enum {
  TARGET_URI_LIST,
};

class AudioCDView : public GenericView
{
public:
  AudioCDView(AudioCDProject *project);
  ~AudioCDView();
  void add_menus(Glib::RefPtr<Gtk::UIManager> m_refUIManager);
  sigc::signal0<void> add_view;

  void update(unsigned long level = 0);

  enum Mode { ZOOM, SELECT };
  void setMode(Mode);

  void zoomIn();
  void zoomx2();
  void zoomOut();
  void fullView();

  sigc::signal1<void, unsigned long> signal_tocModified;

 protected:
  static const char* sample2string(unsigned long sample);
  static unsigned long string2sample(const char* s);

private:
  AudioCDProject *project_;

  Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;

  TrackInfoDialog*  trackInfoDialog_;
  AddFileDialog     addFileDialog_;
  AddSilenceDialog* addSilenceDialog_;

  Mode mode_;
  SampleDisplay *sampleDisplay_;

  Gtk::Entry*  markerPos_;
  Gtk::Label*  cursorPos_;
  Gtk::Entry*  selectionStartPos_;
  Gtk::Entry*  selectionEndPos_;

  void markerSetCallback(unsigned long);
  void cursorMovedCallback(unsigned long);
  void selectionSetCallback(unsigned long, unsigned long);
  void selectAll();
  void selectionClearedCallback();
  void trackMarkSelectedCallback(const Track *, int trackNr, int indexNr);
  void trackMarkMovedCallback(const Track *, int trackNr, int indexNr,
			      unsigned long sample);
  void viewModifiedCallback(unsigned long, unsigned long);
  int  snapSampleToBlock(unsigned long sample, long *block);

  void trackInfo();
  void cutTrackData();
  void pasteTrackData();

  void addTrackMark();
  void addIndexMark();
  void addPregap();
  void removeTrackMark();

  void appendSilence();
  void insertSilence();

  void appendTrack();
  void appendFile();
  void insertFile();

  int  getMarker(unsigned long *sample);
  void markerSet();

  void selectionSet();

  void drag_data_received_cb(const Glib::RefPtr<Gdk::DragContext>& context,
			     int x, int y,
			     const Gtk::SelectionData& selection_data,
			     guint info, guint time);
};

#endif
