/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001 Andreas Mueller <mueller@daneb.ping.de>
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

#include "guiUpdate.h"

#include "xcdrdao.h"
#include "MDIWindow.h"
#include "TrackInfoDialog.h"
#include "TocInfoDialog.h"
#include "AddSilenceDialog.h"
#include "AddFileDialog.h"
#include "DeviceConfDialog.h"
#include "RecordGenericDialog.h"
#include "ProgressDialog.h"
#include "ProcessMonitor.h"
#include "CdDevice.h"
#include "TocEdit.h"

#include "util.h"
#include "GenericChild.h"

void guiUpdate(unsigned long level)
{
  TocEdit *tocEdit = NULL;

  if (MDI_WINDOW == 0)
    return;

  if (MDI_WINDOW->gtkobj()->children) {
    GenericChild *child = static_cast <GenericChild *>(MDI_WINDOW->get_active_child());

    tocEdit = child->tocEdit();

    level |= tocEdit->updateLevel();
    
    child->update(level);
  }


  MDI_WINDOW->update(level);

  if (DEVICE_CONF_DIALOG != NULL)
    DEVICE_CONF_DIALOG->update(level);

  if (RECORD_GENERIC_DIALOG != NULL)
    RECORD_GENERIC_DIALOG->update(level, tocEdit);

  if (PROGRESS_POOL != NULL)
    PROGRESS_POOL->update(level);
//    RECORD_PROGRESS_POOL->update(level, tocEdit);
}

int guiUpdatePeriodic()
{
  if (CdDevice::updateDeviceStatus())
    guiUpdate(UPD_CD_DEVICE_STATUS);

  return 1;
}
