/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999 Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: guiUpdate.cc,v $
 * Revision 1.7  2000/11/05 12:24:41  andreasm
 * Improved handling of TocEdit views. Introduced a new class TocEditView that
 * holds all view data (displayed sample range, selected sample range,
 * selected tracks/index marks, sample marker). This class is passed now to
 * most of the update functions of the dialogs.
 *
 * Revision 1.6  2000/09/21 02:07:07  llanero
 * MDI support:
 * Splitted AudioCDChild into same and AudioCDView
 * Move Selections from TocEdit to AudioCDView to allow
 *   multiple selections.
 * Cursor animation in all the views.
 * Can load more than one from from command line
 * Track info, Toc info, Append/Insert Silence, Append/Insert Track,
 *   they all are built for every child when needed.
 * ...
 *
 * Revision 1.5  2000/07/31 01:55:49  llanero
 * got rid of old Extract dialog and Record dialog.
 * both are using RecordProgressDialog now.
 *
 * Revision 1.4  2000/07/17 22:08:33  llanero
 * DeviceList is now a class
 * RecordGenericDialog and RecordCDTarget first implemented.
 *
 * Revision 1.3  2000/04/24 12:49:06  andreasm
 * Changed handling or message from remote processes to use the
 * Gtk::Main::input mechanism.
 *
 * Revision 1.2  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.1.1.1  2000/02/05 01:40:28  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:28:40  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: guiUpdate.cc,v 1.7 2000/11/05 12:24:41 andreasm Exp $";

#include "guiUpdate.h"

#include "xcdrdao.h"
#include "TocEdit.h"
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

#include "util.h"
#include "GenericChild.h"

void guiUpdate(unsigned long level)
{
  if (MDI_WINDOW == 0)
    return;

  if (MDI_WINDOW->gtkobj()->children)
  {
    GenericChild *child = static_cast <GenericChild *>(MDI_WINDOW->get_active_child());

    level |= child->tocEdit()->updateLevel();
    
    child->update(level);
  }


  MDI_WINDOW->update(level);

  if (DEVICE_CONF_DIALOG != NULL)
    DEVICE_CONF_DIALOG->update(level);

  if (RECORD_GENERIC_DIALOG != NULL)
    RECORD_GENERIC_DIALOG->update(level);
//    RECORD_GENERIC_DIALOG->update(level, tocEdit);

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
