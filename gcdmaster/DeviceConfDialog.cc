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
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <vector>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "DeviceConfDialog.h"

#include "CdDevice.h"
#include "guiUpdate.h"

#include "util.h"

static std::vector<CdDevice::DeviceType> ID2DEVICE_TYPE =
{ CdDevice::CD_ROM, CdDevice::CD_R, CdDevice::CD_RW };

DeviceConfDialog::DeviceConfDialog()
{
    int i;
    Gtk::Label *label;
    Gtk::Grid *grid;
    Gtk::Button *button;

    active_ = false;

    set_title(_("Configure Devices"));

    // TreeView initialization
    listModel_ = Gtk::ListStore::create(listColumns_);
    list_.set_model(listModel_);
    list_.append_column(_("Dev"), listColumns_.dev);
    list_.append_column(_("Vendor"), listColumns_.vendor);
    list_.append_column(_("Model"), listColumns_.model);
    list_.append_column(_("Status"), listColumns_.status);
    list_.set_vexpand(true);
    list_.set_hexpand(true);

    if (list_.get_selection()) {
        selectedRow_ = list_.get_selection()->get_selected();
        list_.get_selection()->signal_changed().connect(
            sigc::mem_fun(*this, &DeviceConfDialog::selectionChanged));
    }

    // Create string lists for dropdown menus
    driverStringList_ = Gtk::StringList::create({});
    for (i = 0; i <= CdDevice::maxDriverId(); i++) {
        driverStringList_->append(CdDevice::driverNames(i));
    }
    driverMenu_.set_model(driverStringList_);
    driverMenu_.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &DeviceConfDialog::setDriverId));

    devtypeStringList_ = Gtk::StringList::create({});
    for (auto i : ID2DEVICE_TYPE) {
        devtypeStringList_->append(CdDevice::deviceType2string(i));
    }
    devtypeMenu_.set_model(devtypeStringList_);
    devtypeMenu_.property_selected().signal_changed().connect(
        sigc::mem_fun(*this, &DeviceConfDialog::setDeviceType));

    devEntry_.set_max_length(32);
    vendorEntry_.set_max_length(8);
    productEntry_.set_max_length(16);

    auto contents = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    contents->set_margin(7);

    // ---------------------------- Device list
    auto listBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    listBox->set_margin(5);

    auto scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolled->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolled->set_child(list_);
    scrolled->set_vexpand(true);
    scrolled->set_hexpand(true);

    listBox->append(*scrolled);

    auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);

    button = Gtk::make_managed<Gtk::Button>(_("Rescan"));
    bbox->append(*button);
    button->signal_clicked().connect(sigc::mem_fun(*this, &DeviceConfDialog::rescanAction));

    button = Gtk::make_managed<Gtk::Button>();
    button->set_icon_name("user-trash-symbolic");
    bbox->append(*button);
    button->signal_clicked().connect(sigc::mem_fun(*this, &DeviceConfDialog::deleteDeviceAction));

    listBox->append(*bbox);

    listFrame_.set_label(_(" Device List "));
    listFrame_.set_vexpand(true);
    listFrame_.set_child(*listBox);
    contents->append(listFrame_);

    // ---------------------------- Device settings

    settingFrame_.set_label(_(" Device Settings "));
    grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(5);
    grid->set_margin(10);
    settingFrame_.set_child(*grid);

    label = Gtk::make_managed<Gtk::Label>(_("Device Type:"));
    label->set_halign(Gtk::Align::START);
    grid->attach(*label, 0, 0, 1, 1);
    devtypeMenu_.set_hexpand(true);
    grid->attach(devtypeMenu_, 1, 0, 1, 1);

    label = Gtk::make_managed<Gtk::Label>(_("Driver:"));
    label->set_halign(Gtk::Align::START);
    grid->attach(*label, 0, 1, 1, 1);
    driverMenu_.set_hexpand(true);
    grid->attach(driverMenu_, 1, 1, 1, 1);

    label = Gtk::make_managed<Gtk::Label>(_("Driver Options:"));
    label->set_halign(Gtk::Align::START);
    grid->attach(*label, 0, 2, 1, 1);
    driverOptionsEntry_.set_hexpand(true);
    grid->attach(driverOptionsEntry_, 1, 2, 1, 1);

    contents->append(settingFrame_);

    // -------------- Add device

    addDeviceFrame_.set_label(_(" Add Device "));
    auto addDeviceBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);
    addDeviceBox->set_margin(5);

    grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(5);
    grid->set_vexpand(true);
    grid->set_hexpand(true);
    addDeviceBox->append(*grid);

    label = Gtk::make_managed<Gtk::Label>(_("Device:"));
    label->set_halign(Gtk::Align::START);
    grid->attach(*label, 0, 0, 1, 1);
    devEntry_.set_hexpand(true);
    grid->attach(devEntry_, 1, 0, 1, 1);

    label = Gtk::make_managed<Gtk::Label>(_("Vendor:"));
    label->set_halign(Gtk::Align::START);
    grid->attach(*label, 0, 1, 1, 1);
    vendorEntry_.set_hexpand(true);
    grid->attach(vendorEntry_, 1, 1, 1, 1);

    label = Gtk::make_managed<Gtk::Label>(_("Product:"));
    label->set_halign(Gtk::Align::START);
    grid->attach(*label, 0, 2, 1, 1);
    productEntry_.set_hexpand(true);
    grid->attach(productEntry_, 1, 2, 1, 1);

    bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    bbox->set_halign(Gtk::Align::FILL);
    auto addButton = Gtk::make_managed<Gtk::Button>();
    addButton->set_icon_name("list-add-symbolic");
    addButton->set_hexpand(true);
    bbox->append(*addButton);
    addButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceConfDialog::addDeviceAction));
    addDeviceBox->append(*bbox);

    addDeviceFrame_.set_child(*addDeviceBox);
    contents->append(addDeviceFrame_);

    // 3 buttons at bottom of window.

    bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    bbox->set_halign(Gtk::Align::FILL);
    auto applyButton = Gtk::make_managed<Gtk::Button>();
    applyButton->set_hexpand(true);
    applyButton->set_icon_name("emblem-ok-symbolic");
    bbox->append(*applyButton);
    applyButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceConfDialog::applyAction));

    auto resetButton = Gtk::make_managed<Gtk::Button>(_("Reset"));
    bbox->append(*resetButton);
    resetButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceConfDialog::resetAction));

    auto cancelButton = Gtk::make_managed<Gtk::Button>();
    cancelButton->set_icon_name("window-close-symbolic");
    bbox->append(*cancelButton);
    cancelButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceConfDialog::closeAction));

    contents->append(*bbox);

    set_child(*contents);
}

DeviceConfDialog::~DeviceConfDialog()
{
}

void DeviceConfDialog::start()
{
    if (active_) {
        present();
        return;
    }

    active_ = true;
    update(UPD_CD_DEVICES);
    set_visible(true);
}

void DeviceConfDialog::stop()
{
    set_visible(false);
    active_ = false;
}

void DeviceConfDialog::update(unsigned long level)
{
    if (!active_)
        return;

    if (level & UPD_CD_DEVICES)
        import();
    else if (level & UPD_CD_DEVICE_STATUS)
        importStatus();
}

void DeviceConfDialog::closeAction()
{
    stop();
}

void DeviceConfDialog::resetAction()
{
    import();
}

void DeviceConfDialog::applyAction()
{
    if (selectedRow_)
        exportConfiguration(selectedRow_);
    exportData();
    guiUpdate(UPD_CD_DEVICES);
}

void DeviceConfDialog::addDeviceAction()
{
    const char *s;

    std::string dev;
    std::string vendor;
    std::string product;
    CdDevice *cddev;

    if ((s = checkString(devEntry_.get_text())) == NULL)
        return;
    dev = s;

    if ((s = checkString(vendorEntry_.get_text())) == NULL)
        return;
    vendor = s;

    if ((s = checkString(productEntry_.get_text())) == NULL)
        return;
    product = s;

    if (CdDevice::find(dev.c_str()) != NULL)
        return;

    cddev = CdDevice::add(dev.c_str(), vendor.c_str(), product.c_str());

    if (cddev) {
        cddev->manuallyConfigured(true);
        Gtk::TreeModel::iterator new_entry = appendTableEntry(cddev);
        list_.get_selection()->select(new_entry);
    }

    guiUpdate(UPD_CD_DEVICES);
}

void DeviceConfDialog::deleteDeviceAction()
{
    DeviceData *data;
    CdDevice *dev;

    if (selectedRow_) {

        data = (*selectedRow_)[listColumns_.data];

        dev = CdDevice::find(data->dev.c_str());
        if (dev == NULL || dev->status() == CdDevice::DEV_RECORDING ||
            dev->status() == CdDevice::DEV_BLANKING) {
            // don't remove device that is currently busy
            return;
        }

        CdDevice::remove(data->dev.c_str());
        listModel_->erase(selectedRow_);
        list_.get_selection()->unselect_all();
        selectedRow_ = list_.get_selection()->get_selected();
        delete data;

        guiUpdate(UPD_CD_DEVICES);
    }
}

void DeviceConfDialog::rescanAction()
{
    CdDevice::scan();
    guiUpdate(UPD_CD_DEVICES);
}

Gtk::TreeModel::iterator DeviceConfDialog::appendTableEntry(CdDevice *dev)
{
    DeviceData *data;

    data = new DeviceData;
    data->dev = dev->dev();
    data->driverId = dev->driverId();
    data->options = dev->driverOptions();

    switch (dev->deviceType()) {
    case CdDevice::CD_ROM:
        data->deviceType = 0;
        break;
    case CdDevice::CD_R:
        data->deviceType = 1;
        break;
    case CdDevice::CD_RW:
        data->deviceType = 2;
        break;
    }

    Gtk::TreeModel::iterator newiter = listModel_->append();
    Gtk::TreeModel::Row row = *newiter;
    row[listColumns_.dev] = data->dev;
    row[listColumns_.vendor] = dev->vendor();
    row[listColumns_.model] = dev->product();
    row[listColumns_.status] = CdDevice::status2string(dev->status());
    row[listColumns_.data] = data;

    return newiter;
}

void DeviceConfDialog::import()
{
    CdDevice *drun;

    list_.get_selection()->unselect_all();
    selectedRow_ = list_.get_selection()->get_selected();

    listModel_->clear();

    for (drun = CdDevice::first(); drun != NULL; drun = CdDevice::next(drun)) {
        appendTableEntry(drun);
    }

    if (listModel_->children().size() > 0) {
        list_.columns_autosize();
        list_.get_selection()->select(Gtk::TreeModel::Path("0"));
    }
}

void DeviceConfDialog::importConfiguration(Gtk::TreeModel::iterator row)
{
    char buf[50];
    DeviceData *data;

    if (selectedRow_) {

        data = (*selectedRow_)[listColumns_.data];
        driverMenu_.set_sensitive(true);
        driverMenu_.set_selected(data->driverId);
        devtypeMenu_.set_sensitive(true);
        devtypeMenu_.set_selected(data->deviceType);
        driverOptionsEntry_.set_sensitive(true);
        snprintf(buf, sizeof(buf), "0x%lx", data->options);
        driverOptionsEntry_.set_text(buf);
    } else {

        driverMenu_.set_selected(0);
        driverMenu_.set_sensitive(false);
        devtypeMenu_.set_selected(0);
        devtypeMenu_.set_sensitive(false);
        driverOptionsEntry_.set_text("");
        driverOptionsEntry_.set_sensitive(false);
    }
}

void DeviceConfDialog::importStatus()
{
    DeviceData *data;
    CdDevice *dev;

    Gtk::TreeNodeChildren ch = listModel_->children();
    for (unsigned i = 0; i < ch.size(); i++) {
        Gtk::TreeRow row = ch[i];
        data = row[listColumns_.data];
        if (data && (dev = CdDevice::find(data->dev.c_str()))) {
            row[listColumns_.status] = CdDevice::status2string(dev->status());
        }
    }

    list_.columns_autosize();
}

void DeviceConfDialog::exportConfiguration(Gtk::TreeModel::iterator row)
{
    DeviceData *data;

    if (row) {
        data = (*row)[listColumns_.data];

        if (data) {
            data->options = strtoul(driverOptionsEntry_.get_text().c_str(), NULL, 0);
        }
    }
}

void DeviceConfDialog::exportData()
{
    DeviceData *data;
    CdDevice *dev;

    Gtk::TreeNodeChildren ch = listModel_->children();
    for (unsigned i = 0; i < ch.size(); i++) {
        Gtk::TreeRow row = ch[i];
        data = row[listColumns_.data];
        if (data && (dev = CdDevice::find(data->dev.c_str()))) {

            if (dev->driverId() != data->driverId) {
                dev->driverId(data->driverId);
                dev->manuallyConfigured(true);
            }

            if (dev->deviceType() != ID2DEVICE_TYPE[data->deviceType]) {
                dev->deviceType(ID2DEVICE_TYPE[data->deviceType]);
                dev->manuallyConfigured(true);
            }

            if (dev->driverOptions() != data->options) {
                dev->driverOptions(data->options);
                dev->manuallyConfigured(true);
            }
        }
    }
}

void DeviceConfDialog::setDriverId(void)
{
    auto selected = driverMenu_.get_selected();
    if (selectedRow_ && selected != GTK_INVALID_LIST_POSITION && 
        selected <= static_cast<guint>(CdDevice::maxDriverId())) {
        DeviceData *data = (*selectedRow_)[listColumns_.data];
        if (data)
            data->driverId = selected;
    }
}

void DeviceConfDialog::setDeviceType(void)
{
    auto selected = devtypeMenu_.get_selected();
    if (selectedRow_ && selected != GTK_INVALID_LIST_POSITION && 
        selected < ID2DEVICE_TYPE.size()) {
        DeviceData *data = (*selectedRow_)[listColumns_.data];
        if (data)
            data->deviceType = selected;
    }
}

void DeviceConfDialog::selectionChanged()
{
    Gtk::TreeModel::iterator new_sel = list_.get_selection()->get_selected();

    if ((bool)selectedRow_ != (bool)new_sel || selectedRow_ != new_sel) {

        if (selectedRow_)
            exportConfiguration(selectedRow_);

        selectedRow_ = new_sel;
        importConfiguration(selectedRow_);
    }
}

const char *DeviceConfDialog::checkString(const std::string &str)
{
    static char *buf = NULL;
    static long bufLen = 0;
    char *p, *s;
    long len = strlen(str.c_str());

    if (len == 0)
        return NULL;

    if (buf == NULL || len + 1 > bufLen) {
        delete[] buf;
        bufLen = len + 1;
        buf = new char[bufLen];
    }

    strcpy(buf, str.c_str());

    s = buf;
    p = buf + len - 1;

    while (*s != 0 && isspace(*s))
        s++;

    if (*s == 0)
        return NULL;

    while (p > s && isspace(*p)) {
        *p = 0;
        p--;
    }

    return s;
}

