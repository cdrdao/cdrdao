/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
/*
 * $Log: DeviceConfDialog.cc,v $
 * Revision 1.3  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.2  2000/02/20 23:34:53  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:13  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/09/06 09:09:37  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: DeviceConfDialog.cc,v 1.3 2000/04/23 09:07:08 andreasm Exp $";

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

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

  active_ = 0;
  selectedRow_ = -1;

  set_title(string("Configure Devices"));
  set_usize(0, 500);


  list_ = new Gtk::CList(6);
  applyButton_ = new Gtk::Button(string(" Apply "));

  list_->set_column_title(0, string("Bus"));
  list_->set_column_justification(0, GTK_JUSTIFY_CENTER);

  list_->set_column_title(1, string("Id"));
  list_->set_column_justification(1, GTK_JUSTIFY_CENTER);

  list_->set_column_title(2, string("Lun"));
  list_->set_column_justification(2, GTK_JUSTIFY_CENTER);

  list_->set_column_title(3, string("Vendor"));
  list_->set_column_justification(3, GTK_JUSTIFY_LEFT);

  list_->set_column_title(4, string("Model"));
  list_->set_column_justification(4, GTK_JUSTIFY_LEFT);

  list_->set_column_title(5, string("Status"));
  list_->set_column_justification(5, GTK_JUSTIFY_LEFT);

  list_->column_titles_show();
  list_->column_titles_passive();
  list_->set_selection_mode(GTK_SELECTION_BROWSE);

  list_->select_row.connect(SigC::slot(this,&DeviceConfDialog::selectRow));

  list_->unselect_row.connect(SigC::slot(this,&DeviceConfDialog::unselectRow));


  Gtk::Menu *dmenu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  for (i = 0; i <= CdDevice::maxDriverId(); i++) {
    mi = manage(new Gtk::MenuItem(CdDevice::driverName(i)));
    mi->activate.connect(bind(slot(this, &DeviceConfDialog::setDriverId), i));
    mi->show();
    dmenu->append(*mi);
  }

  driverMenu_ = new Gtk::OptionMenu;
  driverMenu_->set_menu(dmenu);


  Gtk::Menu *tmenu = manage(new Gtk::Menu);

  for (i = 0; i <= MAX_DEVICE_TYPE_ID; i++) {
    mi = manage(new Gtk::MenuItem(CdDevice::deviceType2string(ID2DEVICE_TYPE[i])));
    mi->activate.connect(bind(slot(this, &DeviceConfDialog::setDeviceType), i));
    mi->show();
    tmenu->append(*mi);
  }

  devtypeMenu_ = new Gtk::OptionMenu;
  devtypeMenu_->set_menu(tmenu);

  Gtk::Adjustment *adjust = new Gtk::Adjustment(0.0, 0.0, 16.0);
  busEntry_ = new Gtk::SpinButton(*adjust, 1.0, 1);
  busEntry_->set_numeric(true);

  adjust = new Gtk::Adjustment(0.0, 0.0, 15.0);
  idEntry_ = new Gtk::SpinButton(*adjust, 1.0, 1);
  idEntry_->set_numeric(true);

  adjust = new Gtk::Adjustment(0.0, 0.0, 8.0);
  lunEntry_ = new Gtk::SpinButton(*adjust, 1.0, 1);
  lunEntry_->set_numeric(true);

  vendorEntry_ = new Gtk::Entry(8);
  productEntry_ = new Gtk::Entry(16);

  specialDeviceEntry_ = new Gtk::Entry;
  driverOptionsEntry_ = new Gtk::Entry;

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);


  // device list
  Gtk::HBox *listHBox = new Gtk::HBox;
  Gtk::VBox *listVBox = new Gtk::VBox;
  Gtk::VBox *listBox = new Gtk::VBox;
  listBox->set_spacing(5);

  hbox = new Gtk::HBox;

  hbox->pack_start(*list_, TRUE, TRUE);
  list_->show();

  adjust = new Gtk::Adjustment(0.0, 0.0, 0.0);

  Gtk::VScrollbar *scrollBar = new Gtk::VScrollbar(*adjust);
  hbox->pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();

  list_->set_vadjustment(*adjust);

  listBox->pack_start(*hbox, TRUE, TRUE);
  hbox->show();

  Gtk::ButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);  //, 5);

  button = new Gtk::Button(string("Rescan"));
  bbox->pack_start(*button);
  button->show();
  button->clicked.connect(SigC::slot(this,&DeviceConfDialog::rescanAction));

  button = new Gtk::Button(string("Delete"));
  bbox->pack_start(*button);
  button->show();
  button->clicked.connect(SigC::slot(this,&DeviceConfDialog::deleteDeviceAction));

  listBox->pack_start(*bbox, FALSE);
  bbox->show();

  listHBox->pack_start(*listBox, TRUE, TRUE, 5);
  listBox->show();
  listVBox->pack_start(*listHBox, TRUE, TRUE, 5);
  listHBox->show();

  Gtk::Frame *listFrame = new Gtk::Frame(string("Devices"));
  listFrame->add(*listVBox);
  listVBox->show();
  contents->pack_start(*listFrame, TRUE, TRUE);
  listFrame->show();

  // device settings
  Gtk::Frame *settingFrame = new Gtk::Frame(string("Device Settings"));
  table = new Gtk::Table(2, 4, FALSE);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  hbox = new Gtk::HBox;
  hbox->pack_start(*table, FALSE, FALSE, 5);
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, FALSE, FALSE, 5);
  settingFrame->add(*vbox);
  vbox->show();
  hbox->show();
  table->show();
  
  label = new Gtk::Label(string("Device Type:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 0, 1);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*devtypeMenu_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 0, 1);
  devtypeMenu_->show();
  hbox->show();

  label = new Gtk::Label(string("Driver:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 1, 2);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*driverMenu_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 1, 2);
  driverMenu_->show();
  hbox->show();

  label = new Gtk::Label(string("Driver Options:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 2, 3);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*driverOptionsEntry_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 2, 3);
  driverOptionsEntry_->show();
  hbox->show();

  label = new Gtk::Label(string("Device Node:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 3, 4);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*specialDeviceEntry_, FALSE, FALSE);
  table->attach(*hbox, 1, 2, 3, 4);
  specialDeviceEntry_->show();
  hbox->show();


  contents->pack_start(*settingFrame, FALSE, FALSE);
  settingFrame->show();


  // add device
  Gtk::Frame *addDeviceFrame = new Gtk::Frame(string("Add Device"));
  Gtk::VBox *addDeviceBox = new Gtk::VBox;
  addDeviceBox->set_spacing(5);

  hbox = new Gtk::HBox;
  hbox->set_spacing(5);

  label = new Gtk::Label(string("Bus:"));
  hbox->pack_start(*label, FALSE);
  hbox->pack_start(*busEntry_, FALSE);
  label->show();
  busEntry_->show();

  label = new Gtk::Label(string("Id:"));
  hbox->pack_start(*label, FALSE);
  hbox->pack_start(*idEntry_, FALSE);
  label->show();
  idEntry_->show();

  label = new Gtk::Label(string("Lun:"));
  hbox->pack_start(*label, FALSE);
  hbox->pack_start(*lunEntry_, FALSE);
  label->show();
  lunEntry_->show();

  addDeviceBox->pack_start(*hbox, FALSE);
  hbox->show();


  table = new Gtk::Table(2, 2, FALSE);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  addDeviceBox->pack_start(*table, FALSE);
  table->show();

  label = new Gtk::Label(string("Vendor:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 0, 1);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*vendorEntry_, TRUE, TRUE);
  table->attach(*hbox, 1, 2, 0, 1);
  vendorEntry_->show();
  hbox->show();

  label = new Gtk::Label(string("Product:"));
  hbox = new Gtk::HBox;
  hbox->pack_end(*label, FALSE, FALSE);
  table->attach(*hbox, 0, 1, 1, 2);
  label->show();
  hbox->show();
  hbox = new Gtk::HBox;
  hbox->pack_start(*productEntry_, TRUE, TRUE);
  table->attach(*hbox, 1, 2, 1, 2);
  productEntry_->show();
  hbox->show();

  bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_START);
  Gtk::Button *addButton = new Gtk::Button(string(" Add "));
  bbox->pack_start(*addButton);
  addButton->show();
  addButton->clicked.connect(SigC::slot(this,&DeviceConfDialog::addDeviceAction));
  addDeviceBox->pack_start(*bbox, FALSE);
  bbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*addDeviceBox, FALSE, FALSE, 5);
  addDeviceBox->show();
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, FALSE, FALSE, 5);
  addDeviceFrame->add(*vbox);
  vbox->show();
  hbox->show();
  
  contents->pack_start(*addDeviceFrame, FALSE, FALSE);
  addDeviceFrame->show();
  

  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();

  get_vbox()->pack_start(*contentsHBox, TRUE, TRUE, 10);
  contentsHBox->show();

  get_vbox()->show();


  bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);

  bbox->pack_start(*applyButton_);
  applyButton_->show();
  applyButton_->clicked.connect(SigC::slot(this,&DeviceConfDialog::applyAction));
  
  Gtk::Button *resetButton = new Gtk::Button(string(" Reset "));
  bbox->pack_start(*resetButton);
  resetButton->show();
  resetButton->clicked.connect(SigC::slot(this,&DeviceConfDialog::resetAction));

  Gtk::Button *cancelButton = new Gtk::Button(string(" Cancel "));
  bbox->pack_start(*cancelButton);
  cancelButton->show();
  cancelButton->clicked.connect(SigC::slot(this,&DeviceConfDialog::cancelAction));

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();
}

DeviceConfDialog::~DeviceConfDialog()
{
  delete list_;
  list_ = NULL;

  delete applyButton_;
  applyButton_ = NULL;
}


void DeviceConfDialog::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void DeviceConfDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void DeviceConfDialog::update(unsigned long level, TocEdit *)
{
  if (!active_)
    return;

  if (level & UPD_CD_DEVICES)
    import();
  else if (level & UPD_CD_DEVICE_STATUS)
    importStatus();
}


gint DeviceConfDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void DeviceConfDialog::cancelAction()
{
  stop();
}

void DeviceConfDialog::resetAction()
{
  import();
}


void DeviceConfDialog::applyAction()
{
  if (selectedRow_ >= 0)
    exportConfiguration(selectedRow_);
  exportData();
  guiUpdate(UPD_CD_DEVICES);
}

void DeviceConfDialog::addDeviceAction()
{
  const char *s;

  string vendor;
  string product;
  int bus = busEntry_->get_value_as_int();
  int id = idEntry_->get_value_as_int();
  int lun = lunEntry_->get_value_as_int();
  CdDevice *dev;

  if ((s = checkString(vendorEntry_->get_text())) == NULL)
    return;
  vendor = s;

  if ((s = checkString(productEntry_->get_text())) == NULL)
    return;
  product = s;

  if (CdDevice::find(bus, id, lun) != NULL) 
    return;

  dev = CdDevice::add(bus, id, lun, vendor.c_str(), product.c_str());

  appendTableEntry(dev);

  list_->cause_select_row(list_->rows() - 1, 0);
  list_->moveto(list_->rows() - 1, 0, 1.0, 0.0);

  guiUpdate(UPD_CD_DEVICES);
}

void DeviceConfDialog::deleteDeviceAction()
{
  DeviceData *data;
  int row = selectedRow_;
  CdDevice *dev;

  if (row >= 0 &&
      (data = (DeviceData*)list_->get_row_data(selectedRow_)) != NULL) {

    dev = CdDevice::find(data->bus, data->id, data->lun);
    if (dev == NULL || dev->status() == CdDevice::DEV_RECORDING ||
	 dev->status() == CdDevice::DEV_BLANKING) {
      // don't remove device that is currently busy
      return;
    }

    CdDevice::remove(data->bus, data->id, data->lun);
    selectedRow_ = -1;
    list_->remove_row(row);
    delete data;

    guiUpdate(UPD_CD_DEVICES);
  }
}

void DeviceConfDialog::rescanAction()
{
  CdDevice::scan();
  guiUpdate(UPD_CD_DEVICES);
}

void DeviceConfDialog::appendTableEntry(CdDevice *dev)
{
  DeviceData *data;
  char buf[50];
  string idStr;
  string busStr;
  string lunStr;
  const gchar *rowStr[6];

  data = new DeviceData;
  data->bus = dev->bus();
  data->id = dev->id();
  data->lun = dev->lun();
  data->driverId = dev->driverId();
  data->options = dev->driverOptions();
  if (dev->specialDevice() != NULL)
    data->specialDevice = dev->specialDevice();

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

  sprintf(buf, "%d", data->bus);
  busStr = buf;
  rowStr[0] = busStr.c_str();

  sprintf(buf, "%d", data->id);
  idStr = buf;
  rowStr[1] = idStr.c_str();

  sprintf(buf, "%d", data->lun);
  lunStr = buf;
  rowStr[2] = lunStr.c_str();

  rowStr[3] = dev->vendor();
  rowStr[4] = dev->product();
  
  rowStr[5] = CdDevice::status2string(dev->status());

  list_->append(rowStr);
  list_->set_row_data(list_->rows() - 1, data);
}

void DeviceConfDialog::import()
{
  CdDevice *drun;
  DeviceData *data;

  selectedRow_ = -1;

  list_->freeze();

  while (list_->rows() > 0) {
    data = (DeviceData*)list_->get_row_data(0);
    list_->remove_row(0);
    delete data;
  }

  for (drun = CdDevice::first(); drun != NULL; drun = CdDevice::next(drun)) {
    appendTableEntry(drun);
  }

  list_->thaw();

  if (list_->rows() > 0) {
    list_->columns_autosize();
    list_->cause_select_row(0, 0);
    list_->moveto(0, 0, 0.0, 0.0);
  }
 

}

void DeviceConfDialog::importConfiguration(int row)
{
  char buf[50];
  DeviceData *data;

  if (row >= 0 && (data = (DeviceData*)list_->get_row_data(row)) != NULL) {
    driverMenu_->set_sensitive(true);
    driverMenu_->set_history(data->driverId);

    devtypeMenu_->set_sensitive(true);
    devtypeMenu_->set_history(data->deviceType);

    driverOptionsEntry_->set_sensitive(true);
    sprintf(buf, "0x%lx", data->options);
    driverOptionsEntry_->set_text(string(buf));

    specialDeviceEntry_->set_sensitive(true);
    specialDeviceEntry_->set_text(data->specialDevice);
  }
  else {
    driverMenu_->set_history(0);
    driverMenu_->set_sensitive(false);

    devtypeMenu_->set_history(0);
    devtypeMenu_->set_sensitive(false);

    driverOptionsEntry_->set_text(string(""));
    driverOptionsEntry_->set_sensitive(false);

    specialDeviceEntry_->set_text(string(""));
    specialDeviceEntry_->set_sensitive(false);
  }
}

void DeviceConfDialog::importStatus()
{
  int i;
  DeviceData *data;
  CdDevice *dev;

  for (i = 0; i < list_->rows(); i++) {
    if ((data = (DeviceData*)list_->get_row_data(i)) != NULL &&
	(dev = CdDevice::find(data->bus, data->id, data->lun)) != NULL) {
      list_->set_text(i, 5, string(CdDevice::status2string(dev->status())));
    }
  }

  list_->columns_autosize();
}

void DeviceConfDialog::exportConfiguration(int row)
{
  DeviceData *data;
  const char *s;

  if (row >= 0 && (data = (DeviceData*)list_->get_row_data(row)) != NULL) {
    data->options = strtoul(driverOptionsEntry_->get_text().c_str(), NULL, 0);

    s = checkString(specialDeviceEntry_->get_text());

    if (s == NULL)
      data->specialDevice = "";
    else
      data->specialDevice = s;
  }

}

void DeviceConfDialog::exportData()
{
  int i;
  DeviceData *data;
  CdDevice *dev;
  string s;

  for (i = 0; i < list_->rows(); i++) {
    if ((data = (DeviceData*)list_->get_row_data(i)) != NULL) {
      if ((dev = CdDevice::find(data->bus, data->id, data->lun)) != NULL) {
	if (dev->driverId() != data->driverId) {
	  dev->driverId(data->driverId);
	  dev->manuallyConfigured(1);
	}

	if (dev->deviceType() != ID2DEVICE_TYPE[data->deviceType]) {
	  dev->deviceType(ID2DEVICE_TYPE[data->deviceType]);
	  dev->manuallyConfigured(1);
	}

	if (dev->driverOptions() != data->options) {
	  dev->driverOptions(data->options);
	  dev->manuallyConfigured(1);
	}

	if ((dev->specialDevice() == NULL && data->specialDevice.c_str()[0] != 0) ||
	    (dev->specialDevice() != NULL &&
	     strcmp(dev->specialDevice(), data->specialDevice.c_str()) != 0)) {
	  dev->specialDevice(data->specialDevice.c_str());
	  dev->manuallyConfigured(1);
	}
      }
    }
  }
}



void DeviceConfDialog::setDriverId(int id)
{
  DeviceData *data;

  if (selectedRow_ >= 0 && id >= 0 && id <= CdDevice::maxDriverId() &&
      (data = (DeviceData*)list_->get_row_data(selectedRow_)) != NULL) {
    data->driverId = id;
  }
}

void DeviceConfDialog::setDeviceType(int id)
{
  DeviceData *data;

  if (selectedRow_ >= 0 && id >= 0 && id <= CdDevice::maxDriverId() &&
      (data = (DeviceData*)list_->get_row_data(selectedRow_)) != NULL) {
    data->deviceType = id;
  }
}

void DeviceConfDialog::selectRow(gint row, gint column, GdkEvent *event)
{
  selectedRow_ = row;
  importConfiguration(row);
}

void DeviceConfDialog::unselectRow(gint row, gint column, GdkEvent *event)
{
  if (selectedRow_ >= 0)
    exportConfiguration(selectedRow_);

  selectedRow_ = -1;

  importConfiguration(-1);
}

const char *DeviceConfDialog::checkString(const string &str)
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
