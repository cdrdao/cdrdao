/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: xcdrdao.h,v $
 * Revision 1.11  2001/04/23 00:15:11  llanero
 * Dialogs will update title with project name + app name.
 *
 * Revision 1.10  2001/04/21 12:53:30  llanero
 * lots of bug fixes and clean up. Works now.
 *
 * Revision 1.9  2001/04/16 21:56:04  llanero
 * Fixes to ProjecChooser.
 * Visibility of Dockitems in Project and AudioCDProject.
 *
 * Revision 1.8  2001/03/02 01:46:32  llanero
 * more work on the new mdi.
 * killed MDIWINDOW
 * per *view* toolbars, ...
 *
 * Revision 1.7  2001/02/27 03:13:22  llanero
 * some work in the new MDI mode
 *
 * Revision 1.6  2000/11/05 12:24:41  andreasm
 * Improved handling of TocEdit views. Introduced a new class TocEditView that
 * holds all view data (displayed sample range, selected sample range,
 * selected tracks/index marks, sample marker). This class is passed now to
 * most of the update functions of the dialogs.
 *
 * Revision 1.5  2000/09/21 02:07:07  llanero
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
 * Revision 1.4  2000/05/17 21:15:55  llanero
 * Beginings of Record Generic Dialog
 *
 * Revision 1.3  2000/05/01 18:15:00  andreasm
 * Switch to gnome-config settings.
 * Adapted Message Box to Gnome look, unfortunately the Gnome::MessageBox is
 * not implemented in gnome--, yet.
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
 * Revision 1.1.1.1  2000/02/05 01:38:57  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1998/11/20 18:58:41  mueller
 * Initial revision
 *
 */

#ifndef __XCDRDAO_H__
#define __XCDRDAO_H__

extern class GCDMaster *gcdmaster;
extern class DeviceConfDialog *DEVICE_CONF_DIALOG;
extern class ProcessMonitor *PROCESS_MONITOR;
extern class ProgressDialogPool *PROGRESS_POOL;
extern class RecordGenericDialog *RECORD_GENERIC_DIALOG;

void blockProcessMonitorSignals();
void unblockProcessMonitorSignals();

#endif

