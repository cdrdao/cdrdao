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
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "SoundIF.h"
#include "TrackData.h"
#include "Toc.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "util.h"
#include "MDIWindow.h"
#include "AddFileDialog.h"
#include "AddSilenceDialog.h"
#include "TocInfoDialog.h"
#include "TrackInfoDialog.h"
#include "RecordGenericDialog.h"
#include "CdTextDialog.h"

#include "SampleDisplay.h"

AudioCDChild::AudioCDChild(TocEdit *tocEdit, gint number)
{
  char buf[20];

//  tocEdit_ = new TocEdit(NULL, NULL);
  tocEdit_ = tocEdit;
  Toc *toc = new Toc;
  sprintf(buf, "unnamed-%i.toc", number);
//FIXME  tocEdit_->toc(toc, buf);

  playing_ = 0;
  playBurst_ = 588 * 10;
  soundInterface_ = new SoundIF;
  playBuffer_ = new Sample[playBurst_];
  soundInterface_ = NULL;

// These are not created until first needed, for faster startup
// and less memory usage.
  tocInfoDialog_ = 0;
  trackInfoDialog_ = 0;
  addFileDialog_ = 0;
  addSilenceDialog_ = 0;
  cdTextDialog_ = 0;

  saveFileSelector_ = 0;  

  views = g_list_alloc();

//FIXME  set_name(tocEdit_->filename());

  vector<Gnome::UI::Info> menus, audioEditMenuTree, viewMenuTree;

  // Edit menu
  //
  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Project Info..."),
			      slot(this, &AudioCDChild::projectInfo),
			      N_("Edit global project data")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Track Info..."),
			      slot(this, &AudioCDChild::trackInfo),
			      N_("Edit track data")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("CD-TEXT..."),
			      slot(this, &AudioCDChild::cdTextDialog),
			      N_("Edit CD-TEXT data")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(Gnome::UI::Icon(GNOME_STOCK_MENU_CUT),
			      N_("Cut"),
			      slot(this, &AudioCDChild::cutTrackData),
			      N_("Cut out selected samples")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(Gnome::UI::Icon(GNOME_STOCK_MENU_PASTE),
			      N_("Paste"),
			      slot(this,
				   &AudioCDChild::pasteTrackData),
			      N_("Paste previously cut samples")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Add Track Mark"),
			      slot(this, &AudioCDChild::addTrackMark),
			      N_("Add track marker at current marker position")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Add Index Mark"),
			      slot(this, &AudioCDChild::addIndexMark),
			      N_("Add index marker at current marker position")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Add Pre-Gap"),
			      slot(this, &AudioCDChild::addPregap),
			      N_("Add pre-gap at current marker position")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Remove Track Mark"),
			      slot(this, &AudioCDChild::removeTrackMark),
			      N_("Remove selected track/index marker or pre-gap")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Append Track"),
			      slot(this, &AudioCDChild::appendTrack),
			      N_("Append track with data from audio file")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Append File"),
			      slot(this, &AudioCDChild::appendFile),
			      N_("Append data from audio file to last track")));
  
  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Insert File"),
			      slot(this, &AudioCDChild::insertFile),
			      N_("Insert data from audio file at current marker position")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Append Silence"),
			      slot(this, &AudioCDChild::appendSilence),
			      N_("Append silence to last track")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Insert Silence"),
			      slot(this, &AudioCDChild::insertSilence),
			      N_("Insert silence at current marker position")));

/*
//FIXME
  viewMenuTree.
    push_back(Gnome::UI::Item(Gnome::UI::Icon(GNOME_STOCK_MENU_BLANK),
			      N_("Add view"),
			      slot(this,
				   &AudioCDChild::new_view),
			      N_("Add new view of current project")));
*/

  menus.push_back(Gnome::Menus::Edit(audioEditMenuTree));
  menus.push_back(Gnome::Menus::View(viewMenuTree));

//FIXME  create_menus(menus);
}


AudioCDChildLabel::AudioCDChildLabel(const string &name) :
  label(name)
{
  pixmap = manage(new Gnome::StockPixmap(GNOME_STOCK_MENU_NEW));
  pack_start(*pixmap);
  pack_start(label);
  show_all();
}


Gtk::Widget* AudioCDChild::create_title_impl()
{
//FIXME  return manage(new AudioCDChildLabel(get_name()));
}

Gtk::Widget *AudioCDChild::update_title_impl(Gtk::Widget *old_label)
{
  cout << "Inside label update" << endl;
//FIXME  static_cast <AudioCDChildLabel *>(old_label)->set_name(get_name());
  return old_label;
}

Gtk::Widget *
AudioCDChild::create_view_impl()
{
//FIXME  AudioCDView * view = manage(new AudioCDView(this));
//FIXME  view->add_view.connect(slot(this, &AudioCDChild::create_view));
//FIXME: gnome-- || C++ way
//FIXME  g_list_prepend(views, view);
//FIXME  return view;
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

//FIXME: propagating here, but it should use the UPD_XX thing
//       so the child can modify whatever...
  views = g_list_first(views);
//FIXME: g_list_foreach in C++ ?
  while (views->data)
  {
    AudioCDView *view = static_cast <AudioCDView *>(views->data);
    view->playLabel_->set_text("Stop");
    views = views->next;
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
//FIXME: propagating here, but it should use the UPD_XX thing
//       so the child can modify whatever...
    views = g_list_first(views);
//FIXME: g_list_foreach in C++ ?
    while (views->data)
    {
      AudioCDView *view = static_cast <AudioCDView *>(views->data);
      view->playLabel_->set_text("Play");
//      view->sampleDisplay_->setCursor(0, 0);
      views = views->next;
    }

    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }

  playLength_ -= len;
  playPosition_ += len;

  unsigned long delay = soundInterface_->getDelay();

  if (delay <= playPosition_) {
//FIXME: propagating here, but it should use the UPD_XX thing
//       so the child can modify whatever...
    views = g_list_first(views);
//FIMXE: g_list_foreach in C++ ?
    while (views->data)
    {
      AudioCDView *view = static_cast <AudioCDView *>(views->data);
      view->sampleDisplay_->setCursor(1, playPosition_ - delay);
      view->cursorPos_->set_text(string(sample2string(playPosition_ - delay)));
      views = views->next;
    }
  }

  if (len == 0 || playAbort_ != 0) {
    soundInterface_->end();
    tocReader.init(NULL);
    playing_ = 0;
//FIXME: propagating here, but it should use the UPD_XX thing
//       so the child can modify whatever...
    views = g_list_first(views);
//FIXME: g_list_foreach in C++ ?
    while (views->data)
    {
      AudioCDView *view = static_cast <AudioCDView *>(views->data);
      view->playLabel_->set_text("Play");
      view->sampleDisplay_->setCursor(0, 0);
      views = views->next;
    }

    tocEdit_->unblockEdit();
    guiUpdate();
    return 0; // remove idle handler
  }
  else {
    return 1; // keep idle handler
  }
}

void AudioCDChild::trackInfo()
{
  if (trackInfoDialog_ == 0)
    trackInfoDialog_ = new TrackInfoDialog();

//FIXME  GenericView *view = static_cast <GenericView *>(get_active());

//FIXME  trackInfoDialog_->start(view->tocEditView());
}

void AudioCDChild::projectInfo()
{
  if (tocInfoDialog_ == 0)
    tocInfoDialog_ = new TocInfoDialog();

//FIXME  GenericView *view = static_cast <GenericView *>(get_active());

//FIXME  tocInfoDialog_->start(view->tocEditView());
}

void AudioCDChild::cdTextDialog()
{
  if (cdTextDialog_ == 0)
    cdTextDialog_ = new CdTextDialog();

//FIXME  GenericView *view = static_cast <GenericView *>(get_active());

//FIXME  cdTextDialog_->start(view->tocEditView());
}

void AudioCDChild::record_to_cd()
{
  RECORD_GENERIC_DIALOG->toc_to_cd(tocEdit_);
}

void AudioCDChild::saveProject()
{
  if (tocEdit_->saveToc() == 0) {
//FIXME    MDI_WINDOW->statusMessage("Project saved to \"%s\".", tocEdit_->filename());
    guiUpdate();
  }
  else {
    string s("Cannot save toc to \"");
    s += tocEdit_->filename();
    s+= "\":";
    
//FIXME    MessageBox msg(MDI_WINDOW->get_active_window(), "Save Project", 0, s.c_str(), strerror(errno), NULL);
//    MessageBox msg(this, "Save Project", 0, s.c_str(), strerror(errno), NULL);
//FIXME    msg.run();
  }
}

void AudioCDChild::saveAsProject()
{
  if (saveFileSelector_)
  {
    Gdk_Window selector_win = saveFileSelector_->get_window();
    selector_win.show();
    selector_win.raise();
  }
  else
  {
    saveFileSelector_ = new Gtk::FileSelection("Save Project");
    saveFileSelector_->get_ok_button()->clicked.connect(
				slot(this, &AudioCDChild::saveFileSelectorOKCB));
    saveFileSelector_->get_cancel_button()->clicked.connect(
				slot(this, &AudioCDChild::saveFileSelectorCancelCB));
  }

  saveFileSelector_->show();
}

void AudioCDChild::saveFileSelectorCancelCB()
{
  saveFileSelector_->hide();
  saveFileSelector_->destroy();
  saveFileSelector_ = 0;
}

void AudioCDChild::saveFileSelectorOKCB()
{
  char *s = g_strdup(saveFileSelector_->get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
    if (tocEdit_->saveAsToc(stripCwd(s)) == 0) {
//FIXME  	MDI_WINDOW->statusMessage("Project saved to \"%s\".", tocEdit_->filename());
  	guiUpdate();
    }
    else {
  	string m("Cannot save toc to \"");
  	m += tocEdit_->filename();
  	m += "\":";
    
//FIXME  	MessageBox msg(MDI_WINDOW->get_active_window(), "Save Project", 0, m.c_str(), strerror(errno), NULL);
//  	MessageBox msg(this, "Save Project", 0, m.c_str(), strerror(errno), NULL);
//FIXME  	msg.run();
    }
    g_free(s);
  }
  saveFileSelectorCancelCB();
}

bool AudioCDChild::closeProject()
{

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Close Project");
    return false;
  }

  if (tocEdit_->tocDirty()) {
//FIXME: This should be Ask3Box: Save changes? "yes" "no" "cancel"
    gchar *message;
    
    message = g_strdup_printf("Project %s not saved.", tocEdit_->filename());
    
    // Ask2Box msg(this, "New", 0, 2, "Current project not saved.", "",
//FIXME    Ask2Box msg(MDI_WINDOW->get_active_window(), "Close", 0, 2, message, "",
//FIXME		"Continue?", NULL);

    g_free(message);

//FIXME    if (msg.run() != 1)
//FIXME      return false;
  }

  return true;
}

void AudioCDChild::update(unsigned long level)
{
  //cout << "updating AudioCDChild - " << get_name() << endl;

  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
    string s(tocEdit_->filename());

    if (tocEdit_->tocDirty())
      s += "(*)";

//FIXME    set_name(s);
  }

//FIXME: update all views?!?
//FIXME  GenericView *view = static_cast <GenericView *>(get_active());
//FIXME  view->update(level);

  // Update dialogs already created.
/*
  if (tocInfoDialog_ != 0)
    tocInfoDialog_->update(level, view->tocEditView());

  if (trackInfoDialog_ != 0)
    trackInfoDialog_->update(level, view->tocEditView());

  if (addFileDialog_ != 0)
    addFileDialog_->update(level, view->tocEditView());

  if (addSilenceDialog_ != 0)
    addSilenceDialog_->update(level, view->tocEditView());

  if (cdTextDialog_ != 0)
    cdTextDialog_->update(level, view->tocEditView());
*/
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

int AudioCDChild::snapSampleToBlock(unsigned long sample, long *block)
{
  unsigned long rest = sample % SAMPLES_PER_BLOCK;

  *block = sample / SAMPLES_PER_BLOCK;

  if (rest == 0) 
    return 0;

  if (rest > SAMPLES_PER_BLOCK / 2)
    *block += 1;

  return 1;
}

void AudioCDChild::trackMarkMovedCallback(const Track *, int trackNr,
					int indexNr, unsigned long sample)
{
//FIXME  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Move Track Marker");
    return;
  }

  long lba;
  int snapped = snapSampleToBlock(sample, &lba);

  switch (tocEdit_->moveTrackMarker(trackNr, indexNr, lba)) {
  case 0:
//FIXME    MDI_WINDOW->statusMessage("Moved track marker to %s%s.", Msf(lba).str(),
//FIXME			      snapped ? " (snapped to next block)" : "");
    break;

  case 6:
//FIXME    MDI_WINDOW->statusMessage("Cannot modify a data track.");
    break;
  default:
//FIXME    MDI_WINDOW->statusMessage("Illegal track marker position.");
    break;
  }

//FIXME  view->tocEditView()->trackSelection(trackNr);
//FIXME  view->tocEditView()->indexSelection(indexNr);

  guiUpdate();
}


void AudioCDChild::addTrackMark()
{
//FIXME
/*  unsigned long sample;
  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Add Track Mark");
    return;
  }

  if (view->getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (tocEdit_->addTrackMarker(lba)) {
    case 0:
      MDI_WINDOW->statusMessage("Added track mark at %s%s.", Msf(lba).str(),
				snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      MDI_WINDOW->statusMessage("Cannot add track at this point.");
      break;

    case 3:
    case 4:
      MDI_WINDOW->statusMessage("Resulting track would be shorter than 4 seconds.");
      break;

    case 5:
      MDI_WINDOW->statusMessage("Cannot modify a data track.");
      break;

    default:
      MDI_WINDOW->statusMessage("Internal error in addTrackMark(), please report.");
      break;
    }
  }
*/
}

void AudioCDChild::addIndexMark()
{
//FIXME
/*
  unsigned long sample;
  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Add Index Mark");
    return;
  }

  if (view->getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (tocEdit_->addIndexMarker(lba)) {
    case 0:
      MDI_WINDOW->statusMessage("Added index mark at %s%s.", Msf(lba).str(),
				snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      MDI_WINDOW->statusMessage("Cannot add index at this point.");
      break;

    case 3:
      MDI_WINDOW->statusMessage("Track has already 98 index marks.");
      break;

    default:
      MDI_WINDOW->statusMessage("Internal error in addIndexMark(), please report.");
      break;
    }
  }
*/
}

void AudioCDChild::addPregap()
{
//FIXME
/*
  unsigned long sample;
  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Add Pre-Gap");
    return;
  }

  if (view->getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (tocEdit_->addPregap(lba)) {
    case 0:
      MDI_WINDOW->statusMessage("Added pre-gap mark at %s%s.", Msf(lba).str(),
				snapped ? " (snapped to next block)" : "");
      guiUpdate();
      break;

    case 2:
      MDI_WINDOW->statusMessage("Cannot add pre-gap at this point.");
      break;

    case 3:
      MDI_WINDOW->statusMessage("Track would be shorter than 4 seconds.");
      break;

    case 4:
      MDI_WINDOW->statusMessage("Cannot modify a data track.");
      break;

    default:
      MDI_WINDOW->statusMessage("Internal error in addPregap(), please report.");
      break;
    }
  }
*/
}

void AudioCDChild::removeTrackMark()
{
//FIXME
/*
  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  int trackNr;
  int indexNr;

  if (!tocEdit_->editable()) {
    tocBlockedMsg("Remove Track Mark");
    return;
  }

  if (view->tocEditView()->trackSelection(&trackNr) &&
      view->tocEditView()->indexSelection(&indexNr)) {
    switch (tocEdit_->removeTrackMarker(trackNr, indexNr)) {
    case 0:
      MDI_WINDOW->statusMessage("Removed track/index marker.");
      guiUpdate();
      break;
    case 1:
      MDI_WINDOW->statusMessage("Cannot remove first track.");
      break;
    case 3:
      MDI_WINDOW->statusMessage("Cannot modify a data track.");
      break;
    default:
      MDI_WINDOW->statusMessage("Internal error in removeTrackMark(), please report.");
      break; 
    }
  }
  else {
    MDI_WINDOW->statusMessage("Please select a track/index mark.");
  }
*/
}

void AudioCDChild::appendTrack()
{
//FIXME
/*
  if (addFileDialog_ == 0)
    addFileDialog_ = new AddFileDialog();

  GenericView *view = static_cast <GenericView *>(get_active());


  addFileDialog_->mode(AddFileDialog::M_APPEND_TRACK);
  addFileDialog_->start(view->tocEditView());
*/
}

void AudioCDChild::appendFile()
{
//FIXME
/*
  if (addFileDialog_ == 0)
    addFileDialog_ = new AddFileDialog();

  GenericView *view = static_cast <GenericView *>(get_active());

  addFileDialog_->mode(AddFileDialog::M_APPEND_FILE);
  addFileDialog_->start(view->tocEditView());
*/
}

void AudioCDChild::insertFile()
{
//FIXME
/*
  if (addFileDialog_ == 0)
    addFileDialog_ = new AddFileDialog();

  GenericView *view = static_cast <GenericView *>(get_active());

  addFileDialog_->mode(AddFileDialog::M_INSERT_FILE);
  addFileDialog_->start(view->tocEditView());
*/
}

void AudioCDChild::appendSilence()
{
//FIXME
/*
  if (addSilenceDialog_ == 0)
    addSilenceDialog_ = new AddSilenceDialog();

  GenericView *view = static_cast <GenericView *>(get_active());

  addSilenceDialog_->mode(AddSilenceDialog::M_APPEND);
  addSilenceDialog_->start(view->tocEditView());
*/
}

void AudioCDChild::insertSilence()
{
//FIXME
/*
  if (addSilenceDialog_ == 0)
    addSilenceDialog_ = new AddSilenceDialog();

  GenericView *view = static_cast <GenericView *>(get_active());

  addSilenceDialog_->mode(AddSilenceDialog::M_INSERT);
  addSilenceDialog_->start(view->tocEditView());
*/
}

void AudioCDChild::cutTrackData()
{
//FIXME
/*
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Cut");
    return;
  }

  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  switch (tocEdit_->removeTrackData(view->tocEditView())) {
  case 0:
    MDI_WINDOW->statusMessage("Removed selected samples.");
    guiUpdate();
    break;
  case 1:
    MDI_WINDOW->statusMessage("Please select samples.");
    break;
  case 2:
    MDI_WINDOW->statusMessage("Selected sample range crosses track boundaries.");
    break;
  }
*/
}

void AudioCDChild::pasteTrackData()
{
//FIXME
/*
  if (!tocEdit_->editable()) {
    tocBlockedMsg("Paste");
    return;
  }

  AudioCDView *view = static_cast <AudioCDView *>(get_active());

  switch (tocEdit_->insertTrackData(view->tocEditView())) {
  case 0:
    MDI_WINDOW->statusMessage("Pasted samples.");
    guiUpdate();
    break;
  case 1:
    MDI_WINDOW->statusMessage("No samples in scrap.");
    break;
  }
*/
}

