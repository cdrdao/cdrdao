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

#include <gtk--.h>
#include <gtk/gtk.h>
#include <gnome--.h>

class Toc;
class Track;
class Sample;
class SoundIF;
class AudioCDChild;
class AudioCDView;
class TocInfoDialog;
class CdTextDialog;
class TocEdit;
#include "Project.h"

class AudioCDProject : public Project
{
public:
  enum PlayStatus {PLAYING, PAUSED, STOPPED};

private:
  TocReader tocReader;

  SoundIF *soundInterface_;
  unsigned long playLength_; // remaining play length
  unsigned long playBurst_;
  unsigned long playPosition_;
  Sample *playBuffer_;
  int playing_;
  int playAbort_;

  int playCallback();

  AudioCDChild *audioCDChild_;
  TocInfoDialog *tocInfoDialog_;
  CdTextDialog *cdTextDialog_;
  void recordToc2CD();
  void projectInfo();
  void cdTextDialog();
  void update(unsigned long level);

  Gtk::Toolbar *playToolbar;
  enum PlayStatus playStatus_;
 
public:
  AudioCDProject(int number, const char *name, TocEdit *tocEdit);
  void newAudioCDView(); 
  bool closeProject();
  Gtk::Toolbar *getPlayToolbar();

  void playStart(unsigned long start, unsigned long end);
  void playPause();
  void playStop();
  enum PlayStatus getPlayStatus();
  unsigned long playPosition();
  unsigned long getDelay();
};
#endif

