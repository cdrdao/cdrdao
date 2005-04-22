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

#include <gtkmm.h>
#include <gnome.h>

#include "RecordTocDialog.h"
#include "RecordTocSource.h"
#include "RecordCDSource.h"
#include "RecordCDTarget.h"
#include "RecordHDTarget.h"
#include "guiUpdate.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "TocEdit.h"
#include "Icons.h"

RecordTocDialog::RecordTocDialog(TocEdit *tocEdit)
{
  tocEdit_ = tocEdit;

  set_title(_("Record CD"));

  Gtk::VBox *vbox = manage(new Gtk::VBox);
  vbox->set_border_width(10);
  vbox->set_spacing(10);
  Gtk::HBox *hbox = manage(new Gtk::HBox);
  hbox->set_spacing(10);
  vbox->pack_start(*hbox);
  add(*vbox);

  active_ = 0;

  TocSource = new RecordTocSource(tocEdit_);
  TocSource->start();
  CDTarget = new RecordCDTarget(this);
  CDTarget->start();

  hbox->pack_start(*TocSource, Gtk::PACK_SHRINK);
  hbox->pack_start(*CDTarget);

  hbox = new Gtk::HBox;
  hbox->set_spacing(10);

  Gtk::VBox *frameBox = new Gtk::VBox;
  simulate_rb = new Gtk::RadioButton(_("Simulate"), 0);
  simulateBurn_rb = new Gtk::RadioButton(_("Simulate and Burn"), 0);
  burn_rb = new Gtk::RadioButton(_("Burn"), 0);

  frameBox->pack_start(*simulate_rb);
  frameBox->pack_start(*simulateBurn_rb);
  Gtk::RadioButton::Group rbgroup = simulate_rb->get_group();
  simulateBurn_rb->set_group(rbgroup);
  frameBox->pack_start(*burn_rb);
  burn_rb->set_group(rbgroup);

  hbox->pack_start(*frameBox, true, false);

  Gtk::Image *pixmap = manage(new Gtk::Image(Icons::GCDMASTER,
                                             Gtk::ICON_SIZE_DIALOG));
  Gtk::Label *startLabel = manage(new Gtk::Label(_("Start")));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->signal_clicked().connect(mem_fun(*this, &RecordTocDialog::startAction));
  hbox->pack_start(*button);

  Gtk::Button* cancel_but =
    manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::CANCEL)));
  cancel_but->signal_clicked().connect(mem_fun(*this, &Gtk::Widget::hide));
  hbox->pack_start(*cancel_but);

  vbox->pack_start(*hbox, Gtk::PACK_SHRINK);
  show_all_children();
}

RecordTocDialog::~RecordTocDialog()
{
  delete TocSource;
  delete CDTarget;
}

void RecordTocDialog::start(Gtk::Window *parent)
{
  if (!active_) {
    active_ = true;
    TocSource->start();
    CDTarget->start();
    update(UPD_ALL);
    set_transient_for(*parent);
  }
  present();
}

void RecordTocDialog::stop()
{
  hide();
  active_ = false;

  TocSource->stop();
  CDTarget->stop();
}

void RecordTocDialog::update(unsigned long level)
{
  if (!active_)
    return;

  std::string title;
  title += _("Record project ");
  title += tocEdit_->filename();
  title += _(" to CD");
  set_title(title);

  TocSource->update(level);
  CDTarget->update(level);

  if (level & UPD_CD_DEVICE_STATUS)
    CDTarget->getDeviceList()->selectOne();
}

bool RecordTocDialog::on_delete_event(GdkEventAny*)
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
    Gtk::MessageDialog md(*this,
                          _("Please select at least one recorder device"),
                          Gtk::MESSAGE_INFO);
    md.run();
    return;
  }

  Toc *toc = tocEdit_->toc();

  if (toc->nofTracks() == 0 || toc->length().lba() < 300) {
    Gtk::MessageDialog md(*this, _("Current toc contains no tracks or length "
                                   "of at least one track is < 4 seconds"),
                          Gtk::MESSAGE_ERROR);
    md.run();
    return;
  }

  switch (toc->checkCdTextData()) {
    case 0: // OK
      break;
    case 1: // warning
      {
        Ask2Box msg(this, _("CD-TEXT Inconsistency"), 0, 2,
                    _("Inconsistencies were detected in the defined CD-TEXT "
                      "data"),
		    _("which may produce undefined results. See the console"),
		    _("output for more details."),
		    "",
		    _("Do you want to proceed anyway?"), NULL);

        if (msg.run() != 1)
  	return;
      }
      break;
    default: // error
      {
        MessageBox msg(this, _("CD-TEXT Error"), 0, 
                       _("The defined CD-TEXT data is erroneous or "
                         "incomplete."),
		       _("See the console output for more details."), NULL);
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
  int overburn = CDTarget->getOverburn();

  int eject = CDTarget->checkEjectWarning(this);
  if (eject == -1)
    return;
  int reload = CDTarget->checkReloadWarning(this);
  if (reload == -1)
    return;

  int buffer = CDTarget->getBuffer();

  DeviceList* target = CDTarget->getDeviceList();
  if (target->selection().empty()) {
    Gtk::MessageDialog d(*this, _("Please select a writer device"),
                         Gtk::MESSAGE_INFO);
    d.run();
    return;
  }

  std::string targetData = target->selection();
  CdDevice *writeDevice = CdDevice::find(targetData.c_str());
  
  if (writeDevice) {
    if (!writeDevice->recordDao(*this, tocEdit_, simulate, multiSession,
                                burnSpeed, eject, reload, buffer, overburn)) {
      Gtk::MessageDialog d(*this, _("Cannot start disk-at-once recording"),
                           Gtk::MESSAGE_ERROR);
      d.run();
    } else {
      guiUpdate(UPD_CD_DEVICE_STATUS);
    }
  }
  stop();
}

