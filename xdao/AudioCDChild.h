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

#ifndef __AUDIO_CD_CHILD_H__
#define __AUDIO_CD_CHILD_H__

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

class AudioCDChild : public Gnome::MDIGenericChild
{
private:
  enum Mode { ZOOM, SELECT };

  TocEdit *tocEdit_;

  Mode mode_;

//it is easier with pointers ?( !
  Gtk::VBox *vbox_;
  GtkWidget *vbox_aux_;

  TocReader tocReader;

  SoundIF *soundInterface_;
  unsigned long playLength_; // remaining play length
  unsigned long playBurst_;
  unsigned long playPosition_;
  Sample *playBuffer_;
  int playing_;
  int playAbort_;


  SampleDisplay *sampleDisplay_;

  Gtk::RadioButton *zoomButton_;
  Gtk::RadioButton *selectButton_;
  Gtk::Button *playButton_;
  
  Gtk::Entry *markerPos_;
  Gtk::Entry *cursorPos_;
  Gtk::Entry *selectionStartPos_;
  Gtk::Entry *selectionEndPos_;


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


public:
  AudioCDChild(TocEdit *tedit);
  void BuildChild();

//  void update(unsigned long level);

};

GtkWidget * AudioCDChild_Creator(GnomeMDIChild *child, gpointer data);


#endif
