/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
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
#include <fstream.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <strstream.h>

#include "xcdrdao.h"
#include "guiUpdate.h"
#include "MessageBox.h"
#include "AudioCDProject.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "SoundIF.h"
#include "TrackData.h"
#include "Toc.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "util.h"
#include "MDIWindow.h"
#include "RecordGenericDialog.h"

#include "SampleDisplay.h"

AudioCDChild::AudioCDChild(AudioCDProject *project)
{
  project_ = project;
  tocEdit_ = project->tocEdit();

  playing_ = 0;
  playBurst_ = 588 * 10;
  soundInterface_ = new SoundIF;
  playBuffer_ = new Sample[playBurst_];
  soundInterface_ = NULL;

  // Menu Stuff
  {
    using namespace Gnome::UI;
    vector<Info> menus, viewMenuTree;

    viewMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_BLANK),
			      N_("Add new track editor view"),
			      slot(project, &AudioCDProject::newAudioCDView),
			      N_("Add new view of current project")));

    menus.push_back(Gnome::Menus::View(viewMenuTree));

    project->insert_menus("Edit", menus);
  }
}

void AudioCDChild::play(unsigned long start, unsigned long end)
{
  if (playing_) {
    playAbort_ = 1;
    return;
  }

  if (tocEdit_->lengthSample() == 0)
    return;

  if (soundInterface_ == NULL) {
    soundInterface_ = new SoundIF;
    if (soundInterface_->init() != 0) {
      delete soundInterface_;
      soundInterface_ = NULL;
      return;
    }
  }

  if (soundInterface_->start() != 0)
    return;

  tocReader.init(tocEdit_->toc());
  if (tocReader.openData() != 0) {
    tocReader.init(NULL);
    soundInterface_->end();
    return;
    }

  if (tocReader.seekSample(start) != 0) {
    tocReader.init(NULL);
    soundInterface_->end();
    return;
  }

  playLength_ = end - start + 1;
  playPosition_ = start;
  playing_ = 1;
  playAbort_ = 0;

// FIXME: Need this one?:
//  tocEdit_->updateLevel_ |= UPD_CURSOR_POS;

//FIXME: use FLAG
  for (list<AudioCDView *>::iterator i = views.begin();
       i != views.end(); i++)
  {
    (*i)->playLabel_->set_text("Stop");
  }

//FIXME: Selection / Zooming does not depend
//       on the Child, but the View.
//       we should have different blocks!
  tocEdit_->blockEdit();

  guiUpdate();

  Gtk::Main::idle.connect(slot(this, &AudioCDChild::playCallback));
}

int AudioCDChild::playCallback()
{
  long len = playLength_ > playBurst_ ? playBurst_ : playLength_;


  if (tocReader.readSamples(playBuffer_, len) != len ||
      soundInterface_->play(playBuffer_, len) != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
//FIXME: use FLAG
    for (list<AudioCDView *>::iterator i = views.begin();
         i != views.end(); i++)
    {
      (*i)->playLabel_->set_text("Play");
    }
    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }

  playLength_ -= len;
  playPosition_ += len;

  unsigned long delay = soundInterface_->getDelay();

  if (delay <= playPosition_) {
//FIXME: use FLAG
    for (list<AudioCDView *>::iterator i = views.begin();
         i != views.end(); i++)
    {
      (*i)->sampleDisplay_->setCursor(1, playPosition_ - delay);
      (*i)->cursorPos_->set_text(string(sample2string(playPosition_ - delay)));
    }
  }

  if (len == 0 || playAbort_ != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
//FIXME: use FLAG
    for (list<AudioCDView *>::iterator i = views.begin();
         i != views.end(); i++)
    {
      (*i)->sampleDisplay_->setCursor(0, 0);
      (*i)->playLabel_->set_text("Play");
    }

    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }
  else {
    return 1; // keep idle handler
  }
}

void AudioCDChild::record_to_cd()
{
  RECORD_GENERIC_DIALOG->toc_to_cd(tocEdit_);
}


bool AudioCDChild::closeProject()
{

  if (!tocEdit_->editable()) {
    project_->tocBlockedMsg("Close Project");
    return false;
  }

  if (tocEdit_->tocDirty()) {
//FIXME: This should be Ask3Box: Save changes? "yes" "no" "cancel"
    gchar *message;
    
    message = g_strdup_printf("Project %s not saved.", tocEdit_->filename());

    Ask2Box msg(project_, "Close", 0, 2, message, "",
		"Continue?", NULL);
    g_free(message);

    if (msg.run() != 1)
      return false;
  }

  return true;
}

void AudioCDChild::update(unsigned long level)
{
  for (list<AudioCDView *>::iterator i = views.begin();
       i != views.end(); i++)
  {
    (*i)->update(level);
  }
}


const char *AudioCDChild::sample2string(unsigned long sample)
{
  static char buf[50];

  unsigned long min = sample / (60 * 44100);
  sample %= 60 * 44100;

  unsigned long sec = sample / 44100;
  sample %= 44100;

  unsigned long frame = sample / 588;
  sample %= 588;

  sprintf(buf, "%2lu:%02lu:%02lu.%03lu", min, sec, frame, sample);
  
  return buf;
}

unsigned long AudioCDChild::string2sample(const char *str)
{
  int m = 0;
  int s = 0;
  int f = 0;
  int n = 0;

  sscanf(str, "%d:%d:%d.%d", &m, &s, &f, &n);

  if (m < 0)
    m = 0;

  if (s < 0 || s > 59)
    s = 0;

  if (f < 0 || f > 74)
    f = 0;

  if (n < 0 || n > 587)
    n = 0;

  return Msf(m, s, f).samples() + n;
}

AudioCDView *AudioCDChild::newView()
{
  AudioCDView *audioCDView = new AudioCDView(this, project_);
  views.push_back(audioCDView);
  return audioCDView;
}

