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

#include "AudioCDProject.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "TocInfoDialog.h"
#include "CdTextDialog.h"
#include "guiUpdate.h"
#include "util.h"

AudioCDProject::AudioCDProject(int number, const char *name, TocEdit *tocEdit)
{
  projectNumber_ = number;

  tocInfoDialog_ = 0;
  cdTextDialog_ = 0;

  if (tocEdit == NULL)
    tocEdit_ = new TocEdit(NULL, NULL);
  else
    tocEdit_ = tocEdit;

  if (strlen(name) == 0)
  {
    char buf[20];
    sprintf(buf, "unnamed-%i.toc", projectNumber_);
    tocEdit_->filename(buf);
    new_ = true;
  }
  else
    new_ = false; // The project file already exists
  
  updateWindowTitle();

  // Menu Stuff
  {
    using namespace Gnome::UI;
    vector<Info> menus, viewMenuTree;

    menus.push_back(Item(Icon(GNOME_STOCK_MENU_PROP),
    				 N_("CD-TEXT..."),
			      slot(this, &AudioCDProject::cdTextDialog),
			      N_("Edit CD-TEXT data")));
    insert_menus("Edit/Project Info...", menus);
  }
  
  install_menu_hints();

// Note: We must show before adding DockItems, because showing a Gnome::App
// seems to also show all the DockItems it contains!
  show();

  add_docked(*viewSwitcher_, "viewSwitcher", GNOME_DOCK_ITEM_BEH_NORMAL,
  		GNOME_DOCK_TOP, 2, 1, 0);
 
  get_dock_item_by_name("viewSwitcher")->show();

  audioCDChild_ = new AudioCDChild(this);

  newAudioCDView();
}

void AudioCDProject::newAudioCDView()
{
  Gnome::StockPixmap *pixmap = new Gnome::StockPixmap(GNOME_STOCK_MENU_CDROM);
  Gtk::Label *label = new Gtk::Label("Track Editor");
  AudioCDView *audioCDView = audioCDChild_->newView();
  hbox->pack_start(*audioCDView, TRUE, TRUE);
  audioCDView->tocEditView()->sampleViewFull();
  viewSwitcher_->addView(audioCDView->widgetList, pixmap, label);
}

bool AudioCDProject::closeProject()
{
  if (audioCDChild_->closeProject())
  {
    delete audioCDChild_;
//FIXME: We should close also the Project Info Dialog, ...
//       Something like:
//  if (tocInfoDialog_)
//    delete TocInfoDialog();
// or perhaps better in audioCDChild->closeProject()
    return true;
  }
  return false;  // Do not close the project
}

void AudioCDProject::recordToc2CD()
{
  audioCDChild_->record_to_cd();
}

void AudioCDProject::projectInfo()
{
  if (tocInfoDialog_ == 0)
    tocInfoDialog_ = new TocInfoDialog();

  tocInfoDialog_->start(tocEdit_);
}

void AudioCDProject::cdTextDialog()
{
  if (cdTextDialog_ == 0)
    cdTextDialog_ = new CdTextDialog();

  cdTextDialog_->start(tocEdit_);
}

void AudioCDProject::update(unsigned long level)
{
//FIXME: Here we should update the menus and the icons
//       this is, enabled/disabled.

  level |= tocEdit_->updateLevel();

  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA))
    updateWindowTitle();

  audioCDChild_->update(level);

  if (tocInfoDialog_ != 0)
    tocInfoDialog_->update(level, tocEdit_);

  if (cdTextDialog_ != 0)
    cdTextDialog_->update(level, tocEdit_);
}


