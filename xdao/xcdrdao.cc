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
/*
 * $Log: xcdrdao.cc,v $
 * Revision 1.15  2000/09/21 02:07:07  llanero
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
 * Revision 1.14  2000/07/31 01:55:49  llanero
 * got rid of old Extract dialog and Record dialog.
 * both are using RecordProgressDialog now.
 *
 * Revision 1.13  2000/07/30 02:41:03  llanero
 * started CD to CD copy. Still not functional.
 *
 * Revision 1.12  2000/07/17 22:08:33  llanero
 * DeviceList is now a class
 * RecordGenericDialog and RecordCDTarget first implemented.
 *
 * Revision 1.11  2000/06/10 14:49:49  andreasm
 * Changed Warning to WARNING in 'message()'.
 *
 * Revision 1.10  2000/05/25 20:12:55  llanero
 * added BUGS and TASKS, changed name to GnomeCDMaster
 *
 * Revision 1.9  2000/05/17 21:15:55  llanero
 * Beginings of Record Generic Dialog
 *
 * Revision 1.8  2000/05/01 18:15:00  andreasm
 * Switch to gnome-config settings.
 * Adapted Message Box to Gnome look, unfortunately the Gnome::MessageBox is
 * not implemented in gnome--, yet.
 *
 * Revision 1.7  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.6  2000/04/14 13:22:02  llanero
 * changed the MDI object to GnomeApp until gnome-- MDI is done.
 * Also catched a bug in SampleDisplay.cc:1000.
 *
 * Revision 1.5  2000/03/05 22:25:52  llanero
 * more code translated to gtk-- 1.1.8
 *
 * Revision 1.4  2000/03/04 01:28:52  llanero
 * SampleDisplay.{cc,h} are fixed now = gtk 1.1.8 compliant.
 *
 * Revision 1.3  2000/02/28 23:29:55  llanero
 * fixed Makefile.in to include glade-gnome
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:40:33  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1998/11/20 18:53:45  mueller
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>

#include <gnome.h>

#include <gtk--.h>
#include <gtk/gtk.h>

#include <glade/glade.h>
#include <gnome--.h>

#include "config.h"

#include "xcdrdao.h"
#include "MDIWindow.h"
#include "TocEdit.h"
#include "MainWindow.h"
#include "TrackInfoDialog.h"
#include "TocInfoDialog.h"
#include "AddSilenceDialog.h"
#include "AddFileDialog.h"
#include "DeviceConfDialog.h"
#include "RecordProgressDialog.h"
#include "RecordGenericDialog.h"
#include "guiUpdate.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"

#include "port.h"

MDIWindow *MDI_WINDOW = NULL;
DeviceConfDialog *DEVICE_CONF_DIALOG = NULL;
ProcessMonitor *PROCESS_MONITOR = NULL;
RecordProgressDialogPool *RECORD_PROGRESS_POOL = NULL;
RecordGenericDialog *RECORD_GENERIC_DIALOG = NULL;

static int VERBOSE = 0;
static int PROCESS_MONITOR_SIGNAL_BLOCKED = 0;

void message(int level, const char *fmt, ...)
{
  long len = strlen(fmt);
  char last = len > 0 ? fmt[len - 1] : 0;

  va_list args;
  va_start(args, fmt);

  if (level < 0) {
    switch (level) {
    case -1:
      fprintf(stderr, "WARNING: ");
      break;
    case -2:
      fprintf(stderr, "ERROR: ");
      break;
    case -3:
      fprintf(stderr, "INTERNAL ERROR: ");
      break;
    default:
      fprintf(stderr, "FATAL ERROR: ");
      break;
    }
    vfprintf(stderr, fmt, args);
    if (last != ' ' && last != '\r')
      fprintf(stderr, "\n");
    
    fflush(stderr);
    if (level <= -10)
      exit(-1);
  }
  else if (level <= VERBOSE) {
    vfprintf(stdout, fmt, args);
    if (last != ' ' && last != '\r')
      fprintf(stdout, "\n");

    fflush(stdout);
  }

  va_end(args);
}

void blockProcessMonitorSignals()
{
  if (PROCESS_MONITOR_SIGNAL_BLOCKED == 0)
    blockSignal(SIGCHLD);

  PROCESS_MONITOR_SIGNAL_BLOCKED++;
}

void unblockProcessMonitorSignals()
{
  if (PROCESS_MONITOR_SIGNAL_BLOCKED > 0) {
    PROCESS_MONITOR_SIGNAL_BLOCKED--;

    if (PROCESS_MONITOR_SIGNAL_BLOCKED == 0)
      unblockSignal(SIGCHLD);
  }
}

static RETSIGTYPE signalHandler(int sig)
{
  if (sig == SIGCHLD)
    PROCESS_MONITOR->handleSigChld();
}


int main (int argc, char* argv[])
{
  Gnome::Main application("GnomeCDMaster", "1.1.4a", argc, argv);
   
  Gtk::ButtonBox::set_child_size_default(50, 10);

  // settings
  CdDevice::importSettings();

  // setup process monitor
  PROCESS_MONITOR = new ProcessMonitor;
  installSignalHandler(SIGCHLD, signalHandler);

  // setup periodic GUI updates
  application.timeout.connect(SigC::slot(&guiUpdatePeriodic), 2000);

  installSignalHandler(SIGPIPE, SIG_IGN);

  // scan for SCSI devices
  CdDevice::scan();

  DEVICE_CONF_DIALOG = new DeviceConfDialog;
  RECORD_PROGRESS_POOL = new RecordProgressDialogPool;
  RECORD_GENERIC_DIALOG = new RecordGenericDialog;

  MDI_WINDOW = new MDIWindow();
  MDI_WINDOW->open_toplevel();

//FIXME: perhaps update only the MDI_WINDOW
  guiUpdate();

//  if (argc > 1) {
  while (argc > 1) {
    MDI_WINDOW->openAudioCDProject(argv[1]);
    argv++;
    argc--;
//    if (tocEdit->readToc(argv[1]) != 0)
//      exit(1);
  }

  application.run();

  // save settings
  CdDevice::exportSettings();
  gnome_config_sync();

  return 0;
}

