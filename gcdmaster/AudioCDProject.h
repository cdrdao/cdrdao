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

#ifndef __AUDIOCDPROJECT_H__
#define __AUDIOCDPROJECT_H__

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <libgnomeuimm.h>

class Toc;
class Track;
class Sample;
class SoundIF;
class AudioCDView;
class TocInfoDialog;
class CdTextDialog;
class TocEdit;
#include "Project.h"

class AudioCDProject : public Project
{
public:
  enum PlayStatus {PLAYING, PAUSED, STOPPED};

  AudioCDProject(int number, const char *name, TocEdit *tocEdit,
                 Gtk::Window *parent);

  void add_menus (Glib::RefPtr<Gtk::UIManager> m_refUIManager);
  void configureAppBar (Gnome::UI::AppBar *s, Gtk::ProgressBar* p,
                        Gtk::Button *b);

  bool            closeProject();

  unsigned long   playPosition();

  unsigned long   getDelay();

  bool            appendTrack(const char* file);
  bool            appendTracks(std::list<std::string>&);
  bool            appendFiles(std::list<std::string>&);
  bool            insertFiles(std::list<std::string>&);

  PlayStatus      playStatus() { return playStatus_; }

  // Controls for app bar
  void            cancelEnable(bool);
  sigc::signal0<void> signalCancelClicked;

 protected:
  Gtk::Button* buttonPlay_;
  Gtk::Button* buttonStop_;
  Gtk::Button* buttonPause_;

  virtual void on_play_clicked();
  virtual void on_stop_clicked();
  virtual void on_pause_clicked();
  virtual void on_select_clicked();
  virtual void on_zoom_clicked();
  virtual void on_zoom_in_clicked();
  virtual void on_zoom_out_clicked();
  virtual void on_zoom_fit_clicked();
  virtual void on_cancel_clicked();

  virtual void status(const char* msg);
  virtual void errorDialog(const char* msg); 
  virtual void progress(double val);
  virtual void fullView();
  virtual void sampleSelect(unsigned long, unsigned long);

  void playStart();
  void playStart(unsigned long start, unsigned long end);
  void playPause();
  void playStop();

private:
  TocReader tocReader;

  SoundIF *soundInterface_;
  unsigned long playLength_; // remaining play length
  unsigned long playBurst_;
  unsigned long playPosition_;
  Sample *playBuffer_;
  bool playAbort_;

  bool playCallback();

  Gtk::HBox      hbox_;
  AudioCDView*   audioCDView_;
  TocInfoDialog* tocInfoDialog_;
  CdTextDialog*  cdTextDialog_;
  Glib::RefPtr<Gtk::ActionGroup> m_refActionGroup;
  Glib::RefPtr<Gtk::ToggleAction> selectToggle_;
  Glib::RefPtr<Gtk::ToggleAction> zoomToggle_;
  void recordToc2CD();
  void projectInfo();
  void cdTextDialog();
  void update(unsigned long level);

  enum PlayStatus playStatus_;
};
#endif
