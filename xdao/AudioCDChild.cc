/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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


#include "xcdrdao.h"
#include "guiUpdate.h"
#include "MessageBox.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "SoundIF.h"
#include "TrackData.h"
#include "Toc.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "util.h"
#include "AudioCDProject.h"

#include "SampleDisplay.h"

AudioCDChild::AudioCDChild(AudioCDProject *project)
{
  project_ = project;
  tocEdit_ = project->tocEdit();

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

  zoomToolbar = new Gtk::Toolbar;
  zoomToolbar->set_border_width(2);
  zoomToolbar->set_button_relief(GTK_RELIEF_NONE);
  zoomToolbar->show();
  project->add_docked(*zoomToolbar, "zoomToolbar", GNOME_DOCK_ITEM_BEH_NORMAL,
  		GNOME_DOCK_TOP, 1, 2, 0);
  project->get_dock_item_by_name("zoomToolbar")->show();

}

AudioCDChild::~AudioCDChild()
{
  AudioCDView *view = 0;

  for (std::list<AudioCDView *>::iterator i = views.begin();
       i != views.end(); i++)
  {
    if (view != 0)
      delete view;
    view = *i;   
  }
  delete view;
}

Gtk::Toolbar *AudioCDChild::getZoomToolbar()
{
	return zoomToolbar;
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
  for (std::list<AudioCDView *>::iterator i = views.begin();
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

