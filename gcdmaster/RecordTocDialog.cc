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

#include "DeviceList.h"
#include "MessageBox.h"
#include "RecordCDSource.h"
#include "RecordCDTarget.h"
#include "RecordTocDialog.h"
#include "RecordTocSource.h"
#include "TocEdit.h"
#include "config.h"
#include "guiUpdate.h"
#include "log.h"

RecordTocDialog::RecordTocDialog(Glib::RefPtr<TocEdit> tocEdit)
    : tocEdit_(tocEdit)
{
    set_title(_("Record CD"));

    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    vbox->set_margin(10);
    set_child(*vbox);

    auto hbox_top = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    vbox->append(*hbox_top);

    active_ = false;

    TocSource = Gtk::make_managed<RecordTocSource>(tocEdit_);
    CDTarget = Gtk::make_managed<RecordCDTarget>(this);
    
    hbox_top->append(*TocSource);
    hbox_top->append(*CDTarget);
    CDTarget->set_expand(true);

    auto hbox_bottom = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    auto frameBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    
    // Initialize CheckButtons
    simulate_cb = Gtk::make_managed<Gtk::CheckButton>(_("Simulate"));
    simulateBurn_cb = Gtk::make_managed<Gtk::CheckButton>(_("Simulate and Burn"));
    burn_cb = Gtk::make_managed<Gtk::CheckButton>(_("Burn"));

    // Set the group: assigning them to the same group makes them mutually exclusive
    simulateBurn_cb->set_group(*simulate_cb);
    burn_cb->set_group(*simulate_cb);

    // Default selection
    burn_cb->set_active(true);

    frameBox->append(*simulate_cb);
    frameBox->append(*simulateBurn_cb);
    frameBox->append(*burn_cb);
    frameBox->set_expand(true);

    hbox_bottom->append(*frameBox);

    auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    bbox->set_homogeneous(true);
    bbox->set_spacing(10);
    bbox->set_hexpand(true);
    bbox->set_halign(Gtk::Align::CENTER);
    hbox_bottom->append(*bbox);

    // Start Button
    auto startButton = Gtk::make_managed<Gtk::Button>();
    auto startBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    auto pixmap = Gtk::make_managed<Gtk::Image>();
    pixmap->set_from_resource("/org/gnome/gcdmaster/gcdmaster");
    pixmap->set_pixel_size(48);
    auto startLabel = Gtk::make_managed<Gtk::Label>(_("Start"));
    
    startBox->append(*pixmap);
    startBox->append(*startLabel);
    startButton->set_child(*startBox);
    startButton->signal_clicked().connect(sigc::mem_fun(*this, &RecordTocDialog::startAction));
    bbox->append(*startButton);

    // Cancel Button
    auto cancel_but = Gtk::make_managed<Gtk::Button>(_("_Cancel"), true);
    cancel_but->signal_clicked().connect(sigc::mem_fun(*this, &Gtk::Widget::hide));
    bbox->append(*cancel_but);

    vbox->append(*hbox_bottom);
}

void RecordTocDialog::start(Gtk::Window *parent)
{
    if (!active_) {
        active_ = true;
        update(UPD_ALL);
        set_transient_for(*parent);
    }
    present();
}

void RecordTocDialog::stop()
{
    hide();
    active_ = false;
}

void RecordTocDialog::update(unsigned long level)
{
    log_message(1, "[update %x] RecordTocDialog", level);
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

void RecordTocDialog::startAction()
{
    if (tocEdit_ == NULL)
        return;

    DeviceList *targetList = CDTarget->getDeviceList();

    if (targetList->selection().empty()) {
        MessageBox::message(*this,_("Please select at least one recorder device"));
        return;
    }

    Toc *toc = tocEdit_->toc();

    if (toc->nofTracks() == 0 || toc->length().lba() < 300) {
        MessageBox::message(*this,
                           _("Current toc contains no tracks or length "
                             "of at least one track is < 4 seconds"));
        return;
    }

    auto check_result = toc->checkCdTextData();

    if (check_result == 0) {
        startActionConfirmed(0);
        return;
    }

    if (check_result == 1) {
        // Warning
        auto ask = Ask2Box::create(*this, 
                                   _("CD-TEXT Inconsistency"),
                                   0,
                                   { _("Inconsistencies were detected in the defined "
                                       "CD-TEXT data"),
                                     _("which may produce undefined results. See the console"),
                                     _("output for more details."), "",
                                     _("Do you want to proceed anyway?") });
        ask->dialogDone.connect(sigc::mem_fun(*this,
                                              &RecordTocDialog::startActionConfirmed));
        return;
    }
    
    // Error case
    MessageBox::message(*this, _("CD-TEXT Error"),
                        { _("The defined CD-TEXT data is erroneous or "
                            "incomplete."),
                          _("See the console output for more details.") });
}

void RecordTocDialog::startActionConfirmed(int clicked_button)
{
    int simulate = 0;
    if (simulate_cb->get_active())
        simulate = 1;
    else if (simulateBurn_cb->get_active())
        simulate = 2;

    int multiSession = CDTarget->getMultisession();
    int burnSpeed = CDTarget->getSpeed();
    int overburn = CDTarget->getOverburn();
    int buffer = CDTarget->getBuffer();
    int eject = CDTarget->getEject();
    int reload = CDTarget->getReload();

    DeviceList *target = CDTarget->getDeviceList();
    if (target->selection().empty()) {
        MessageBox::message(*this, _("Please select a writer device"));
        return;
    }

    std::string targetData = target->selection();
    CdDevice *writeDevice = CdDevice::find(targetData.c_str());

    if (writeDevice) {
        if (!writeDevice->recordDao(*this, tocEdit_.get(),
                                    simulate, multiSession, burnSpeed, eject,
                                    reload, buffer, overburn)) {
            ErrorBox::message(*this, _("Cannot start disk-at-once recording"));
        } else {
            guiUpdate(UPD_CD_DEVICE_STATUS);
        }
    }
    stop();
}
