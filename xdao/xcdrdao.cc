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

#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <stdlib.h>

#include <gnome.h>

#include <gtk--.h>
#include <gtk/gtk.h>

#include <gnome--.h>

#include "config.h"

#include "xcdrdao.h"
#include "TocEdit.h"
#include "TrackInfoDialog.h"
#include "TocInfoDialog.h"
#include "AddSilenceDialog.h"
#include "AddFileDialog.h"
#include "DeviceConfDialog.h"
#include "ProgressDialog.h"
#include "guiUpdate.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"
#include "ProjectChooser.h"

#include "gcdmaster.h"

#include "port.h"

GCDMaster *gcdmaster = NULL;
DeviceConfDialog *DEVICE_CONF_DIALOG = NULL;
ProcessMonitor *PROCESS_MONITOR = NULL;
ProgressDialogPool *PROGRESS_POOL = NULL;

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
  Gnome::Main application("GnomeCDMaster", VERSION, argc, argv);
   
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

  // this forces a CdDevice::updateDeviceStatus() so
  // when gcdmaster is first show we already have the device status
  guiUpdatePeriodic();

  DEVICE_CONF_DIALOG = new DeviceConfDialog;
  PROGRESS_POOL = new ProgressDialogPool;

  gcdmaster = new GCDMaster;

  if (argc == 1)
    gcdmaster->newChooserWindow();
  else while (argc > 1)
  {
    if(!gcdmaster->openNewProject(argv[1]))
    {
      std::string message("Error loading ");
      message += argv[1];
      Gnome::Dialogs::error(*(gcdmaster->newChooserWindow2()), message);
    }
    argv++;
    argc--;
  }

  application.run();

  // save settings
  CdDevice::exportSettings();
  gnome_config_sync();

  return 0;
}
