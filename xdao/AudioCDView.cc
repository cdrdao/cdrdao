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

#include <iostream>

#include "xcdrdao.h"
#include "guiUpdate.h"
#include "MessageBox.h"
#include "SampleDisplay.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "util.h"

#include "AudioCDProject.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "Project.h"
#include "TrackInfoDialog.h"
#include "AddFileDialog.h"
#include "AddSilenceDialog.h"
#include <gtkmm.h>
#include <gnome.h>

AudioCDView::AudioCDView(AudioCDChild *child, AudioCDProject *project) 
    : addFileDialog_(project)
{
  char buf[20];
  gint viewNumber = project->getViewNumber();
  cdchild = child;
  project_ = project;
  tocEditView_ = new TocEditView(project->tocEdit());

// These are not created until first needed, for faster startup
// and less memory usage.
  trackInfoDialog_ = 0;
  addSilenceDialog_ = 0;

  std::list<Gtk::TargetEntry> drop_types;

  drop_types.push_back(Gtk::TargetEntry("text/uri-list", 0, TARGET_URI_LIST));

  drag_dest_set(drop_types,
                Gtk::DEST_DEFAULT_MOTION |
                Gtk::DEST_DEFAULT_HIGHLIGHT |
                Gtk::DEST_DEFAULT_DROP);

  signal_drag_data_received().connect(slot(*this, &AudioCDView::drag_data_received_cb));

  sampleDisplay_ = new SampleDisplay;
  sampleDisplay_->setTocEdit(child->tocEdit());

  sampleDisplay_->set_size_request(200,200);
  
  pack_start(*sampleDisplay_, TRUE, TRUE);
  sampleDisplay_->modify_font(Pango::FontDescription("Monospace 8"));
  sampleDisplay_->show();

  Gtk::HScrollbar *scrollBar =
    new Gtk::HScrollbar(*(sampleDisplay_->getAdjustment()));
  pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();
  
  Gtk::Label *label;
  Gtk::HBox *selectionInfoBox = new Gtk::HBox;

//FIXME: Calculate entry width for the current font.
  gint entry_width = 90;

  markerPos_ = new Gtk::Entry;
  markerPos_->set_editable(true);
  markerPos_->set_size_request(entry_width, -1);
  markerPos_->signal_activate().connect(slot(*this, &AudioCDView::markerSet));

  cursorPos_ = new Gtk::Label;
  cursorPos_->set_size_request(entry_width, -1);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
  selectionStartPos_->set_size_request(entry_width, -1);
  selectionStartPos_->signal_activate().
      connect(slot(*this, &AudioCDView::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
  selectionEndPos_->set_size_request(entry_width, -1);
  selectionEndPos_->signal_activate().
      connect(slot(*this, &AudioCDView::selectionSet));

  label = new Gtk::Label(_("Cursor: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*cursorPos_);
  label->show();
  cursorPos_->show();

  label = new Gtk::Label(_("Marker: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*markerPos_);
  label->show();
  markerPos_->show();

  label = new Gtk::Label(_("Selection: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*selectionStartPos_);
  label->show();
  selectionStartPos_->show();

  label = new Gtk::Label(" - ");
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*selectionEndPos_);
  label->show();
  selectionEndPos_->show();
  
  selectionInfoBox->set_border_width(2);
  sprintf(buf, "selectionBox-%i", viewNumber);
  project->add_docked(*selectionInfoBox, buf,
                      BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL,
                      BONOBO_DOCK_BOTTOM, 1, 1, 0);
  Gtk::Widget* w = Glib::wrap(GTK_WIDGET(project->get_dock_item_by_name(buf)));
  selectionInfoBox->show();

  setMode(SELECT);

  sampleDisplay_->markerSet.connect(slot(*this,
                        &AudioCDView::markerSetCallback));
  sampleDisplay_->selectionSet.connect(slot(*this,
                        &AudioCDView::selectionSetCallback));
  sampleDisplay_->selectionCleared.connect(slot(*this,
                        &AudioCDView::selectionClearedCallback));
  sampleDisplay_->cursorMoved.connect(slot(*this,
  			&AudioCDView::cursorMovedCallback));
  sampleDisplay_->trackMarkSelected.connect(slot(*this,
  			&AudioCDView::trackMarkSelectedCallback));
  sampleDisplay_->trackMarkMoved.connect(slot(*this,
  			&AudioCDView::trackMarkMovedCallback));
  sampleDisplay_->viewModified.connect(slot(*this,
		        &AudioCDView::viewModifiedCallback));

  tocEditView_->sampleViewFull();
  
  // Menu Stuff
  {
      using namespace Gnome::UI::Items;
      std::vector<Info> menus;
      Info info;
      int i;
    
    menus.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::PROPERTIES)),
                         _("Track Info..."),
                         slot(*this, &AudioCDView::trackInfo),
                         _("Edit track data")));
  
    menus.push_back(Separator());
  
    info = Item(Icon(Gtk::StockID(Gtk::Stock::CUT)),
                _("Cut"),
                slot(*this, &AudioCDView::cutTrackData),
                _("Cut out selected samples"));
    info.set_accel(Gtk::Menu::AccelKey("<control>x"));
    menus.push_back(info);

    info = Item(Icon(Gtk::StockID(Gtk::Stock::PASTE)),
                _("Paste"),
                slot(*this, &AudioCDView::pasteTrackData),
                _("Paste previously cut samples"));
    info.set_accel(Gtk::Menu::AccelKey("<control>v"));
    menus.push_back(info);

    menus.push_back(Separator());

    info = Item(_("Add Track Mark"),
                slot(*this, &AudioCDView::addTrackMark),
                _("Add track marker at current marker position"));
    info.set_accel(Gtk::Menu::AccelKey("T"));
    menus.push_back(info);

    info = Item(_("Add Index Mark"),
                slot(*this, &AudioCDView::addIndexMark),
                _("Add index marker at current marker position"));
    info.set_accel(Gtk::Menu::AccelKey("I"));
    menus.push_back(info);
    
    info = Item(_("Add Pre-Gap"),
                slot(*this, &AudioCDView::addPregap),
                _("Add pre-gap at current marker position"));
    info.set_accel(Gtk::Menu::AccelKey("P"));
    menus.push_back(info);

    info = Item(_("Remove Track Mark"),
                slot(*this, &AudioCDView::removeTrackMark),
                _("Remove selected track/index marker or pre-gap"));
    info.set_accel(Gtk::Menu::AccelKey("<control>D"));
    menus.push_back(info);
 
    menus.push_back(Separator());

    info = Item(_("Append Track"),
                slot(*this, &AudioCDView::appendTrack),
                _("Append track with data from audio file"));
    info.set_accel(Gtk::Menu::AccelKey("<control>T"));
    menus.push_back(info);

    info = Item(_("Append File"),
                slot(*this, &AudioCDView::appendFile),
                _("Append data from audio file to last track"));
    info.set_accel(Gtk::Menu::AccelKey("<control>F"));
    menus.push_back(info);
  
    info = Item(_("Insert File"),
                slot(*this, &AudioCDView::insertFile),
                _("Insert data from audio file at current marker position"));
    info.set_accel(Gtk::Menu::AccelKey("<control>I"));
    menus.push_back(info);

    menus.push_back(Separator());

    menus.push_back(Item(_("Append Silence"),
                         slot(*this, &AudioCDView::appendSilence),
                         _("Append silence to last track")));

    menus.push_back(Item(_("Insert Silence"),
                         slot(*this, &AudioCDView::insertSilence),
                         _("Insert silence at current marker position")));

    Array<Info>& arrayInfo = project->insert_menus(_("Edit/CD-TEXT..."),
                                                   menus);
  }
}

AudioCDView::~AudioCDView()
{
  if (trackInfoDialog_)
    delete trackInfoDialog_;

  if (addSilenceDialog_)
    delete addSilenceDialog_;
}

void AudioCDView::update(unsigned long level)
{
  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
    cursorPos_->set_text("");
  }

  if (level & UPD_TRACK_MARK_SEL) {
    int trackNr, indexNr;

    if (tocEditView_->trackSelection(&trackNr) && 
	tocEditView_->indexSelection(&indexNr)) {
      sampleDisplay_->setSelectedTrackMarker(trackNr, indexNr);
    }
    else {
      sampleDisplay_->setSelectedTrackMarker(0, 0);
    }
  }

  if (level & UPD_SAMPLES) {
    unsigned long smin, smax;

    tocEditView_->sampleView(&smin, &smax);
    sampleDisplay_->updateToc(smin, smax);
  }
  else if (level & (UPD_TRACK_DATA | UPD_TRACK_MARK_SEL)) {
    sampleDisplay_->updateTrackMarks();
  }

  if (level & UPD_SAMPLE_MARKER) {
    unsigned long marker;

    if (tocEditView_->sampleMarker(&marker)) {
      markerPos_->set_text(cdchild->sample2string(marker));
      sampleDisplay_->setMarker(marker);
    }
    else {
      markerPos_->set_text("");
      sampleDisplay_->clearMarker();
    }
  }

  if (level & UPD_SAMPLE_SEL) {
    unsigned long start, end;

    if (tocEditView_->sampleSelection(&start, &end)) {
      selectionStartPos_->set_text(cdchild->sample2string(start));
      selectionEndPos_->set_text(cdchild->sample2string(end));
      sampleDisplay_->setRegion(start, end);
    }
    else {
      selectionStartPos_->set_text("");
      selectionEndPos_->set_text("");
      sampleDisplay_->setRegion(1, 0);
    }
  }

  if (level & UPD_PLAY_STATUS) {
    switch (project_->getPlayStatus()) {
      case AudioCDProject::PLAYING:
        sampleDisplay_->setCursor(1, project_->playPosition() - project_->getDelay());
// FIXME: What about using a separate cursor for playing?
        cursorPos_->set_text(cdchild->sample2string(project_->playPosition() - project_->getDelay()));
        break;
      case AudioCDProject::PAUSED:
        sampleDisplay_->setCursor(1, project_->playPosition() - project_->getDelay());
// FIXME: What about using a separate cursor for playing?
        cursorPos_->set_text(cdchild->sample2string(project_->playPosition() - project_->getDelay()));
        break;
      case AudioCDProject::STOPPED:
        sampleDisplay_->setCursor(0, 0);
        break;
      default:
        std::cerr << "invalid play status" << std::endl;
    }
  }

  if (trackInfoDialog_ != 0)
    trackInfoDialog_->update(level, tocEditView_);

  addFileDialog_.update(level);

  if (addSilenceDialog_ != 0)
    addSilenceDialog_->update(level, tocEditView_);

}

void AudioCDView::zoomIn()
{
  unsigned long start, end;

  if (tocEditView_->sampleSelection(&start, &end)) {
    tocEditView_->sampleView(start, end);
    guiUpdate();
  }
}

void AudioCDView::zoomx2()
{
  unsigned long start, end, len, center;

  tocEditView_->sampleView(&start, &end);

  len = end - start + 1;
  center = start + len / 2;

  start = center - len / 4;
  end = center + len / 4;

  tocEditView_->sampleView(start, end);
  guiUpdate();
}

void AudioCDView::zoomOut()
{
  unsigned long start, end, len, center;

  tocEditView_->sampleView(&start, &end);

  len = end - start + 1;
  center = start + len / 2;

  if (center > len)
    start = center - len;
  else 
    start = 0;

  end = center + len;
  if (end >= tocEditView_->tocEdit()->toc()->length().samples())
    end = tocEditView_->tocEdit()->toc()->length().samples() - 1;

  tocEditView_->sampleView(start, end);
  guiUpdate();
}

void AudioCDView::fullView()
{
  tocEditView_->sampleViewFull();

  guiUpdate();
}

int AudioCDView::getMarker(unsigned long *sample)
{
  if (tocEditView_->tocEdit()->lengthSample() == 0)
    return 0;

  if (sampleDisplay_->getMarker(sample) == 0) {
    project_->statusMessage(_("Please set marker."));
    return 0;
  }

  return 1;
}

void AudioCDView::trackMarkSelectedCallback(const Track *, int trackNr,
					   int indexNr)
{
  tocEditView_->trackSelection(trackNr);
  tocEditView_->indexSelection(indexNr);
  guiUpdate();
}

void AudioCDView::markerSetCallback(unsigned long sample)
{
  tocEditView_->sampleMarker(sample);
  guiUpdate();
}

void AudioCDView::selectionSetCallback(unsigned long start,
                                       unsigned long end)
{
  if (mode_ == ZOOM ) {
    tocEditView_->sampleView(start, end);
  }
  else {
    tocEditView_->sampleSelection(start, end);
  }

  guiUpdate();
}

void AudioCDView::selectionClearedCallback()
{
  if (mode_ != ZOOM) {
    tocEditView_->sampleSelectionClear();
    guiUpdate();
  }
}

void AudioCDView::cursorMovedCallback(unsigned long pos)
{
  cursorPos_->set_text(cdchild->sample2string(pos));
}

void AudioCDView::viewModifiedCallback(unsigned long start, unsigned long end)
{
  tocEditView_->sampleView(start, end);
  guiUpdate();
}

void AudioCDView::setMode(Mode m)
{
  mode_ = m;
}

void AudioCDView::markerSet()
{
  unsigned long s = cdchild->string2sample(markerPos_->get_text().c_str());

  tocEditView_->sampleMarker(s);
  guiUpdate();
}

void AudioCDView::selectionSet()
{
  unsigned long s1 =
    cdchild->string2sample(selectionStartPos_->get_text().c_str());
  unsigned long s2 =
    cdchild->string2sample(selectionEndPos_->get_text().c_str());

  tocEditView_->sampleSelection(s1, s2);
  guiUpdate();
}

void 
AudioCDView::drag_data_received_cb(const
                                   Glib::RefPtr<Gdk::DragContext>& context,
                                   gint x, gint y,
                                   GtkSelectionData *selection_data,
                                   guint info, guint time)
{
  GList *names = NULL;
  
  switch (info) {
  case TARGET_URI_LIST:
    if (names && names->data) {
      std::string str = g_strdup(static_cast <char *>(names->data));
      const char *file = stripCwd(str.c_str());

      switch (tocEditView_->tocEdit()->appendTrack(file)) {
      case 0:
        guiUpdate();
        project_->statusMessage(_("Appended track with audio data from "
                                  "\"%s\"."), file);
        break;
      case 1:
        project_->statusMessage(_("Cannot open audio file \"%s\"."), file);
        break;
      case 2:
        project_->statusMessage(_("Audio file \"%s\" has wrong format."),
                                file);
        break;
      }
      names = g_list_remove(names, names->data);
      if (names == NULL)
        break;
    }
    break;
  }
}

void AudioCDView::trackInfo()
{
  int track;
  
  if (tocEditView_->trackSelection(&track)) {

    if (trackInfoDialog_ == 0)
      trackInfoDialog_ = new TrackInfoDialog();
  
    trackInfoDialog_->start(tocEditView_);

  } else {

    Gtk::MessageDialog md(*project_, _("Please select a track first"),
                          Gtk::MESSAGE_INFO);
    md.run();
  }
}

void AudioCDView::cutTrackData()
{
  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg("Cut");
    return;
  }

  switch (project_->tocEdit()->removeTrackData(tocEditView_)) {
  case 0:
    project_->statusMessage(_("Removed selected samples."));
    guiUpdate();
    break;
  case 1:
    project_->statusMessage(_("Please select samples."));
    break;
  case 2:
    project_->statusMessage(_("Selected sample range crosses track "
                              "boundaries."));
    break;
  }
}

void AudioCDView::pasteTrackData()
{
  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg("Paste");
    return;
  }

  switch (project_->tocEdit()->insertTrackData(tocEditView_)) {
  case 0:
    project_->statusMessage(_("Pasted samples."));
    guiUpdate();
    break;
  case 1:
    project_->statusMessage(_("No samples in scrap."));
    break;
  }
}

void AudioCDView::addTrackMark()
{
  unsigned long sample;

  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg(_("Add Track Mark"));
    return;
  }

  if (getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (project_->tocEdit()->addTrackMarker(lba)) {
    case 0:
      project_->statusMessage(_("Added track mark at %s%s."), Msf(lba).str(),
                              snapped ? _(" (snapped to next block)") : "");
      guiUpdate();
      break;

    case 2:
      project_->statusMessage(_("Cannot add track at this point."));
      break;

    case 3:
    case 4:
      project_->statusMessage(_("Resulting track would be shorter than "
                                "4 seconds."));
      break;

    case 5:
      project_->statusMessage(_("Cannot modify a data track."));
      break;

    default:
      project_->statusMessage(_("Internal error in addTrackMark(), please "
                                "report."));
      break;
    }
  }
}

void AudioCDView::addIndexMark()
{
  unsigned long sample;

  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg(_("Add Index Mark"));
    return;
  }

  if (getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (project_->tocEdit()->addIndexMarker(lba)) {
    case 0:
      project_->statusMessage(_("Added index mark at %s%s."), Msf(lba).str(),
                              snapped ? _(" (snapped to next block)") : "");
      guiUpdate();
      break;

    case 2:
      project_->statusMessage(_("Cannot add index at this point."));
      break;

    case 3:
      project_->statusMessage(_("Track has already 98 index marks."));
      break;

    default:
      project_->statusMessage(_("Internal error in addIndexMark(), "
                                "please report."));
      break;
    }
  }
}

void AudioCDView::addPregap()
{
  unsigned long sample;

  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg(_("Add Pre-Gap"));
    return;
  }

  if (getMarker(&sample)) {
    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (project_->tocEdit()->addPregap(lba)) {
    case 0:
      project_->statusMessage(_("Added pre-gap mark at %s%s."), Msf(lba).str(),
                              snapped ? _(" (snapped to next block)") : "");
      guiUpdate();
      break;

    case 2:
      project_->statusMessage(_("Cannot add pre-gap at this point."));
      break;

    case 3:
      project_->statusMessage(_("Track would be shorter than 4 seconds."));
      break;

    case 4:
      project_->statusMessage(_("Cannot modify a data track."));
      break;

    default:
      project_->statusMessage(_("Internal error in addPregap(), "
                                "please report."));
      break;
    }
  }
}

void AudioCDView::removeTrackMark()
{
  int trackNr;
  int indexNr;

  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg(_("Remove Track Mark"));
    return;
  }

  if (tocEditView_->trackSelection(&trackNr) &&
      tocEditView_->indexSelection(&indexNr)) {
    switch (project_->tocEdit()->removeTrackMarker(trackNr, indexNr)) {
    case 0:
      project_->statusMessage(_("Removed track/index marker."));
      guiUpdate();
      break;
    case 1:
      project_->statusMessage(_("Cannot remove first track."));
      break;
    case 3:
      project_->statusMessage(_("Cannot modify a data track."));
      break;
    default:
      project_->statusMessage(_("Internal error in removeTrackMark(), "
                                "please report."));
      break; 
    }
  }
  else {
    project_->statusMessage(_("Please select a track/index mark."));
  }
}

int AudioCDView::snapSampleToBlock(unsigned long sample, long *block)
{
  unsigned long rest = sample % SAMPLES_PER_BLOCK;

  *block = sample / SAMPLES_PER_BLOCK;

  if (rest == 0) 
    return 0;

  if (rest > SAMPLES_PER_BLOCK / 2)
    *block += 1;

  return 1;
}

void AudioCDView::trackMarkMovedCallback(const Track *, int trackNr,
					int indexNr, unsigned long sample)
{
  if (!project_->tocEdit()->editable()) {
    project_->tocBlockedMsg(_("Move Track Marker"));
    return;
  }

  long lba;
  int snapped = snapSampleToBlock(sample, &lba);

  switch (project_->tocEdit()->moveTrackMarker(trackNr, indexNr, lba)) {
  case 0:
    project_->statusMessage(_("Moved track marker to %s%s."), Msf(lba).str(),
                            snapped ? _(" (snapped to next block)") : "");
    break;

  case 6:
    project_->statusMessage(_("Cannot modify a data track."));
    break;
  default:
    project_->statusMessage(_("Illegal track marker position."));
    break;
  }

  tocEditView_->trackSelection(trackNr);
  tocEditView_->indexSelection(indexNr);

  guiUpdate();
}

void AudioCDView::appendTrack()
{
  addFileDialog_.mode(AddFileDialog::M_APPEND_TRACK);
  addFileDialog_.start();
}

void AudioCDView::appendFile()
{
  addFileDialog_.mode(AddFileDialog::M_APPEND_FILE);
  addFileDialog_.start();
}

void AudioCDView::insertFile()
{
  addFileDialog_.mode(AddFileDialog::M_INSERT_FILE);
  addFileDialog_.start();
}

void AudioCDView::appendSilence()
{
  if (addSilenceDialog_ == 0) {
    addSilenceDialog_ = new AddSilenceDialog();
    addSilenceDialog_->set_transient_for(*project_);
  }

  addSilenceDialog_->mode(AddSilenceDialog::M_APPEND);
  addSilenceDialog_->start(tocEditView_);
}

void AudioCDView::insertSilence()
{
  if (addSilenceDialog_ == 0)
    addSilenceDialog_ = new AddSilenceDialog();

  addSilenceDialog_->mode(AddSilenceDialog::M_INSERT);
  addSilenceDialog_->start(tocEditView_);
}
