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

#include "Toc.h"
#include "SoundIF.h"
#include "AudioCDProject.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "TocInfoDialog.h"
#include "CdTextDialog.h"
#include "guiUpdate.h"
#include "util.h"
#include "RecordTocDialog.h"

AudioCDProject::AudioCDProject(int number, const char *name, TocEdit *tocEdit)
{
  hbox = new Gtk::HBox;
  hbox->show();
  set_contents(*hbox);
  viewSwitcher_ = new ViewSwitcher(hbox);
  viewSwitcher_->show();

  projectNumber_ = number;

  tocInfoDialog_ = 0;
  cdTextDialog_ = 0;
  playStatus_ = STOPPED;
  playBurst_ = 588 * 10;
  playBuffer_ = new Sample[playBurst_];
  soundInterface_ = NULL;

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

  playToolbar = new Gtk::Toolbar;
  playToolbar->set_border_width(2);
  playToolbar->set_button_relief(GTK_RELIEF_NONE);
  playToolbar->set_style(GTK_TOOLBAR_ICONS);
  playToolbar->show();
  add_docked(*playToolbar, "playToolbar", GNOME_DOCK_ITEM_BEH_NORMAL,
  		GNOME_DOCK_TOP, 2, 2, 0);
  get_dock_item_by_name("playToolbar")->show();

  audioCDChild_ = new AudioCDChild(this);

  newAudioCDView();
  guiUpdate(UPD_ALL);
}

Gtk::Toolbar *AudioCDProject::getPlayToolbar()
{
  return playToolbar;
}

void AudioCDProject::newAudioCDView()
{
  Gnome::StockPixmap *pixmap = new Gnome::StockPixmap(GNOME_STOCK_MENU_CDROM);
  Gtk::Label *label = new Gtk::Label("Track Editor");
  AudioCDView *audioCDView = audioCDChild_->newView();
  hbox->pack_start(*audioCDView, TRUE, TRUE);
  audioCDView->tocEditView()->sampleViewFull();
  viewSwitcher_->addView(audioCDView->widgetList, pixmap, label);
  guiUpdate(UPD_ALL);
}

bool AudioCDProject::closeProject()
{
  if (audioCDChild_->closeProject())
  {
    delete audioCDChild_;
    audioCDChild_ = 0;

    if (tocInfoDialog_)
      delete tocInfoDialog_;

    if (cdTextDialog_)
      delete cdTextDialog_;

    if (recordTocDialog_)
      delete recordTocDialog_;

    return true;
  }
  return false;  // Do not close the project
}

void AudioCDProject::recordToc2CD()
{
  if (recordTocDialog_ == 0)
    recordTocDialog_ = new RecordTocDialog(tocEdit_);

  recordTocDialog_->start(this);
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

  if (audioCDChild_ != 0)
    audioCDChild_->update(level);

  if (tocInfoDialog_ != 0)
    tocInfoDialog_->update(level, tocEdit_);

  if (cdTextDialog_ != 0)
    cdTextDialog_->update(level, tocEdit_);
  if (recordTocDialog_ != 0)
    recordTocDialog_->update(level);
}

void AudioCDProject::playStart(unsigned long start, unsigned long end)
{
  unsigned long level = 0;

  if (playStatus_ == PLAYING)
    return;

  if (playStatus_ == PAUSED)
  {
    playStatus_ = PLAYING;
    Gtk::Main::idle.connect(slot(this, &AudioCDProject::playCallback));
    return;
  }

  if (tocEdit_->lengthSample() == 0)
  {
    guiUpdate(UPD_PLAY_STATUS);
    return;
  }

  if (soundInterface_ == NULL) {
    soundInterface_ = new SoundIF;
    if (soundInterface_->init() != 0) {
      delete soundInterface_;
      soundInterface_ = NULL;
      guiUpdate(UPD_PLAY_STATUS);
	  statusMessage("WARNING: Cannot open \"/dev/dsp\"");
      return;
    }
  }

  if (soundInterface_->start() != 0)
  {
    guiUpdate(UPD_PLAY_STATUS);
    return;
  }

  tocReader.init(tocEdit_->toc());
  if (tocReader.openData() != 0) {
    tocReader.init(NULL);
    soundInterface_->end();
    guiUpdate(UPD_PLAY_STATUS);
    return;
    }

  if (tocReader.seekSample(start) != 0) {
    tocReader.init(NULL);
    soundInterface_->end();
    guiUpdate(UPD_PLAY_STATUS);
    return;
  }

  playLength_ = end - start + 1;
  playPosition_ = start;
  playStatus_ = PLAYING;
  playAbort_ = 0;

  level |= UPD_PLAY_STATUS;

//FIXME: Selection / Zooming does not depend
//       on the Child, but the View.
//       we should have different blocks!
  tocEdit_->blockEdit();

  guiUpdate(level);

  Gtk::Main::idle.connect(slot(this, &AudioCDProject::playCallback));
}

void AudioCDProject::playPause()
{
  playStatus_ = PAUSED;
}

void AudioCDProject::playStop()
{
  if (getPlayStatus() == PAUSED)
  {
    soundInterface_->end();
    tocReader.init(NULL);
    playStatus_ = STOPPED;
    tocEdit_->unblockEdit();
    playStatus_ = STOPPED;
    guiUpdate(UPD_PLAY_STATUS);
  }
  else
  {
    playAbort_ = 1;
  }
}

int AudioCDProject::playCallback()
{
  unsigned long level = 0;

  long len = playLength_ > playBurst_ ? playBurst_ : playLength_;

  if (playStatus_ == PAUSED)
  {
    level |= UPD_PLAY_STATUS;
    guiUpdate(level);
    return 0; // remove idle handler
  }

  if (tocReader.readSamples(playBuffer_, len) != len ||
      soundInterface_->play(playBuffer_, len) != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playStatus_ = STOPPED;
    level |= UPD_PLAY_STATUS;
    tocEdit_->unblockEdit();
    guiUpdate(level);
    return 0; // remove idle handler
  }

  playLength_ -= len;
  playPosition_ += len;

  unsigned long delay = soundInterface_->getDelay();

  if (delay <= playPosition_)
    level |= UPD_PLAY_STATUS;

  if (len == 0 || playAbort_ != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playStatus_ = STOPPED;
    level |= UPD_PLAY_STATUS;
    tocEdit_->unblockEdit();
    guiUpdate(level);
    return 0; // remove idle handler
  }
  else {
    guiUpdate(level);
    return 1; // keep idle handler
  }
}

enum AudioCDProject::PlayStatus AudioCDProject::getPlayStatus()
{
  return playStatus_;
}

unsigned long AudioCDProject::playPosition()
{
  return playPosition_;
}

unsigned long AudioCDProject::getDelay()
{
  return soundInterface_->getDelay();
}

