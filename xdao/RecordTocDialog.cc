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

#include <gnome--.h>

#include "RecordTocDialog.h"
#include "RecordTocSource.h"
#include "RecordCDSource.h"
#include "RecordCDTarget.h"
#include "RecordHDTarget.h"
#include "guiUpdate.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "TocEdit.h"

RecordTocDialog::RecordTocDialog(TocEdit *tocEdit)
{
  tocEdit_ = tocEdit;

  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(10);
  vbox->set_spacing(10);
  vbox->show();
  Gtk::HBox *hbox = new Gtk::HBox;
  hbox->set_spacing(10);
  hbox->show();
  vbox->pack_start(*hbox, false, false);
  add(*vbox);

  active_ = 0;

  TocSource = new RecordTocSource(tocEdit_);
  TocSource->start();
  CDTarget = new RecordCDTarget(this);
  CDTarget->start();

  hbox->pack_start(*TocSource);
  TocSource->show();
  hbox->pack_start(*CDTarget);
  CDTarget->show();

  hbox = new Gtk::HBox;
  hbox->set_spacing(10);
  hbox->show();

  Gtk::VBox *frameBox = new Gtk::VBox;
  frameBox->show();
  simulate_rb = new Gtk::RadioButton("Simulate", 0);
  simulateBurn_rb = new Gtk::RadioButton("Simulate and Burn", 0);
  burn_rb = new Gtk::RadioButton("Burn", 0);

  frameBox->pack_start(*simulate_rb);
  simulate_rb->show();
  frameBox->pack_start(*simulateBurn_rb);
//  simulateBurn_rb->show();
  simulateBurn_rb->set_group(simulate_rb->group());
  frameBox->pack_start(*burn_rb);
  burn_rb->show();
  burn_rb->set_group(simulate_rb->group());

  hbox->pack_start(*frameBox, true, false);

  Gnome::Pixmap *pixmap =
  	manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/gcdmaster.png")));
  Gtk::Label *startLabel = manage(new Gtk::Label("      Start      "));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->clicked.connect(slot(this, &RecordTocDialog::startAction));
  pixmap->show();
  startLabel->show();
  startBox->show();
  button->show();

  hbox->pack_start(*button, true, false);

  Gtk::HBox *hbox2 = new Gtk::HBox;
  hbox2->show();
  hbox2->pack_start(*hbox, true, false);
  vbox->pack_start(*hbox2, true, false);
}

RecordTocDialog::~RecordTocDialog()
{
  delete TocSource;
  delete CDTarget;
}

void RecordTocDialog::start(Gtk::Window *parent)
{
  if (active_) {
    get_window().raise();
  }

  active_ = 1;

  TocSource->start();
  CDTarget->start();

  update(UPD_ALL);

  set_transient_for(*parent);

  show();
}

void RecordTocDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }

  TocSource->stop();
  CDTarget->stop();
}

void RecordTocDialog::update(unsigned long level)
{
  if (!active_)
    return;

  std::string title;
  title += "Record project ";
  title += tocEdit_->filename();
  title += " to CD";
  set_title(title);

  TocSource->update(level);
  CDTarget->update(level);

  if (level & UPD_CD_DEVICE_STATUS)
    CDTarget->getDeviceList()->selectOne();
}

gint RecordTocDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void RecordTocDialog::startAction()
{
  if (tocEdit_ == NULL)
    return;

  DeviceList *targetList = CDTarget->getDeviceList();
 
  if (targetList->selection().empty()) {
    Gnome::Dialogs::ok(*this, "Please select at least one recorder device");
    return;
  }

  Toc *toc = tocEdit_->toc();

  if (toc->nofTracks() == 0 || toc->length().lba() < 300) {
    Gnome::Dialogs::error(*this, "Current toc contains no tracks or length of at least one track is < 4 seconds");
    return;
  }

  switch (toc->checkCdTextData()) {
    case 0: // OK
      break;
    case 1: // warning
      {
        Ask2Box msg(this, "CD-TEXT Inconsistency", 0, 2,
	  	  "Inconsistencies were detected in the defined CD-TEXT data",
		    "which may produce undefined results. See the console",
		    "output for more details.",
		    "",
		    "Do you want to proceed anyway?", NULL);

        if (msg.run() != 1)
  	return;
      }
      break;
    default: // error
      {
        MessageBox msg(this, "CD-TEXT Error", 0, 
	  	     "The defined CD-TEXT data is erroneous or incomplete.",
		       "See the console output for more details.", NULL);
        msg.run();
        return;
      }
      break;
  }

  int simulate;
  if (simulate_rb->get_active())
    simulate = 1;
  else if (simulateBurn_rb->get_active())
    simulate = 2;
  else
    simulate = 0;

  int multiSession = CDTarget->getMultisession();
  int burnSpeed = CDTarget->getSpeed();

  int eject = CDTarget->checkEjectWarning(this);
  if (eject == -1)
    return;
  int reload = CDTarget->checkReloadWarning(this);
  if (reload == -1)
    return;

  int buffer = CDTarget->getBuffer();

  Gtk::CList_Helpers::SelectionList targetSelection = targetList->selection();

  for (int i = 0; i < targetSelection.size(); i++) {
    DeviceList::DeviceData *targetData =
        (DeviceList::DeviceData*)targetSelection[i].get_data();

    if (targetData == NULL)
      break;
  
    CdDevice *writeDevice = CdDevice::find(targetData->bus, targetData->id, targetData->lun);
  
    if (writeDevice == NULL)
      break;

    if (writeDevice->recordDao(tocEdit_, simulate, multiSession,
			     burnSpeed, eject, reload, buffer) != 0)
      Gnome::Dialogs::error(*this, "Cannot start disk-at-once recording");
    else
      guiUpdate(UPD_CD_DEVICE_STATUS);
  }
  stop();
}

