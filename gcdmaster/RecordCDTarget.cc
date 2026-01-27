/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.ping.de>
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

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <gtkmm.h>
#include <glibmm/i18n.h>


#include "MessageBox.h"
#include "RecordCDTarget.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "ConfigManager.h"
#include "DeviceList.h"
#include "TocEdit.h"
#include "guiUpdate.h"

#include "util.h"

RecordCDTarget::RecordCDTarget(Gtk::Window *parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    parent_ = parent;
    moreOptions_ = nullptr;
    set_spacing(10);

    devices_ = Gtk::make_managed<DeviceList>(CdDevice::CD_R);
    append(*devices_);

    // device settings
    auto recordOptionsFrame = Gtk::make_managed<Gtk::Frame>(" Record Options ");

    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    vbox->set_margin(5);
    vbox->set_spacing(5);
    recordOptionsFrame->set_child(*vbox);

auto hboxSpeed = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    auto labelSpeed = Gtk::make_managed<Gtk::Label>("Speed: ");
    hboxSpeed->append(*labelSpeed);

    auto adjustmentSpeed = Gtk::Adjustment::create(1, 1, 48);
    speedSpinButton_ = Gtk::make_managed<Gtk::SpinButton>(adjustmentSpeed);
    speedSpinButton_->set_digits(0);
    speedSpinButton_->set_sensitive(false);
    adjustmentSpeed->signal_value_changed().connect(sigc::mem_fun(*this, &RecordCDTarget::speedChanged));
    
    speedSpinButton_->set_margin_start(10);
    hboxSpeed->append(*speedSpinButton_);

    speedButton_ = Gtk::make_managed<Gtk::CheckButton>("Use max.");
    speedButton_->set_active(true);
    speedButton_->signal_toggled().connect(sigc::mem_fun(*this, &RecordCDTarget::speedButtonChanged));
    hboxSpeed->append(*speedButton_);
    vbox->append(*hboxSpeed);

    // Copies row
    auto hboxCopies = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    auto labelCopies = Gtk::make_managed<Gtk::Label>("Copies: ");
    hboxCopies->append(*labelCopies);

    auto adjustmentCopies = Gtk::Adjustment::create(1, 1, 999);
    copiesSpinButton_ = Gtk::make_managed<Gtk::SpinButton>(adjustmentCopies);
    copiesSpinButton_->set_digits(0);
    copiesSpinButton_->set_margin_start(10);
    hboxCopies->append(*copiesSpinButton_);
    vbox->append(*hboxCopies);

    // More Options Button
    auto moreOptionsButton = Gtk::make_managed<Gtk::Button>();
    auto moreOptionsBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    
    auto moreOptionsIcon = Gtk::make_managed<Gtk::Image>("emblem-system-symbolic");
    auto moreOptionsLabel = Gtk::make_managed<Gtk::Label>("More Options");
    
    moreOptionsBox->set_spacing(4);
    moreOptionsBox->append(*moreOptionsIcon);
    moreOptionsBox->append(*moreOptionsLabel);
    moreOptionsButton->set_child(*moreOptionsBox);
    moreOptionsButton->signal_clicked().connect(sigc::mem_fun(*this, &RecordCDTarget::moreOptions));

    auto buttonContainer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    buttonContainer->set_halign(Gtk::Align::END);
    buttonContainer->append(*moreOptionsButton);
    vbox->append(*buttonContainer);

    append(*recordOptionsFrame);
}

void RecordCDTarget::moreOptions()
{
    if (!moreOptions_) {
        moreOptions_ = Gtk::make_managed<Gtk::Window>();
        moreOptions_->set_modal(true);
        moreOptions_->set_transient_for(*parent_);

        auto *top_vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        top_vbox->set_margin(10);
        moreOptions_->set_child(*top_vbox);

        auto *frame = Gtk::make_managed<Gtk::Frame>(" More Target Options ");
        frame->set_margin(5);
        top_vbox->append(*frame);
        
        auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        vbox->set_margin(10);
        vbox->set_spacing(5);
        frame->set_child(*vbox);

        overburnButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Allow over-burning"));
        vbox->append(*overburnButton_);
        ejectButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Eject the CD after writing"));
        vbox->append(*ejectButton_);
        reloadButton_ =
            Gtk::make_managed<Gtk::CheckButton>(_("Reload the CD after writing, if necessary"));
        vbox->append(*reloadButton_);
        closeSessionButton_ =
            Gtk::make_managed<Gtk::CheckButton>(_("Close disk - no further writing possible!"));
        closeSessionButton_->set_active(true);
        vbox->append(*closeSessionButton_);

        auto hboxBuffer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        auto labelBuf = Gtk::make_managed<Gtk::Label>(_("Buffer: "));
        hboxBuffer->append(*labelBuf);

        auto adjBuffer = Gtk::Adjustment::create(10, 10, 1000);
        bufferSpinButton_ = Gtk::make_managed<Gtk::SpinButton>(adjBuffer);
        hboxBuffer->append(*bufferSpinButton_);
        bufferSpinButton_->set_margin_start(10);

        auto labelSec = Gtk::make_managed<Gtk::Label>(_(" audio seconds "));
        hboxBuffer->append(*labelSec);
        
        bufferRAMLabel_ = Gtk::make_managed<Gtk::Label>(_("= 1.72 Mb buffer."));
        hboxBuffer->append(*bufferRAMLabel_);
        adjBuffer->signal_value_changed().connect(
            sigc::mem_fun(*this, &RecordCDTarget::updateBufferRAMLabel));

        vbox->append(*hboxBuffer);

        auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
        bbox->set_margin(10);
        bbox->set_halign(Gtk::Align::CENTER);
        auto closeButton = Gtk::make_managed<Gtk::Button>("close");
        bbox->append(*closeButton);
        closeButton->signal_clicked().connect([this]() {
            this->moreOptions_->hide();
        });
        vbox->append(*bbox);
            
    }

    moreOptions_->present();
}

void RecordCDTarget::update(unsigned long level)
{
    if (level & UPD_CD_DEVICES)
        devices_->import();
    else if (level & UPD_CD_DEVICE_STATUS)
        devices_->importStatus();
}

int RecordCDTarget::getMultisession()
{
    if (moreOptions_)
        return closeSessionButton_->get_active() ? 0 : 1;
    else
        return 0;
}

int RecordCDTarget::getCopies()
{
    return copiesSpinButton_->get_value_as_int();
}

int RecordCDTarget::getSpeed()
{
    if (speedButton_->get_active())
        return 0;
    else
        return speed_;
}

int RecordCDTarget::getBuffer()
{
    if (moreOptions_)
        return bufferSpinButton_->get_value_as_int();
    else
        return 10;
}

bool RecordCDTarget::getEject()
{
    if (moreOptions_)
        return ejectButton_->get_active() ? 1 : 0;
    else
        return 0;
}

bool RecordCDTarget::getOverburn()
{
    if (moreOptions_)
        return overburnButton_->get_active() ? 1 : 0;
    else
        return 0;
}

bool RecordCDTarget::getReload()
{
    if (moreOptions_)
        return reloadButton_->get_active() ? 1 : 0;
    else
        return 0;
}

void RecordCDTarget::updateBufferRAMLabel()
{
    char label[32];

    snprintf(label, sizeof(label), "= %0.2f Mb buffer.",
             bufferSpinButton_->get_value() * 0.171875);
    bufferRAMLabel_->set_text(label);
}

void RecordCDTarget::speedButtonChanged()
{
    speedSpinButton_->set_sensitive(!speedButton_->get_active());
}

void RecordCDTarget::speedChanged()
{
    int new_speed = speedSpinButton_->get_value_as_int();

    if ((new_speed % 2) == 1 && new_speed > 2) {
        new_speed = (new_speed > speed_) ? new_speed + 1 : new_speed - 1;
        speedSpinButton_->set_value(new_speed);
    }
    speed_ = new_speed;
}
