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
#include "SampleDisplay.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "MDIWindow.h"
#include "util.h"

#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "Project.h"
#include <gnome.h>



AudioCDView::AudioCDView(AudioCDChild *child, Project *project) 
{
  char buf[20];
  gint viewNumber = project->getViewNumber();
  cdchild = child;
  tocEditView_ = new TocEditView(child->tocEdit());

  widgetList = new list<Gtk::Widget *>;
  widgetList->push_back(this);

  Gtk::VBox *vbox = this;
  
  static const GtkTargetEntry drop_types [] =
  {
    { "text/uri-list", 0, TARGET_URI_LIST }
  };

  static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);

  drag_dest_set(static_cast <GtkDestDefaults> (GTK_DEST_DEFAULT_MOTION
    | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP),
    &drop_types[0], n_drop_types, GDK_ACTION_COPY);
  
  drag_data_received.connect(slot(this, &AudioCDView::drag_data_received_cb));

  sampleDisplay_ = new SampleDisplay;
  sampleDisplay_->setTocEdit(child->tocEdit());

  sampleDisplay_->set_usize(200,200);
  
  vbox->pack_start(*sampleDisplay_, TRUE, TRUE);
  sampleDisplay_->show();

  Gtk::HScrollbar *scrollBar =
    new Gtk::HScrollbar(*(sampleDisplay_->getAdjustment()));
  vbox->pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();
  
  Gtk::Label *label;
  Gtk::HBox *selectionInfoBox = new Gtk::HBox;

//FIXME: Calculate entry width for the current font.
  gint entry_width = 90;

  markerPos_ = new Gtk::Entry;
  markerPos_->set_editable(true);
  markerPos_->set_usize(entry_width, 0);
  markerPos_->activate.connect(slot(this, &AudioCDView::markerSet));

  cursorPos_ = new Gtk::Entry;
  cursorPos_->set_usize(entry_width, 0);
  cursorPos_->set_editable(false);

  selectionStartPos_ = new Gtk::Entry;
  selectionStartPos_->set_editable(true);
  selectionStartPos_->set_usize(entry_width, 0);
  selectionStartPos_->activate.connect(slot(this, &AudioCDView::selectionSet));

  selectionEndPos_ = new Gtk::Entry;
  selectionEndPos_->set_editable(true);
  selectionEndPos_->set_usize(entry_width, 0);
  selectionEndPos_->activate.connect(slot(this, &AudioCDView::selectionSet));

  label = new Gtk::Label(string("Cursor: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*cursorPos_);
  label->show();
  cursorPos_->show();

  label = new Gtk::Label(string("Marker: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*markerPos_);
  label->show();
  markerPos_->show();

  label = new Gtk::Label(string("Selection: "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*selectionStartPos_);
  label->show();
  selectionStartPos_->show();

  label = new Gtk::Label(string(" - "));
  selectionInfoBox->pack_start(*label, FALSE, FALSE);
  selectionInfoBox->pack_start(*selectionEndPos_);
  label->show();
  selectionEndPos_->show();
  
//  vbox->pack_start(*selectionInfoBox, FALSE, FALSE);
  selectionInfoBox->set_border_width(2);
  sprintf(buf, "selectionBox-%i", viewNumber);
  project->add_docked(*selectionInfoBox, buf, GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL,
  		GNOME_DOCK_BOTTOM, 1, 1, 0);
  Gnome::DockItem *dockItem = project->get_dock_item_by_name(buf);
  dockItem->hide();
  widgetList->push_back(dockItem);
  selectionInfoBox->show();

  Gtk::HButtonBox *buttonBox = new Gtk::HButtonBox(GTK_BUTTONBOX_START, 5);
  zoomButton_ = new Gtk::RadioButton(string("Zoom"));
  selectButton_ = new Gtk::RadioButton(string("Select"));
  selectButton_->set_group(zoomButton_->group());

  playButton_ = new Gtk::Button();
  playLabel_ = new Gtk::Label("Play");
  playButton_->add (* playLabel_);
  playLabel_->show();
  
  buttonBox->pack_start(*zoomButton_);
  zoomButton_->show();
  buttonBox->pack_start(*selectButton_);
  selectButton_->show();
  zoomButton_->set_active(true);
  setMode(ZOOM);
  buttonBox->pack_start(*playButton_);
  playButton_->show();

  Gtk::Button *button = manage(new Gtk::Button("Zoom Out"));
  buttonBox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AudioCDView::zoomOut));

  button = manage(new Gtk::Button("Full View"));
  buttonBox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AudioCDView::fullView));
  
//  vbox->pack_start(*buttonBox, FALSE, FALSE);
  buttonBox->set_border_width(2);
  sprintf(buf, "zoomBox-%i", viewNumber);
  project->add_docked(*buttonBox, buf, GNOME_DOCK_ITEM_BEH_NEVER_VERTICAL,
  		GNOME_DOCK_TOP, 1, 1, 0);
  dockItem = project->get_dock_item_by_name(buf);
  dockItem->hide();
  widgetList->push_back(dockItem);
  buttonBox->show();

  sampleDisplay_->markerSet.connect(slot(this,
  			&AudioCDView::markerSetCallback));
  sampleDisplay_->selectionSet.connect(slot(this,
  			&AudioCDView::selectionSetCallback));
  sampleDisplay_->cursorMoved.connect(slot(this,
  			&AudioCDView::cursorMovedCallback));
  sampleDisplay_->trackMarkSelected.connect(slot(this,
  			&AudioCDView::trackMarkSelectedCallback));
  sampleDisplay_->trackMarkMoved.connect(slot(cdchild,
  			&AudioCDChild::trackMarkMovedCallback));
  sampleDisplay_->viewModified.connect(slot(this,
		        &AudioCDView::viewModifiedCallback));

  zoomButton_->toggled.connect(bind(slot(this, &AudioCDView::setMode), ZOOM));
  selectButton_->toggled.connect(bind(slot(this, &AudioCDView::setMode), SELECT));
  playButton_->clicked.connect(slot(this, &AudioCDView::play));

  tocEditView_->sampleViewFull();
}

void AudioCDView::update(unsigned long level)
{
  //cout << "updating AudioCDView - " << cdchild->get_name() << endl;

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
      markerPos_->set_text(string(cdchild->sample2string(marker)));
      sampleDisplay_->setMarker(marker);
    }
    else {
      markerPos_->set_text(string(""));
      sampleDisplay_->clearMarker();
    }
  }

  if (level & UPD_SAMPLE_SEL) {
    unsigned long start, end;

    if (tocEditView_->sampleSelection(&start, &end)) {
      selectionStartPos_->set_text(string(cdchild->sample2string(start)));
      selectionEndPos_->set_text(string(cdchild->sample2string(end)));
      sampleDisplay_->setRegion(start, end);
    }
    else {
      selectionStartPos_->set_text(string(""));
      selectionEndPos_->set_text(string(""));
      sampleDisplay_->setRegion(1, 0);
    }
  }
}

void AudioCDView::zoomIn()
{
  unsigned long start, end;

  if (tocEditView_->sampleSelection(&start, &end)) {
    tocEditView_->sampleView(start, end);
    guiUpdate();
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

  tocEditView_->sampleView(start, end);
  guiUpdate();
}

void AudioCDView::fullView()
{
  tocEditView_->sampleViewFull();
  guiUpdate();
}

void AudioCDView::play()
{
  unsigned long start, end;

  if (!tocEditView_->sampleSelection(&start, &end))
    tocEditView_->sampleView(&start, &end);

  cdchild->play(start, end);
}

int AudioCDView::getMarker(unsigned long *sample)
{
  if (tocEditView_->tocEdit()->lengthSample() == 0)
    return 0;

  if (sampleDisplay_->getMarker(sample) == 0) {
//FIXME    MDI_WINDOW->statusMessage("Please set marker.");
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
  if (mode_ == ZOOM) {
    tocEditView_->sampleView(start, end);
  }
  else {
    tocEditView_->sampleSelection(start, end);
  }
cout << "selectionSetCallback called" << endl;

  guiUpdate();
}

void AudioCDView::cursorMovedCallback(unsigned long pos)
{
  cursorPos_->set_text(string(cdchild->sample2string(pos)));
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
  unsigned long s1 = cdchild->string2sample(selectionStartPos_->get_text().c_str());
  unsigned long s2 = cdchild->string2sample(selectionEndPos_->get_text().c_str());

  tocEditView_->sampleSelection(s1, s2);
  guiUpdate();
}

void AudioCDChild::tocBlockedMsg(const char *op)
{
//FIXME  MessageBox msg(MDI_WINDOW->get_active_window(), op, 0,
//  MessageBox msg(MDI_WINDOW, op, 0,
//FIXME		 "Cannot perform requested operation because", 
//FIXME		 "project is in read-only state.", NULL);
//FIXME  msg.run();
}

void AudioCDView::drag_data_received_cb(GdkDragContext *context,
  gint x, gint y, GtkSelectionData *selection_data, guint info, guint time)
{
  GList *names;
  
  switch (info) {
    case TARGET_URI_LIST:
      names = (GList *)gnome_uri_list_extract_filenames \
			((char *)selection_data->data);  

//  	tocEdit_->blockEdit();
//FIXME      while (names->data) {
      if (names->data) {
          string str = g_strdup(static_cast <char *>(names->data));
          const char *file = stripCwd(str.c_str());

        switch (tocEditView_->tocEdit()->appendTrack(file)) {
        case 0:
	      guiUpdate();
//FIXME	      MDI_WINDOW->statusMessage("Appended track with audio data from \"%s\".", file);
	      break;
        case 1:
//FIXME	      MDI_WINDOW->statusMessage("Cannot open audio file \"%s\".", file);
	      break;
        case 2:
//FIXME	      MDI_WINDOW->statusMessage("Audio file \"%s\" has wrong format.", file);
	      break;
	    }
	    names = g_list_remove(names, names->data);
      }
//	tocEdit_->unblockEdit();
    break;
  }
}
