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
 * $Log: MainWindow.h,v $
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:38:46  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.5  1999/05/24 18:10:25  mueller
 * Adapted to new reading interface of 'trackdb'.
 *
 * Revision 1.4  1999/02/28 10:59:07  mueller
 * Adapted to changes in 'trackdb'.
 *
 * Revision 1.3  1999/01/30 19:45:43  mueller
 * Fixes for compilation with Gtk-- 0.11.1.
 *
 * Revision 1.2  1999/01/30 15:11:13  mueller
 * First released version. Compiles with Gtk-- 0.9.14.
 *
 * Revision 1.1  1998/11/20 18:54:34  mueller
 * Initial revision
 *
 */

#ifndef __MAIN_WINDOW_H__
#define __MAIN_WINDOW_H__

#include <gtk--.h>
#include <gtk/gtk.h>

#include <gnome--.h>

#include "Toc.h"

class SampleDisplay;
class AddSilenceDialog;

class Toc;
class Track;
class SoundIF;
class Sample;
class TrackData;
class TocEdit;

class MainWindow : public Gtk::Window
//llanero class AudioChild : public Gnome::MDIGenericChild
{
private:
  enum Mode { ZOOM, SELECT };

  TocEdit *tocEdit_;

  Mode mode_;

  Gtk::VBox vbox_;

  Gtk::ItemFactory *itemFactory_;
//llanero  Gtk_ObjectHandle<Gtk_MenuBar> menuBar_;
  Gtk::MenuBar menuBar_;

  TocReader tocReader;

  SoundIF *soundInterface_;
  unsigned long playLength_; // remaining play length
  unsigned long playBurst_;
  unsigned long playPosition_;
  Sample *playBuffer_;
  int playing_;
  int playAbort_;


  SampleDisplay *sampleDisplay_;
  Gtk::Statusbar *statusBar_;
  guint lastMessageId_;

  Gtk::FileSelection *fileSelector_;
  Gtk::Connection fileSelectorC1_;
  Gtk::Connection fileSelectorC2_;

  Gtk::RadioButton *zoomButton_;
  Gtk::RadioButton *selectButton_;
  Gtk::Button *playButton_;
  
  Gtk::Entry *markerPos_;
  Gtk::Entry *cursorPos_;
  Gtk::Entry *selectionStartPos_;
  Gtk::Entry *selectionEndPos_;


  void createMenuBar();
  void setMode(Mode);

  void markerSetCallback(unsigned long);
  void cursorMovedCallback(unsigned long);
  void selectionSetCallback(unsigned long, unsigned long);

  void trackMarkSelectedCallback(const Track *, int trackNr, int indexNr);
  void trackMarkMovedCallback(const Track *, int trackNr, int indexNr,
			      unsigned long sample);

  const char *sample2string(unsigned long sample);
  unsigned long string2sample(const char *s);

  int snapSampleToBlock(unsigned long sample, long *block);
  void statusMessage(const char *fmt, ...);
  int getMarker(unsigned long *sample);
  void readTocCallback(int);
  void saveAsTocCallback(int);
  int playCallback();

  void tocBlockedMsg(const char *);

  void appendTrack();
  void appendFile();
  void insertFile();

  void appendSilence();
  void insertSilence();

  void trackInfo();
  void tocInfo();

  void cutTrackData();
  void pasteTrackData();

  void quit();
  void newToc();
  void readToc();
  void saveToc();
  void saveAsToc();
  void zoomIn();
  void zoomOut();
  void fullView();
  void play();

  void addTrackMark();
  void addIndexMark();
  void addPregap();
  void removeTrackMark();

  void markerSet();
  void selectionSet();

  void configureDevices();

  void extract();
  void record();

public:
  MainWindow(TocEdit *);

  TocEdit *tocEdit() const { return tocEdit_; }
  
  void update(unsigned long level);

  gint delete_event_impl(GdkEventAny*);


};

#endif
