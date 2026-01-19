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
#include <string>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "DeviceList.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "TocEdit.h"
#include "guiUpdate.h"

#include "util.h"

DeviceList::DeviceList(CdDevice::DeviceType filterType)
{
    filterType_ = filterType;

    listModel_ = Gtk::ListStore::create(listColumns_);
    list_.set_model(listModel_);
    list_.append_column(_("Vendor"), listColumns_.vendor);
    list_.append_column(_("Model"), listColumns_.model);
    list_.append_column(_("Status"), listColumns_.status);
    list_.set_expand(true);
    list_.set_halign(Gtk::Align::FILL);

    auto contents = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);

    // available device list
    auto listHBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    listHBox->set_expand(true);
    listHBox->set_halign(Gtk::Align::FILL);
    listHBox->set_margin(5);
    auto listVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);

    auto hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    hbox->set_expand(true);
    hbox->set_halign(Gtk::Align::FILL);
    hbox->set_margin(5);
    hbox->append(list_);

    Glib::RefPtr<Gtk::Adjustment> adjust = Gtk::Adjustment::create(0.0, 0.0, 0.0);

    auto scrollBar = new Gtk::Scrollbar(adjust, Gtk::Orientation::VERTICAL);
    hbox->append(*scrollBar);

    list_.set_vadjustment(adjust);

    listHBox->append(*hbox);
    listVBox->append(*listHBox);

    switch (filterType_) {
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

    set_child(*listVBox);
}

std::string DeviceList::selection()
{
    auto selection = list_.get_selection();
    Gtk::TreeIter i = selection->get_selected();

    if (i) {
	return (*i)[listColumns_.dev];
    }
    return std::string();
}

void DeviceList::appendTableEntry(CdDevice *dev)
{
    auto newiter = listModel_->append();
    auto row = *newiter;
    row[listColumns_.dev] = dev->dev();
    row[listColumns_.vendor] = dev->vendor();
    row[listColumns_.model] = dev->product();
    row[listColumns_.status] = CdDevice::status2string(dev->status());

    if (dev->status() == CdDevice::DEV_READY)
        list_.get_selection()->select(newiter);
}

void DeviceList::import()
{
    listModel_->clear();

    for (CdDevice* drun = CdDevice::first(); drun != NULL;
	 drun = CdDevice::next(drun)) {
	bool match = false;
        switch (filterType_) {
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
	if (match && drun->driverId() > 0)
	    appendTableEntry(drun);
    }
    if (!listModel_->children().empty()) {
        list_.columns_autosize();
	selectOne();
    }
}

void DeviceList::importStatus()
{
    std::string data;
    CdDevice *cddev;

    Gtk::TreeNodeChildren ch = listModel_->children();
    for (unsigned i = 0; i < ch.size(); i++) {
        Gtk::TreeRow row = ch[i];
        data = row[listColumns_.dev];

        if ((cddev = CdDevice::find(data.c_str()))) {
            if (cddev->status() == CdDevice::DEV_READY)
                list_.get_column(i)->set_clickable(true);
            else
                list_.get_column(i)->set_clickable(false);

            row[listColumns_.status] = CdDevice::status2string(cddev->status());
        }
    }

    list_.columns_autosize();
}

void DeviceList::selectOne()
{
    auto selection = list_.get_selection();
    if (selection->count_selected_rows() > 0)
        return;

    auto children = listModel_->children();
    if (!children.empty())
	selection->select(children.begin());
}

void DeviceList::selectOneBut(const std::string& targetData)
{
    if (targetData.empty()) {
        selectOne();
	return;
    }

    auto selection = list_.get_selection();
    if (selection->count_selected_rows() > 0)
	return;

    for (auto const& row : listModel_->children()) {
	std::string sourceData = row[listColumns_.dev];
	if (sourceData != targetData) {
	    selection->select(row.get_iter());
	    return;
	}
    }

    // Fallback if no other device found.
    selectOne();
}
