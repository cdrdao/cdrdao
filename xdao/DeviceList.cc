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

#include <gnome.h>

#include "DeviceList.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "util.h"

DeviceList::DeviceList(CdDevice::DeviceType filterType)
{
  Gtk::HBox *hbox;

  filterType_ = filterType;

  list_ = new Gtk::CList(6);

  list_->set_column_title(0, "Bus");
  list_->set_column_justification(0, GTK_JUSTIFY_CENTER);

  list_->set_column_title(1, "Id");
  list_->set_column_justification(1, GTK_JUSTIFY_CENTER);

  list_->set_column_title(2, "Lun");
  list_->set_column_justification(2, GTK_JUSTIFY_CENTER);

  list_->set_column_title(3, "Vendor");
  list_->set_column_justification(3, GTK_JUSTIFY_LEFT);

  list_->set_column_title(4, "Model");
  list_->set_column_justification(4, GTK_JUSTIFY_LEFT);

  list_->set_column_title(5, "Status");
  list_->set_column_justification(5, GTK_JUSTIFY_LEFT);

  list_->column_titles_show();
  list_->column_titles_passive();

  list_->set_usize(0, 80);

  list_->select_row.connect(slot(this, &DeviceList::selection_changed_emit));


  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  // available device list
  Gtk::HBox *listHBox = new Gtk::HBox;
  Gtk::VBox *listVBox = new Gtk::VBox;

  hbox = new Gtk::HBox;

  hbox->pack_start(*list_, TRUE, TRUE);
  list_->show();

  Gtk::Adjustment *adjust = new Gtk::Adjustment(0.0, 0.0, 0.0);

  Gtk::VScrollbar *scrollBar = new Gtk::VScrollbar(*adjust);
  hbox->pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();

  list_->set_vadjustment(*adjust);

  listHBox->pack_start(*hbox, TRUE, TRUE, 5);
  hbox->show();
  listVBox->pack_start(*listHBox, TRUE, TRUE, 5);
  listHBox->show();

  switch (filterType_)
  {
    case CdDevice::CD_ROM:
                 set_label("Available Reader Devices");
                 list_->set_selection_mode(GTK_SELECTION_SINGLE);
                 break;
    case CdDevice::CD_R:
                 set_label("Available Recorder Devices");
                 list_->set_selection_mode(GTK_SELECTION_MULTIPLE);
                 break;
    case CdDevice::CD_RW:
                 set_label("Available Recorder (RW) Devices");
                 list_->set_selection_mode(GTK_SELECTION_MULTIPLE);
                 break;
  }

  add(*listVBox);
  listVBox->show();

  show();

}

DeviceList::~DeviceList()
{
  DeviceData *data;

  while (list_->rows().size() > 0) {
    data = (DeviceData*)list_->row(0).get_data();
    delete data;
    list_->rows().remove(list_->row(0));
  }

  delete list_;
  list_ = NULL;
}

Gtk::CList_Helpers::SelectionList DeviceList::selection()
{
  return list_->selection();
}

void DeviceList::appendTableEntry(CdDevice *dev)
{
  DeviceData *data;
  char buf[50];
  std::string idStr;
  std::string busStr;
  std::string lunStr;
  const gchar *rowStr[6];

  data = new DeviceData;
  data->bus = dev->bus();
  data->id = dev->id();
  data->lun = dev->lun();

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

  list_->rows().push_back(rowStr);
  list_->row(list_->rows().size() - 1).set_data(data);

  if (dev->status() == CdDevice::DEV_READY)
    list_->row(list_->rows().size() - 1).set_selectable(true);
  else
    list_->row(list_->rows().size() - 1).set_selectable(false);
}

void DeviceList::import()
{
  CdDevice *drun;
  DeviceData *data;
  unsigned int i;

  list_->freeze();

  while (list_->rows().size() > 0) {
    data = (DeviceData*)list_->row(0).get_data();
    delete data;
    list_->rows().remove(list_->row(0));
  }

  for (drun = CdDevice::first(); drun != NULL; drun = CdDevice::next(drun)) {
    switch (filterType_)
    {
      case CdDevice::CD_ROM:
                   if (drun->driverId() > 0 &&
	               (drun->deviceType() == CdDevice::CD_ROM ||
	                drun->deviceType() == CdDevice::CD_R ||
	                drun->deviceType() == CdDevice::CD_RW)) {
                     appendTableEntry(drun);
                   }
                   break;
      case CdDevice::CD_R:
                   if (drun->driverId() > 0 &&
	               (drun->deviceType() == CdDevice::CD_R ||
	                drun->deviceType() == CdDevice::CD_RW)) {
                     appendTableEntry(drun);
                   }
                   break;
      case CdDevice::CD_RW:
                   if (drun->driverId() > 0 &&
	               (drun->deviceType() == CdDevice::CD_RW)) {
                     appendTableEntry(drun);
                   }
                   break;
    }
  }

  list_->thaw();

  if (list_->rows().size() > 0) {
    list_->columns_autosize();
    list_->moveto(0, 0, 0.0, 0.0);

    // select first selectable device
    for (i = 0; i < list_->rows().size(); i++) {
      if (list_->row(i).get_selectable()) {
	list_->row(i).select();
	break;
      }
    }
  }
}

void DeviceList::importStatus()
{
  unsigned int i;
  DeviceData *data;
  CdDevice *dev;

  for (i = 0; i < list_->rows().size(); i++) {
    if ((data = (DeviceData*)list_->row(i).get_data()) != NULL &&
	(dev = CdDevice::find(data->bus, data->id, data->lun)) != NULL) {
      if (dev->status() == CdDevice::DEV_READY) {
	list_->row(i).set_selectable(true);
      }
      else {
	list_->row(i).unselect();
	list_->row(i).set_selectable(false);
      }

      list_->cell(i, 5).set_text(CdDevice::status2string(dev->status()));
    }
  }

  list_->columns_autosize();

}

void DeviceList::selectOne()
{
  if (list_->selection().empty()) {
    for (int i = 0; i < list_->get_rows(); i++)
    {
      list_->row(i).select();
      if (list_->selection().empty())
        break;
    }
  }
}

void DeviceList::selectOneBut(Gtk::CList_Helpers::SelectionList &targetSelection)
{
  if (list_->selection().empty()) {
    for (int i = 0; i < list_->get_rows(); i++)
    {
      bool targetSelected = false;
      DeviceList::DeviceData *sourceData =
          (DeviceList::DeviceData*)list_->row(i).get_data();

      for (int j = 0; j < targetSelection.size(); j++) {
        DeviceList::DeviceData *targetData =
            (DeviceList::DeviceData*)targetSelection[j].get_data();
    
        if (targetData == NULL)
          break;

        if ((targetData->bus == sourceData->bus)
         && (targetData->id == sourceData->id)
         && (targetData->lun == sourceData->lun))
        {
          targetSelected = true;
          break;
        }
      }

      if (!targetSelected)
      {
        list_->row(i).select();
        if (!list_->selection().empty())
          break;
      }
    }
  }

  // Try selectOne in case we don't selected any device.
  selectOne();
}

void DeviceList::selection_changed_emit(gint p0, gint p1, GdkEvent* p2)
{
//Emited when the selection ends.

// g_print("row select emited!!\n");

}
