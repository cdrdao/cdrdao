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
 * $Log: xcdrdao.cc,v $
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

#include <gtk--.h>
#include <gtk/gtk.h>

#include "config.h"

#include "xcdrdao.h"
#include "TocEdit.h"
#include "MainWindow.h"
#include "TrackInfoDialog.h"
#include "TocInfoDialog.h"
#include "AddSilenceDialog.h"
#include "AddFileDialog.h"
#include "DeviceConfDialog.h"
#include "ExtractDialog.h"
#include "ExtractProgressDialog.h"
#include "RecordDialog.h"
#include "RecordProgressDialog.h"
#include "guiUpdate.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"
#include "Settings.h"

#include "port.h"

MainWindow *MAIN_WINDOW = NULL;
TrackInfoDialog *TRACK_INFO_DIALOG = NULL;
TocInfoDialog *TOC_INFO_DIALOG = NULL;
AddSilenceDialog *ADD_SILENCE_DIALOG = NULL;
AddFileDialog *ADD_FILE_DIALOG = NULL;
DeviceConfDialog *DEVICE_CONF_DIALOG = NULL;
ExtractDialog *EXTRACT_DIALOG = NULL;
RecordDialog *RECORD_DIALOG = NULL;
ProcessMonitor *PROCESS_MONITOR = NULL;
RecordProgressDialogPool *RECORD_PROGRESS_POOL = NULL;
ExtractProgressDialogPool *EXTRACT_PROGRESS_POOL = NULL;
Settings *SETTINGS = NULL;

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
      fprintf(stderr, "Warning: ");
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


int main (int argc, char **argv)
{
  const char *s;
  string settingsPath;
  Gtk_Main application(&argc, &argv);

  Gtk_ButtonBox::set_child_size_default(50, 10);

  // settings
  SETTINGS = new Settings;

  if ((s = getenv("HOME")) != NULL) {
    settingsPath = s;
    settingsPath += "/.xcdrdao";

    SETTINGS->read(settingsPath.c_str());
  }

  // setup process monitor
  PROCESS_MONITOR = new ProcessMonitor;
  installSignalHandler(SIGCHLD, signalHandler);

  // setup periodic GUI updates
  connect_to_function(application.timeout(2000), &guiUpdatePeriodic);


  installSignalHandler(SIGPIPE, SIG_IGN);

  // scan for SCSI devices
  CdDevice::scan();

  TRACK_INFO_DIALOG = new TrackInfoDialog;
  TOC_INFO_DIALOG = new TocInfoDialog;
  ADD_SILENCE_DIALOG = new AddSilenceDialog;
  ADD_FILE_DIALOG = new AddFileDialog;
  DEVICE_CONF_DIALOG = new DeviceConfDialog;
  EXTRACT_DIALOG = new ExtractDialog;
  EXTRACT_PROGRESS_POOL = new ExtractProgressDialogPool;
  RECORD_DIALOG = new RecordDialog;
  RECORD_PROGRESS_POOL = new RecordProgressDialogPool;

  // create TocEdit object
  TocEdit *tocEdit = new TocEdit(NULL, NULL);

  if (argc > 1) {
    if (tocEdit->readToc(argv[1]) != 0)
      exit(1);
  }
  
  MAIN_WINDOW = new MainWindow(tocEdit);
  MAIN_WINDOW->show();

  guiUpdate();

  application.run();

  s = settingsPath.c_str();

  if (s != NULL && *s != 0) 
    SETTINGS->write(s);

  return 0;
}
