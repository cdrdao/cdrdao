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

#include "DuplicateCDProject.h"
#include "ConfigManager.h"
#include "DeviceList.h"
#include "Icons.h"
#include "MessageBox.h"
#include "Project.h"
#include "RecordCDSource.h"
#include "RecordCDTarget.h"
#include "config.h"
#include "guiUpdate.h"
#include "xcdrdao.h"

#include <glibmm/i18n.h>
#include <gtkmm.h>

DuplicateCDProject::DuplicateCDProject(Gtk::Window *parent) : Project(parent)
{
    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    vbox->set_margin(10);
    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    hbox->set_hexpand(true);
    vbox->append(*hbox);
    append(*vbox);
    parent_ = parent;

    CDSource = new RecordCDSource(parent_);
    CDSource->start();
    CDTarget = new RecordCDTarget(parent_);
    CDTarget->start();

    hbox->append(*CDSource);
    hbox->append(*CDTarget);

    hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);

    auto frameBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    simulate_rb = Gtk::make_managed<Gtk::CheckButton>(_("Simulate"));
    simulateBurn_rb = Gtk::make_managed<Gtk::CheckButton>(_("Simulate and Burn"));
    burn_rb = Gtk::make_managed<Gtk::CheckButton>(_("Burn"));

    // Set up radio button group
    simulate_rb->set_group(*simulateBurn_rb);
    simulate_rb->set_group(*burn_rb);

    frameBox->append(*simulate_rb);
    frameBox->append(*simulateBurn_rb);
    frameBox->append(*burn_rb);

    hbox->append(*frameBox);

    Gtk::Image *pixmap = Gtk::make_managed<Gtk::Image>();
    pixmap->set_from_icon_name(Icons::GCDMASTER);
    pixmap->set_icon_size(Gtk::IconSize::LARGE);
    
    Gtk::Label *startLabel = Gtk::make_managed<Gtk::Label>(_("Start"));
    auto startBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    Gtk::Button *button = Gtk::make_managed<Gtk::Button>();
    startBox->append(*pixmap);
    startBox->append(*startLabel);

    button->set_child(*startBox);
    button->signal_clicked().connect(sigc::mem_fun(*this, &DuplicateCDProject::start));

    hbox->append(*button);

    auto hbox2 = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    hbox2->set_halign(Gtk::Align::CENTER);
    hbox2->append(*hbox);
    vbox->append(*hbox2);

    // Create dialogs.
    onTheFlyError = ErrorBox::create(*parent, _("Incorrect Devices"),
				     { _("You need two difference devices to do on-the-fly copy.") });
    noReaderDevice = ErrorBox::create(*parent, _("No Device Selected"),
				      { _("Please select one reader device.") });
    noWriterDevice = ErrorBox::create(*parent, _("No Device Selected"),
				      { _("Please select one recroding device.") });
    genericError = ErrorBox::create(*parent, _("Duplication Failure"),
				    { _("Unable to start Disk-At-Once duplication.") });

    guiUpdate(UPD_ALL);
}

DuplicateCDProject::~DuplicateCDProject()
{
    delete CDSource;
    delete CDTarget;
}

void DuplicateCDProject::start()
{
    DeviceList *sourceList = CDSource->getDeviceList();
    DeviceList *targetList = CDTarget->getDeviceList();

    std::string sourceData = sourceList->selection();
    std::string targetData = targetList->selection();

    if (sourceData.empty()) {
	noReaderDevice->choose();
        return;
    }

    if (targetData.empty()) {
	noWriterDevice->choose();
        return;
    }

    // Read options
    int onTheFly = CDSource->getOnTheFly();
    if (onTheFly) {
        // We can't make on the fly copy with the same device, check that
        // We can only have one source device selected

        if (sourceData == targetData) {
            // If the user selects the same device for reading and writing
            // we can't do on the fly copying. More complex situations with
            // multiple target devices are not handled
            if (CONFIG_MANAGER->getDuplicateOnTheFlyWarning()) {
		onTheFlyError->choose();
		CONFIG_MANAGER->setDuplicateOnTheFlyWarning(false);
		return;
            }
        }
    }

    int correction = CDSource->getCorrection();
    int subChanReadMode = CDSource->getSubChanReadMode();

    // Record options
    int simulate;
    if (simulate_rb->get_active())
        simulate = 1;
    else if (simulateBurn_rb->get_active())
        simulate = 2;
    else
        simulate = 0;

    int multiSession = CDTarget->getMultisession();
    int burnSpeed = CDTarget->getSpeed();
    int eject = CDTarget->checkEjectWarning(parent_);
    if (eject == -1)
        return;

    int reload = CDTarget->checkReloadWarning(parent_);
    if (reload == -1)
        return;

    int buffer = CDTarget->getBuffer();

    CdDevice *readDevice = CdDevice::find(sourceData.c_str());
    if (readDevice == NULL)
        return;

    CdDevice *writeDevice = CdDevice::find(targetData.c_str());
    if (writeDevice == NULL)
        return;

    if (writeDevice->duplicateDao(*parent_, simulate, multiSession, burnSpeed, eject, reload,
                                  buffer, onTheFly, correction, subChanReadMode, readDevice) != 0) {
	genericError->choose();
    } else {
        guiUpdate(UPD_CD_DEVICE_STATUS);
    }
}

bool DuplicateCDProject::closeProject()
{
    return true; // Close the project
}

void DuplicateCDProject::update(unsigned long level)
{
    CDSource->update(level);
    CDTarget->update(level);

    if (level & UPD_CD_DEVICE_STATUS) {
        DeviceList *sourceList = CDSource->getDeviceList();
        DeviceList *targetList = CDTarget->getDeviceList();

        targetList->selectOne();

        if (targetList->selection().empty()) {
            sourceList->selectOne();
        } else {
            sourceList->selectOneBut(targetList->selection().c_str());
        }
    }
}
