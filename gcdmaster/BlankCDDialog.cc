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

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "BlankCDDialog.h"
#include "CdDevice.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "guiUpdate.h"
#include "xcdrdao.h"
#include "log.h"

BlankCDDialog* BlankCDDialog::create(Glib::RefPtr<Gtk::Builder> builder,
				     Gtk::Window& parent)
{
    auto win = new BlankCDDialog();
    win->set_transient_for(parent);
    return win;
}

BlankCDDialog::BlankCDDialog()
{
    set_title(_("Blank CD Rewritable"));
    set_default_size(400, -1);

    vbox_.set_margin(10);
    set_child(vbox_);

    active_ = false;
    moreOptionsDialog_ = nullptr;
    speed_ = 1;

    // Device List
    Devices = Gtk::make_managed<DeviceList>(CdDevice::CD_RW);
    Devices->set_expand(true);
    vbox_.append(*Devices);

    // Blank Options Frame
    auto *blankOptionsFrame = Gtk::make_managed<Gtk::Frame>(_(" Blank Options "));
    auto *frameBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    frameBox->set_margin(5);
    blankOptionsFrame->set_child(*frameBox);

    // 1. Create the first button (the "master" of the group)
    fastBlank_rb = Gtk::make_managed<Gtk::CheckButton>(_("Fast Blank - does not erase contents"));
    
    // 2. Create the second button and join the group of the first
    fullBlank_rb = Gtk::make_managed<Gtk::CheckButton>(_("Full Blank - erases contents, slower"));
    fullBlank_rb->set_group(*fastBlank_rb);

    // 3. Set the default selection
    fastBlank_rb->set_active(true);
    frameBox->append(*fastBlank_rb);
    frameBox->append(*fullBlank_rb);

    // More Options Button
    auto *moreOptionsButton = Gtk::make_managed<Gtk::Button>();
    auto *moreOptionsBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    auto *moreOptionsPixmap = Gtk::make_managed<Gtk::Image>();
    moreOptionsPixmap->set_from_icon_name("org.gnome.Settings-symbolic");
    auto *moreOptionsLabel = Gtk::make_managed<Gtk::Label>(_("More Options"));
    
    moreOptionsBox->append(*moreOptionsPixmap);
    moreOptionsBox->append(*moreOptionsLabel);
    moreOptionsButton->set_child(*moreOptionsBox);
    moreOptionsButton->set_halign(Gtk::Align::END);
    moreOptionsButton->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &BlankCDDialog::moreOptions), frameBox, moreOptionsButton));
    
    frameBox->append(*moreOptionsButton);
    vbox_.append(*blankOptionsFrame);

    // Action Buttons (Start/Cancel)
    auto *hbox2 = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 20);
    hbox2->set_margin(10);
    hbox2->set_halign(Gtk::Align::CENTER);
    hbox2->set_homogeneous(true);

    auto *startBtn = Gtk::make_managed<Gtk::Button>();
    auto *startBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    auto *startPixmap = Gtk::make_managed<Gtk::Image>();
    startPixmap->set_from_resource("/org/gnome/gcdmaster/gcdmaster");
    startPixmap->set_pixel_size(48);
    startBox->append(*startPixmap);
    startBox->append(*Gtk::make_managed<Gtk::Label>(_("Start")));
    startBtn->set_child(*startBox);
    startBtn->signal_clicked().connect(sigc::mem_fun(*this, &BlankCDDialog::startAction));

    auto *cancelBtn = Gtk::make_managed<Gtk::Button>(_("Cancel"));
    cancelBtn->signal_clicked().connect(sigc::mem_fun(*this, &BlankCDDialog::stop));

    hbox2->append(*startBtn);
    hbox2->append(*cancelBtn);
    vbox_.append(*hbox2);

    // Create dialog boxes
    ejectWarningDialog = Ask3Box::create(*this,
					 _("Request"), 0,
					 { ("Ejecting a CD may block the SCSI bus and"),
					   _("cause buffer under runs when other devices"), 
					   _("are still recording."),
					   "", _("Keep the eject setting anyway?") });
    ejectWarningDialog->dialogDone.connect(sigc::mem_fun(*this,
							 &BlankCDDialog::checkEjectWarningAsync));

    reloadWarningDialog = Ask3Box::create(*this,
					  _("Request"), 0,
					  { _("Reloading a CD may block the SCSI bus and"),
					    _("cause buffer under runs when other devices"), 
					    _("are still recording."),
					    "", _("Keep the reload setting anyway?")});
    reloadWarningDialog->dialogDone.connect(sigc::mem_fun(*this,
							  &BlankCDDialog::checkReloadWarningAsync));

}

void BlankCDDialog::moreOptions(Gtk::Box* frame, Gtk::Button* button)
{
    button->set_visible(false);

    auto *innerVbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    innerVbox->set_margin(10);

    ejectButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Eject the CD after blanking"));
    innerVbox->append(*ejectButton_);

    reloadButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Reload the CD after writing, if necessary"));
    innerVbox->append(*reloadButton_);

    auto *speedHbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    speedHbox->append(*Gtk::make_managed<Gtk::Label>(_("Speed: ")));

    auto adjustment = Gtk::Adjustment::create(1, 1, 20);
    speedSpinButton_ = Gtk::make_managed<Gtk::SpinButton>(adjustment);
    speedSpinButton_->set_sensitive(false);
    adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &BlankCDDialog::speedChanged));
    speedHbox->append(*speedSpinButton_);

    speedButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Use max."));
    speedButton_->set_active(true);
    speedButton_->signal_toggled().connect(sigc::mem_fun(*this, &BlankCDDialog::speedButtonChanged));
    speedHbox->append(*speedButton_);

    innerVbox->append(*speedHbox);
    frame->append(*innerVbox);
}

void BlankCDDialog::start()
{
    set_visible(true);
    active_ = true;
    update(UPD_CD_DEVICES);
}

void BlankCDDialog::stop()
{
    set_visible(false);
    if (moreOptionsDialog_)
        moreOptionsDialog_->set_visible(false);
    active_ = false;
}

bool BlankCDDialog::on_close_request()
{
    log_message(0, "BlankCDWindow: close request");
    stop();
    return false;
}

void BlankCDDialog::startAction()
{
    if (Devices->selection().empty()) {
        auto *d = Gtk::make_managed<Gtk::MessageDialog>(*this, _("Please select at least one recorder device"),
							false, Gtk::MessageType::WARNING, Gtk::ButtonsType::OK, true);
        d->signal_response().connect([d](int) { d->hide(); });
        d->show();
        return;
    }

    // Start the warning check chain
    ejectWarningDialog->choose();
}

void BlankCDDialog::checkEjectWarningAsync(int result)
{
    switch (result) {
    case 0: // Keep eject setting
	CONFIG_MANAGER->setEjectWarning(false);
	reloadWarningDialog->choose(); // Move to next check
	break;
    case 1: // Don't keep eject setting
	ejectButton_->set_active(false);
	reloadWarningDialog->choose(); // Move to next check
	break;
    default:
	// If response is cancel/close, we do nothing (stopping the chain)
	break;
    }
}

void BlankCDDialog::checkReloadWarningAsync(int result)
{
    switch (result) {
    case 0: // keep reload setting
	CONFIG_MANAGER->setReloadWarning(false);
	proceedWithBlanking(); // Final step
	break;
    case 1: // don't keep reload setting
	reloadButton_->set_active(false);
	proceedWithBlanking(); // Final step
	break;
    default:
	break;
    }
}

bool BlankCDDialog::getEject()
{
    if (moreOptionsDialog_)
	return ejectButton_->get_active() ? 1 : 0;
    else
	return 0; 
}

void BlankCDDialog::proceedWithBlanking()
{
    int fast = fastBlank_rb->get_active() ? 1 : 0;
    int burnSpeed = getSpeed();
    int eject = getEject();
    int reload = getReload();

    std::string targetData = Devices->selection();
    CdDevice *writeDevice = CdDevice::find(targetData);

    if (writeDevice) {
        if (writeDevice->blank(this, fast, burnSpeed, eject, reload) != 0) {
            auto *d = Gtk::make_managed<Gtk::MessageDialog>(*this, _("Cannot start blanking"), 
							    false, Gtk::MessageType::ERROR, Gtk::ButtonsType::OK, true);
            d->signal_response().connect([d](int) { d->hide(); });
            d->show();
        } else {
            guiUpdate(UPD_CD_DEVICE_STATUS);
        }
    }

    stop();
}

int BlankCDDialog::getSpeed()
{
    if (moreOptionsDialog_) {
        return speedButton_->get_active() ? 0 : speed_;
    }
    return 0;
}

bool BlankCDDialog::getReload()
{
    if (moreOptionsDialog_)
	return reloadButton_->get_active() ? 1 : 0;
    else
	return 0;
}

void BlankCDDialog::speedChanged()
{
    int new_speed;

    new_speed = speedSpinButton_->get_value_as_int();

    if ((new_speed % 2) == 1)
    {
	if (new_speed > 2)
	{
	    if (new_speed > speed_)
	    {
		new_speed = new_speed + 1;
	    }
	    else
	    {
		new_speed = new_speed - 1;
	    }
	}
	speedSpinButton_->set_value(new_speed);
    }
    speed_ = new_speed;
}

void BlankCDDialog::speedButtonChanged()
{
    if (speedButton_->get_active())
    {
	speedSpinButton_->set_sensitive(false);
    }
    else
    {
	speedSpinButton_->set_sensitive(true);
    }
}

void BlankCDDialog::update(unsigned long level)
{
    if (!active_)
	return;

    log_message(0, "BlankCDWindow update (%d)\n", level);

    set_title("Blank CD Rewritable");

    if (level & UPD_CD_DEVICES) {
	Devices->import();
    }
    
    if (level & UPD_CD_DEVICE_STATUS) {
	Devices->importStatus();
	Devices->selectOne();
    }
}

