/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2026 Denis Leroy <denis@poolshark.org>
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
#include <string>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "DeviceSelector.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "TocEdit.h"
#include "guiUpdate.h"
#include "log.h"
#include "util.h"

DeviceSelector::DeviceSelector(std::optional<CdDevice::DeviceType> filterType)
    : filterType_(filterType)
{
    listModel_ = Gio::ListStore<ListColumns>::create();

    selectionModel_ = Gtk::SingleSelection::create(listModel_);
    selectionModel_->set_autoselect(true);
    selectionModel_->set_can_unselect(false);
    colView_.set_model(selectionModel_);

    // Setting up ColumnView is total complete overengineered madness.

    // Setup function, just creates an empty label.
    auto setup_empty = [](const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto label = Gtk::make_managed<Gtk::Label>("", Gtk::Align::START);
	list_item->set_child(*label);
    };
    auto bind_desc = [](const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto it = std::dynamic_pointer_cast<ListColumns>(list_item->get_item());
	if (!it)
	    return;
	auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
	if (label)
	    label->set_text(it->device_->description());
    };
    auto bind_status = [](const Glib::RefPtr<Gtk::ListItem>& list_item) {
	auto it = std::dynamic_pointer_cast<ListColumns>(list_item->get_item());
	if (!it)
	    return;
	auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
	if (label)
	    label->set_text(CdDevice::status2string(it->device_->status()));
    };
    
    { // Description column
	auto factory = Gtk::SignalListItemFactory::create();
	factory->signal_setup().connect(setup_empty);
	factory->signal_bind().connect(bind_desc);
	auto cvc = Gtk::ColumnViewColumn::create(_("Descripton"), factory);
	cvc->set_expand(true);
	colView_.append_column(cvc);
    }
    { // Status column
	auto factory = Gtk::SignalListItemFactory::create();
	factory->signal_setup().connect(setup_empty);
	factory->signal_bind().connect(bind_status);
	auto cvc = Gtk::ColumnViewColumn::create(_("Status"), factory);
	cvc->set_expand(true);
	colView_.append_column(cvc);
    }

    colView_.set_expand(true);
    colView_.set_hexpand(true);
    colView_.set_halign(Gtk::Align::FILL);

    auto listVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    {
	auto scrWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
	scrWindow->set_child(colView_);
	scrWindow->add_css_class("device-view");
	listVBox->append(*scrWindow);
    }
    set_child(*listVBox);

    if (filterType_.has_value()) {
	switch (filterType_.value()) {
	case CdDevice::CD_ROM:
	    set_label(_(" Available Reader Devices "));
	    break;
	case CdDevice::CD_R:
	    set_label(_(" Available Recorder Devices "));
	    break;
	case CdDevice::CD_RW:
	    set_label(_(" Available Recorder (RW) Devices "));
	    break;
	}
    } else {
	set_label(_(" Available Devices: "));
    }

}

CdDevice* DeviceSelector::selection()
{
    auto robj = selectionModel_->get_selected_item();
    if (!robj)
	return nullptr;

    auto obj = std::dynamic_pointer_cast<ListColumns>(robj);

    return obj->device_;
}

void DeviceSelector::appendTableEntry(CdDevice *device)
{
    auto entry = ListColumns::create(device);
    listModel_->append(entry);
}

void DeviceSelector::import()
{
    log_message(0, "DeviceSelector::import");
    listModel_->remove_all();

    for (auto drun : CdDevice::deviceList()) {
	bool match = false;
	if (!filterType_.has_value()) {
	    match = true;
	} else {
	    switch (filterType_.value()) {
	    case CdDevice::CD_ROM:
		match = (drun->deviceType() == CdDevice::CD_ROM ||
			 drun->deviceType() == CdDevice::CD_R ||
			 drun->deviceType() == CdDevice::CD_RW);
		break;
	    case CdDevice::CD_R:
		match = (drun->deviceType() == CdDevice::CD_R ||
			 drun->deviceType() == CdDevice::CD_RW);
		break;
	    case CdDevice::CD_RW:
		match = (drun->deviceType() == CdDevice::CD_RW);
		break;
	    }
	}
	if (match && drun->driverId() > 0)
	    appendTableEntry(drun);
    }
}

void DeviceSelector::importStatus()
{
}

void DeviceSelector::selectOne()
{
    if (!listModel_->get_n_items() > 0)
	selectionModel_->set_selected(0);
}
