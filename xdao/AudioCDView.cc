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

#include <gtkmm.h>
#include <gnome.h>
#include <glibmm/convert.h>
#include <iostream>

#include "config.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "MessageBox.h"
#include "SampleDisplay.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "util.h"

#include "AudioCDProject.h"
#include "AudioCDView.h"
#include "Project.h"
#include "TrackInfoDialog.h"
#include "AddFileDialog.h"
#include "AddSilenceDialog.h"

AudioCDView::AudioCDView(AudioCDProject *project) 
    : addFileDialog_(project)
{
  project_ = project;
  tocEditView_ = new TocEditView(project->tocEdit());

// These are not created until first needed, for faster startup
// and less memory usage.
  trackInfoDialog_ = 0;
  addSilenceDialog_ = 0;

  std::list<Gtk::TargetEntry> drop_types;

  drop_types.push_back(Gtk::TargetEntry("text/uri-list", Gtk::TargetFlags(0),
                                        TARGET_URI_LIST));

  drag_dest_set(drop_types);
  signal_drag_data_received().
      connect(sigc::mem_fun(*this, &AudioCDView::drag_data_received_cb));

  sampleDisplay_ = new SampleDisplay;
  sampleDisplay_->setTocEdit(project->tocEdit());

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
  markerPos_->signal_activate().
      connect(sigc::mem_fun(*this, &AudioCDView::markerSet));

  cursorPos_ = new Gtk::Label;
  cursorPos_->set_size_request(entry_width, -1);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
  selectionStartPos_->set_size_request(entry_width, -1);
  selectionStartPos_->signal_activate().
      connect(sigc::mem_fun(*this, &AudioCDView::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
  selectionEndPos_->set_size_request(entry_width, -1);
  selectionEndPos_->signal_activate().
      connect(sigc::mem_fun(*this, &AudioCDView::selectionSet));

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
  pack_start(*selectionInfoBox, FALSE, FALSE);
  selectionInfoBox->show();

  setMode(SELECT);

  sampleDisplay_->markerSet.connect(sigc::mem_fun(*this,
                        &AudioCDView::markerSetCallback));
  sampleDisplay_->selectionSet.connect(sigc::mem_fun(*this,
                        &AudioCDView::selectionSetCallback));
  sampleDisplay_->selectionCleared.connect(sigc::mem_fun(*this,
                        &AudioCDView::selectionClearedCallback));
  sampleDisplay_->cursorMoved.connect(sigc::mem_fun(*this,
  			&AudioCDView::cursorMovedCallback));
  sampleDisplay_->trackMarkSelected.connect(sigc::mem_fun(*this,
  			&AudioCDView::trackMarkSelectedCallback));
  sampleDisplay_->trackMarkMoved.connect(sigc::mem_fun(*this,
  			&AudioCDView::trackMarkMovedCallback));
  sampleDisplay_->viewModified.connect(sigc::mem_fun(*this,
		        &AudioCDView::viewModifiedCallback));

  tocEditView_->sampleViewFull();
}

AudioCDView::~AudioCDView()
{
  if (trackInfoDialog_)
    delete trackInfoDialog_;

  if (addSilenceDialog_)
    delete addSilenceDialog_;
}

void AudioCDView::add_menus(Glib::RefPtr<Gtk::UIManager> m_refUIManager)
{
  m_refActionGroup = Gtk::ActionGroup::create("AudioCDView");

  m_refActionGroup->add( Gtk::Action::create("TrackInfo", Gtk::Stock::PROPERTIES,
                         _("Track Info..."),
                         _("Edit track data")),
                         sigc::mem_fun(*this, &AudioCDView::trackInfo) );

  m_refActionGroup->add( Gtk::Action::create("Cut", Gtk::Stock::CUT,
                         _("Cut"),
                         _("Cut out selected samples")),
                         Gtk::AccelKey("<control>x"),
                         sigc::mem_fun(*this, &AudioCDView::cutTrackData) );

  m_refActionGroup->add( Gtk::Action::create("Paste", Gtk::Stock::PASTE,
                         _("Paste"),
                         _("Paste previously cut samples")),
                         Gtk::AccelKey("<control>v"),
                         sigc::mem_fun(*this, &AudioCDView::pasteTrackData) );

  m_refActionGroup->add( Gtk::Action::create("SelectAll",
                         _("Select All"),
                         _("Select entire sample")),
                         Gtk::AccelKey("<control>a"),
                         sigc::mem_fun(*this, &AudioCDView::selectAll) );

  m_refActionGroup->add( Gtk::Action::create("AddTrackMark",
                         _("Add Track Mark"),
                         _("Add track marker at current marker position")),
                         Gtk::AccelKey("<control>m"),
                         sigc::mem_fun(*this, &AudioCDView::addTrackMark) );

  m_refActionGroup->add( Gtk::Action::create("AddIndexMark",
                         _("Add Index Mark"),
                         _("Add index marker at current marker position")),
                         sigc::mem_fun(*this, &AudioCDView::addIndexMark) );

  m_refActionGroup->add( Gtk::Action::create("AddPreGap",
                         _("Add Pre-Gap"),
                         _("Add pre-gap at current marker position")),
                         sigc::mem_fun(*this, &AudioCDView::addPregap) );

  m_refActionGroup->add( Gtk::Action::create("RemoveTrackMark",
                         _("Remove Track Mark"),
                         _("Remove selected track/index marker or pre-gap")),
                         Gtk::AccelKey("<control>D"),
                         sigc::mem_fun(*this, &AudioCDView::removeTrackMark) );

  m_refActionGroup->add( Gtk::Action::create("AppendTrack",
                         _("Append Track"),
                         _("Append track with data from audio file")),
                         Gtk::AccelKey("<control>T"),
                         sigc::mem_fun(*this, &AudioCDView::appendTrack) );

  m_refActionGroup->add( Gtk::Action::create("AppendFile",
                         _("Append File"),
                         _("Append data from audio file to last track")),
                         Gtk::AccelKey("<control>F"),
                         sigc::mem_fun(*this, &AudioCDView::appendFile) );

  m_refActionGroup->add( Gtk::Action::create("InsertFile",
                         _("Insert File"),
                         _("Insert data from audio file at current marker position")),
                         Gtk::AccelKey("<control>I"),
                         sigc::mem_fun(*this, &AudioCDView::insertFile) );

  m_refActionGroup->add( Gtk::Action::create("AppendSilence",
                         _("Append Silence"),
                         _("Append silence to last track")),
                         sigc::mem_fun(*this, &AudioCDView::appendSilence) );

  m_refActionGroup->add( Gtk::Action::create("InsertSilence",
                         _("Insert Silence"),
                         _("Insert silence at current marker position")),
                         sigc::mem_fun(*this, &AudioCDView::insertSilence) );

  m_refUIManager->insert_action_group(m_refActionGroup);

  // Merge menuitems
  try
  {
    Glib::ustring ui_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
        "    <menu action='EditMenu'>"
        "      <menuitem action='TrackInfo'/>"
        "    <separator/>"
        "      <menuitem action='Cut'/>"
        "      <menuitem action='Paste'/>"
        "    <separator/>"
        "      <menuitem action='SelectAll'/>"
        "    <separator/>"
        "      <menuitem action='AddTrackMark'/>"
        "      <menuitem action='AddIndexMark'/>"
        "      <menuitem action='AddPreGap'/>"
        "      <menuitem action='RemoveTrackMark'/>"
        "    <separator/>"
        "      <menuitem action='AppendTrack'/>"
        "      <menuitem action='AppendFile'/>"
        "      <menuitem action='InsertFile'/>"
        "    <separator/>"
        "      <menuitem action='AppendSilence'/>"
        "      <menuitem action='InsertSilence'/>"
        "    </menu>"
        "  </menubar>"
        "</ui>";

    m_refUIManager->add_ui_from_string(ui_info);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << "merging menus failed: " <<  ex.what();
  }
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
      markerPos_->set_text(sample2string(marker));
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
      selectionStartPos_->set_text(sample2string(start));
      selectionEndPos_->set_text(sample2string(end));
      sampleDisplay_->setRegion(start, end);
    } else {
      selectionStartPos_->set_text("");
      selectionEndPos_->set_text("");
      sampleDisplay_->clearRegion();
    }
  }

  if (level & UPD_PLAY_STATUS) {
    switch (project_->playStatus()) {
      case AudioCDProject::PLAYING:
        sampleDisplay_->setCursor(1, project_->playPosition() -
                                  project_->getDelay());
        // FIXME: What about using a separate cursor for playing?
        cursorPos_->set_text(sample2string(project_->playPosition() -
                                           project_->getDelay()));
        break;
      case AudioCDProject::PAUSED:
        sampleDisplay_->setCursor(1, project_->playPosition() -
                                  project_->getDelay());
        // FIXME: What about using a separate cursor for playing?
        cursorPos_->set_text(sample2string(project_->playPosition() -
                                           project_->getDelay()));
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
    if (tocEditView_->sampleView(start, end)) {
      update(UPD_SAMPLES);
    }
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

  if (tocEditView_->sampleView(start, end)) {
    update(UPD_SAMPLES);
  }
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

  if (tocEditView_->sampleView(start, end)) {
    update(UPD_SAMPLES);
  }
}

void AudioCDView::fullView()
{
  tocEditView_->sampleViewFull();
  update(UPD_SAMPLES);
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
  update(UPD_TRACK_MARK_SEL);
}

// Called when the user clicks on the SampleDisplay
void AudioCDView::markerSetCallback(unsigned long sample)
{
  tocEditView_->sampleMarker(sample);
  update(UPD_SAMPLE_MARKER);
}

// Called when the user makes a selection on the SampleDisplay
void AudioCDView::selectionSetCallback(unsigned long start,
                                       unsigned long end)
{
  if (mode_ == ZOOM ) {
    if (tocEditView_->sampleView(start, end)) {
      update(UPD_SAMPLES);
    }
  }
  else {
    tocEditView_->sampleSelect(start, end);
    update(UPD_SAMPLE_SEL);
  }
}

void AudioCDView::selectAll()
{
  tocEditView_->sampleSelectAll();
  update(UPD_SAMPLE_SEL);
}

void AudioCDView::selectionClearedCallback()
{
  if (mode_ != ZOOM) {
    if (tocEditView_->sampleSelectionClear()) {
      update(UPD_SAMPLE_SEL);
    }
  }
}

void AudioCDView::cursorMovedCallback(unsigned long pos)
{
  cursorPos_->set_text(sample2string(pos));
}

void AudioCDView::viewModifiedCallback(unsigned long start, unsigned long end)
{
  if (tocEditView_->sampleView(start, end)) {
    update(UPD_SAMPLES);
  }
}

void AudioCDView::setMode(Mode m)
{
  mode_ = m;
}

// Called when the user enters a value in the marker entry
void AudioCDView::markerSet()
{
  unsigned long s = string2sample(markerPos_->get_text().c_str());

  tocEditView_->sampleMarker(s);
  update(UPD_SAMPLE_MARKER);
}

// Called when the user enters a value in one of the two selection entries
void AudioCDView::selectionSet()
{
  unsigned long s1 =
    string2sample(selectionStartPos_->get_text().c_str());
  unsigned long s2 =
    string2sample(selectionEndPos_->get_text().c_str());

  tocEditView_->sampleSelect(s1, s2);
  update(UPD_SAMPLE_SEL);
}

void
AudioCDView::drag_data_received_cb(const Glib::RefPtr<Gdk::DragContext>&
                                   context, int x, int y,
                                   const Gtk::SelectionData& selection_data,
                                   guint info, guint time)
{
  switch (info) {

  case TARGET_URI_LIST:

    if (project_->playStatus() != AudioCDProject::STOPPED)
      return;

    std::string list = selection_data.get_data_as_string();
    int idx = 0, n;

    while ((n = list.find("\r\n", idx)) >= 0) {
      std::string sub = list.substr(idx, n - idx);
      idx = n + 2;

      std::string fn;
      try {
        fn = Glib::filename_from_uri(sub);
      } catch (std::exception& e) {
        fn.clear();
      }

      if (fn.empty())
        continue;

      // Process m3u file.
      FileExtension type = fileExtension(fn.c_str());

      if (type == FE_WAV || type == FE_M3U
#ifdef HAVE_MP3_SUPPORT
          || type == FE_MP3
#endif
#ifdef HAVE_OGG_SUPPORT
          || type == FE_OGG
#endif
          ) {
        project_->appendTrack(fn.c_str());
      }
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

    Gtk::MessageDialog md(*project_->getParentWindow (), _("Please select a track first"),
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
    signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL | UPD_SAMPLE_MARKER | UPD_SAMPLES);
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
    signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL);
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
      signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
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
      signal_tocModified(UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
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
      signal_tocModified(UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
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
      signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
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

  update(UPD_TRACK_MARK_SEL);
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
    addSilenceDialog_->set_transient_for(*project_->getParentWindow ());
    addSilenceDialog_->signal_tocModified.
      connect(sigc::mem_fun(*this, &AudioCDView::update));
    addSilenceDialog_->signal_fullView.
      connect(sigc::mem_fun(*this, &AudioCDView::fullView));
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

const char *AudioCDView::sample2string(unsigned long sample)
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

unsigned long AudioCDView::string2sample(const char *str)
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
