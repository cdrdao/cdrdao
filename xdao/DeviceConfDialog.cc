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

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#include <gtkmm.h>
#include <gnome.h>

#include "DeviceConfDialog.h"

#include "CdDevice.h"
#include "guiUpdate.h"

#include "util.h"

#define MAX_DEVICE_TYPE_ID 2

static CdDevice::DeviceType ID2DEVICE_TYPE[MAX_DEVICE_TYPE_ID + 1] = {
  CdDevice::CD_ROM,
  CdDevice::CD_R,
  CdDevice::CD_RW
};
    

DeviceConfDialog::DeviceConfDialog()
{
  int i;
  Gtk::Label *label;
  Gtk::Table *table;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
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

  selectedRow_ = list_.get_selection()->get_selected();
  list_.get_selection()->signal_changed().
      connect(mem_fun(*this, &DeviceConfDialog::selectionChanged));

  Gtk::Menu *dmenu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  for (i = 0; i <= CdDevice::maxDriverId(); i++) {
    mi = manage(new Gtk::MenuItem(CdDevice::driverName(i)));
    mi->signal_activate().connect(bind(mem_fun(*this,
                                            &DeviceConfDialog::setDriverId),
                                       i));
    mi->show();
    dmenu->append(*mi);
  }

  driverMenu_ = manage(new Gtk::OptionMenu);
  driverMenu_->set_menu(*dmenu);

  Gtk::Menu *tmenu = manage(new Gtk::Menu);

  for (i = 0; i <= MAX_DEVICE_TYPE_ID; i++) {
    mi = manage(new
                Gtk::MenuItem(CdDevice::deviceType2string(ID2DEVICE_TYPE[i])));
    mi->signal_activate().connect(bind(mem_fun(*this,
                                            &DeviceConfDialog::setDeviceType),
                                       i));
    mi->show();
    tmenu->append(*mi);
  }

  devtypeMenu_ = manage(new Gtk::OptionMenu);
  devtypeMenu_->set_menu(*tmenu);

  devEntry_.set_max_length(32);
  vendorEntry_.set_max_length(8);
  productEntry_.set_max_length(16);

  Gtk::VBox *contents = manage(new Gtk::VBox);
  contents->set_spacing(5);
  contents->set_border_width(7);

  // ---------------------------- Device list
  Gtk::VBox *listBox = manage(new Gtk::VBox);
  listBox->set_spacing(5);
  listBox->set_border_width(5);

  hbox = manage(new Gtk::HBox);

  hbox->pack_start(list_, Gtk::PACK_EXPAND_WIDGET);

  Gtk::Adjustment *adjust = manage(new Gtk::Adjustment(0.0, 0.0, 0.0));
  Gtk::VScrollbar *scrollBar = manage(new Gtk::VScrollbar(*adjust));
  hbox->pack_start(*scrollBar, Gtk::PACK_SHRINK);

  list_.set_vadjustment(*adjust);

  listBox->pack_start(*hbox, Gtk::PACK_EXPAND_WIDGET);

  Gtk::ButtonBox *bbox = manage(new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD));

  button = manage(new Gtk::Button(_("Rescan")));
  bbox->pack_start(*button);
  button->signal_clicked().
    connect(sigc::mem_fun(*this,&DeviceConfDialog::rescanAction));

  button = manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::DELETE)));
  bbox->pack_start(*button);
  button->signal_clicked().
    connect(sigc::mem_fun(*this,&DeviceConfDialog::deleteDeviceAction));

  listBox->pack_start(*bbox, Gtk::PACK_SHRINK);

  listFrame_.set_label(_(" Device List "));
  listFrame_.add(*listBox);
  contents->pack_start(listFrame_, Gtk::PACK_EXPAND_WIDGET);

  // ---------------------------- Device settings

  settingFrame_.set_label(_(" Device Settings "));
  table = manage(new Gtk::Table(2, 4, FALSE));
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  table->set_border_width(10);
  settingFrame_.add(*table);
  
  label = manage(new Gtk::Label(_("Device Type:")));
  table->attach(*label, 0, 1, 0, 1);
  table->attach(*devtypeMenu_, 1, 2, 0, 1);

  label = manage(new Gtk::Label(_("Driver:")));
  table->attach(*label, 0, 1, 1, 2);
  table->attach(*driverMenu_, 1, 2, 1, 2);

  label = manage(new Gtk::Label(_("Driver Options:")));
  table->attach(*label, 0, 1, 2, 3);
  table->attach(driverOptionsEntry_, 1, 2, 2, 3);

  contents->pack_start(settingFrame_, Gtk::PACK_SHRINK);

  // -------------- Add device

  addDeviceFrame_.set_label(_(" Add Device "));
  Gtk::VBox *addDeviceBox = manage(new Gtk::VBox);
  addDeviceBox->set_spacing(5);
  addDeviceBox->set_border_width(5);

  table = manage(new Gtk::Table(3, 2, FALSE));
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  addDeviceBox->pack_start(*table, Gtk::PACK_EXPAND_WIDGET);

  label = manage(new Gtk::Label(_("Device:")));
  table->attach(*label, 0, 1, 0, 1);
  table->attach(devEntry_, 1, 2, 0, 1);

  label = manage(new Gtk::Label(_("Vendor:")));
  table->attach(*label, 0, 1, 1, 2);
  table->attach(vendorEntry_, 1, 2, 1, 2);

  label = manage(new Gtk::Label(_("Product:")));
  table->attach(*label, 0, 1, 2, 3);
  table->attach(productEntry_, 1, 2, 2, 3);

  bbox = manage(new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD));
  bbox->set_spacing(5);
  Gtk::Button* addButton =
      manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::ADD)));
  bbox->pack_start(*addButton);
  addButton->signal_clicked().
      connect(mem_fun(*this, &DeviceConfDialog::addDeviceAction));
  addDeviceBox->pack_start(*bbox);

  addDeviceFrame_.add(*addDeviceBox);
  contents->pack_start(addDeviceFrame_, Gtk::PACK_SHRINK);

  // 3 buttons at bottom of window.

  bbox = manage(new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD));
  bbox->set_spacing(5);
  hbox->set_border_width(10);
  Gtk::Button* applyButton =
    manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::APPLY)));
  bbox->pack_start(*applyButton);
  applyButton->signal_clicked().connect(mem_fun(*this,
                                             &DeviceConfDialog::applyAction));
  
  Gtk::Button *resetButton = manage(new Gtk::Button(_("Reset")));
  bbox->pack_start(*resetButton);
  resetButton->signal_clicked().connect(mem_fun(*this,
                                             &DeviceConfDialog::resetAction));

  Gtk::Button *cancelButton =
    manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::CLOSE)));
  bbox->pack_start(*cancelButton);
  cancelButton->signal_clicked().connect(mem_fun(*this,
                                              &DeviceConfDialog::closeAction));

  contents->pack_start(*bbox, Gtk::PACK_SHRINK);

  add(*contents);
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
  show_all();
}

void DeviceConfDialog::stop()
{
  hide();
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
      Gtk::TreeIter new_entry = appendTableEntry(cddev);
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

Gtk::TreeIter DeviceConfDialog::appendTableEntry(CdDevice *dev)
{
  DeviceData *data;
  const gchar *rowStr[6];

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

  Gtk::TreeIter newiter = listModel_->append();
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
  DeviceData *data;

  list_.get_selection()->unselect_all();
  selectedRow_ = list_.get_selection()->get_selected();

  listModel_->clear();

  for (drun = CdDevice::first(); drun != NULL; drun = CdDevice::next(drun)) {
    appendTableEntry(drun);
  }

  if (listModel_->children().size() > 0) {
    list_.columns_autosize();
    list_.get_selection()->select(Gtk::TreeModel::Path((unsigned)1));
  }
}

void DeviceConfDialog::importConfiguration(Gtk::TreeIter row)
{
  char buf[50];
  DeviceData *data;

  if (selectedRow_) {

    data = (*selectedRow_)[listColumns_.data];
    driverMenu_->set_sensitive(true);
    driverMenu_->set_history(data->driverId);
    devtypeMenu_->set_sensitive(true);
    devtypeMenu_->set_history(data->deviceType);
    driverOptionsEntry_.set_sensitive(true);
    sprintf(buf, "0x%lx", data->options);
    driverOptionsEntry_.set_text(buf);

  } else {

    driverMenu_->set_history(0);
    driverMenu_->set_sensitive(false);
    devtypeMenu_->set_history(0);
    devtypeMenu_->set_sensitive(false);
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

void DeviceConfDialog::exportConfiguration(Gtk::TreeIter row)
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
  std::string s;

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



void DeviceConfDialog::setDriverId(int id)
{
  DeviceData *data;

  if (selectedRow_ && id >= 0 && id <= CdDevice::maxDriverId()) {
    data = (*selectedRow_)[listColumns_.data];
    if (data)
      data->driverId = id;
  }
}

void DeviceConfDialog::setDeviceType(int id)
{
  DeviceData *data;

  if (selectedRow_ && id >= 0 && id <= CdDevice::maxDriverId()) {
    data = (*selectedRow_)[listColumns_.data];
    if (data)
      data->deviceType = id;
  }
}

void DeviceConfDialog::selectionChanged()
{
  Gtk::TreeIter new_sel = list_.get_selection()->get_selected();

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
