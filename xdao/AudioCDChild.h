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

#ifndef __AUDIO_CD_CHILD_H__
#define __AUDIO_CD_CHILD_H__

#include <gtk--.h>
#include <gtk/gtk.h>

#include <gnome--.h>

#include "Toc.h"
#include "GenericChild.h"

class Toc;
class Track;
class SoundIF;
class Sample;
class TrackData;
class TocEdit;
class TocInfoDialog;
class TrackInfoDialog;
class CdTextDialog;
class AddFileDialog;
class AddSilenceDialog;
class AudioCDView;
class AudioCDChildLabel;

class AudioCDChild : public GenericChild
{
public:
  AudioCDChild(TocEdit *tocEdit, gint number);

//FIXME  void create_view() { Gnome::MDIChild::create_view(); }

protected:
  virtual Gtk::Widget *create_view_impl();
  virtual Gtk::Widget* create_title_impl();
  virtual Gtk::Widget* update_title_impl(Gtk::Widget *old_label);

private: //related windows
  TocInfoDialog *tocInfoDialog_;
  TrackInfoDialog *trackInfoDialog_;
  AddFileDialog *addFileDialog_;
  AddSilenceDialog *addSilenceDialog_;
  CdTextDialog *cdTextDialog_;
  Gtk::FileSelection *saveFileSelector_;
  void saveFileSelectorOKCB();
  void saveFileSelectorCancelCB();

//FIXME  void new_view() { Gnome::MDIChild::create_toplevel_view(); }

private:
  friend class AudioCDView;

  TocReader tocReader;

  SoundIF *soundInterface_;
  unsigned long playLength_; // remaining play length
  unsigned long playBurst_;
  unsigned long playPosition_;
  Sample *playBuffer_;
  int playing_;
  int playAbort_;

  void play(unsigned long start, unsigned long end);
  int playCallback();

  GList *views;

  void addTrackMark();
  void addIndexMark();

  void trackMarkMovedCallback(const Track *, int trackNr, int indexNr,
			      unsigned long sample);

  const char *sample2string(unsigned long sample);
  unsigned long string2sample(const char *s);

  int snapSampleToBlock(unsigned long sample, long *block);
  void readTocCallback(int);
  void saveAsTocCallback(int);

  void tocBlockedMsg(const char *);

  void projectInfo();
  void trackInfo();
  void cdTextDialog();

public:

  void update(unsigned long level);
  void saveProject();
  void saveAsProject();
  bool closeProject();
  void record_to_cd();

  void cutTrackData();
  void pasteTrackData();

  void addPregap();
  void removeTrackMark();

  void appendSilence();
  void insertSilence();

  void appendTrack();
  void appendFile();
  void insertFile();

};

class AudioCDChildLabel : public Gtk::HBox
{
public:
  AudioCDChildLabel(const string &name);
  void set_name(const string &name) { label.set_text(name); }
protected:
  Gnome::Pixmap *pixmap;
  Gtk::Label label;
};

#endif
