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

#include "config.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "RecordCDSource.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "guiUpdate.h"

#include "util.h"

#include "DeviceList.h"

#define MAX_CORRECTION_ID 3

static RecordCDSource::CorrectionTable CORRECTION_TABLE[MAX_CORRECTION_ID + 1] = {
    {3, N_("Jitter + scratch (slow)")},
    {2, N_("Jitter + checks")},
    {1, N_("Jitter correction")},
    {0, N_("No checking (fast)")}};

#define MAX_SUBCHAN_READ_MODE_ID 2

static RecordCDSource::SubChanReadModeTable SUBCHAN_READ_MODE_TABLE[MAX_SUBCHAN_READ_MODE_ID + 1] =
    {{0, N_("none")}, {1, N_("R-W packed")}, {2, N_("R-W raw")}};

RecordCDSource::RecordCDSource(Gtk::Window *parent)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    parent_ = parent;
    set_spacing(10);

    devices_ = new DeviceList(CdDevice::CD_ROM);
    append(*devices_);

    // device settings
    audo extractOptionsFrame = Gtk::make_managed<Gtk::Frame>(_(" Read Options "));
    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    vbox->set_margin(5);
    vbox->set_spacing(5);
    extractOptionsFrame->set_child(*vbox);

    onTheFlyButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Copy to disk before burning"));
    onTheFlyButton_->set_active(true);
    vbox->append(*onTheFlyButton_);

    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    auto *label = Gtk::make_managed<Gtk::Label>(_("Speed: "));
    hbox->append(*label);

    Glib::RefPtr<Gtk::Adjustment> adjustment = Gtk::Adjustment::create(1, 1, 50);
    speedSpinButton_ = new Gtk::SpinButton(adjustment);
    speedSpinButton_->set_digits(0);
    speedSpinButton_->set_sensitive(false);
    adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &RecordCDSource::speedChanged));
    speedSpinButton_->set_margin_start(10);
    hbox->append(*speedSpinButton_);

    speedButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Use max."));
    speedButton_->set_active(true);
    speedButton_->signal_toggled().connect(
        sigc::mem_fun(*this, &RecordCDSource::speedButtonChanged));
    hbox->append(*speedButton_);
    vbox->append(*hbox);

    // More Options Button
    auto moreOptionsButton = Gtk::make_managed<Gtk::Button>();
    auto moreOptionsBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    
    auto moreOptionsIcon = Gtk::make_managed<Gtk::Image>("emblem-system-symbolic");
    auto moreOptionsLabel = Gtk::make_managed<Gtk::Label>("More Options");
    
    moreOptionsBox->set_spacing(4);
    moreOptionsBox->append(*moreOptionsIcon);
    moreOptionsBox->append(*moreOptionsLabel);
    moreOptionsButton->set_child(*moreOptionsBox);
    moreOptionsButton->signal_clicked().connect(sigc::mem_fun(*this, &RecordCDSource::moreOptions));

    auto buttonContainer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    buttonContainer->set_halign(Gtk::Align::END);
    buttonContainer->append(*moreOptionsButton);
    vbox->append(*buttonContainer);

    append(*extractOptionsFrame);
    update(UPD_CD_DEVICES);
}

void RecordCDSource::update(unsigned long level)
{
    if (!active_)
        return;

    if (level & UPD_CD_DEVICES)
        devices_->import();
    else if (level & UPD_CD_DEVICE_STATUS)
        devices_->importStatus();
}

void RecordCDSource::moreOptions()
{
    if (!moreOptions_) {
        moreOptions_ = Gtk::make_managed<Gtk::Window>();
        moreOptions_->set_modal(true);
        moreOptions_->set_transient_for(*parent_);
        set_title(_("Source options"));

        auto *top_vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        top_vbox->set_margin(10);
        moreOptions_->set_child(*top_vbox);

        auto grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_row_spacing(2);
        grid->set_column_spacing(10);
        grid->set_margin(5);

        frame = Gtk::make_managed<Gtk::Frame>(_(" More Source Options "));
        frame->set_child(*grid);
        top_vbox->append(*frame);
        
        continueOnErrorButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Continue if errors found"));
        continueOnErrorButton_->set_active(false);
        grid->attach(continueOnErrorButton_, 0, 0);
        ignoreIncorrectTOCButton_ = Gtk::make_managed<Gtk::CheckButton>(_("Ignore incorrect TOC"));
        ignoreIncorrectTOCButton_->set_active(false);
        grid->attach(*ignoreIncorrectTOCButton_, 0, 1);

        for (int i = 0; i <= MAX_CORRECTION_ID; i++) {
            correctionMenu_.append(CORRECTION_TABLE[i].name);
        }
        correctionMenu_.signal_changed().connect(
            sigc::mem_fun(*this, &RecordCDSource::setCorrection));
        correctionMenu_.set_active(correction_);

        Gtk::Alignment *align;

        label = new Gtk::Label(_("Audio Correction Method:"));
        align = new Gtk::Alignment(0, 0.5, 0, 1);
        align->add(*label);
        table->attach(*align, 0, 1, 2, 3, Gtk::FILL);
        table->attach(correctionMenu_, 1, 2, 2, 3);

        for (int i = 0; i <= MAX_SUBCHAN_READ_MODE_ID; i++) {
            subChanReadModeMenu_.append(SUBCHAN_READ_MODE_TABLE[i].name);
        }
        subChanReadModeMenu_.signal_changed().connect(
            sigc::mem_fun(*this, &RecordCDSource::setSubChanReadMode));
        subChanReadModeMenu_.set_active(subChanReadMode_);

        label = new Gtk::Label(_("Sub-Channel Reading Mode:"));
        align = new Gtk::Alignment(0, 0.5, 0, 1);
        align->add(*label);
        table->attach(*align, 0, 1, 3, 4, Gtk::FILL);
        table->attach(subChanReadModeMenu_, 1, 2, 3, 4);
    }

    moreOptionsDialog_->show_all();
    moreOptionsDialog_->run();
    moreOptionsDialog_->hide();
}

void RecordCDSource::setCorrection()
{
    int s = correctionMenu_.get_active_row_number();
    if (s >= 0 && s <= MAX_CORRECTION_ID)
        correction_ = s;
}

bool RecordCDSource::getOnTheFly()
{
    return onTheFlyButton_->get_active() ? 0 : 1;
}

void RecordCDSource::setOnTheFly(bool active)
{
    onTheFlyButton_->set_active(!active);
}

int RecordCDSource::getCorrection()
{
    return CORRECTION_TABLE[correction_].correction;
}

void RecordCDSource::setSubChanReadMode()
{
    int m = subChanReadModeMenu_.get_active_row_number();
    if (m >= 0 && m <= MAX_SUBCHAN_READ_MODE_ID)
        subChanReadMode_ = m;
}

int RecordCDSource::getSubChanReadMode()
{
    return SUBCHAN_READ_MODE_TABLE[subChanReadMode_].mode;
}

void RecordCDSource::speedButtonChanged()
{
    if (speedButton_->get_active()) {
        speedSpinButton_->set_sensitive(false);
    } else {
        speedSpinButton_->set_sensitive(true);
    }
}

void RecordCDSource::speedChanged()
{
    // Do some validating here. speed_ <= MAX read speed of CD.
    speed_ = speedSpinButton_->get_value_as_int();
}

void RecordCDSource::onTheFlyOption(bool visible)
{
    if (visible)
        onTheFlyButton_->show();
    else
        onTheFlyButton_->hide();
}
