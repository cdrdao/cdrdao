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

#include <gtkmm.h>

#include "config.h"

#include "xcdrdao.h"
#include "TocEdit.h"
#include "TrackInfoDialog.h"
#include "AddSilenceDialog.h"
#include "AddFileDialog.h"
#include "DeviceConfDialog.h"
#include "PreferencesDialog.h"
#include "ProgressDialog.h"
#include "guiUpdate.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"
#include "ProjectChooser.h"
#include "ConfigManager.h"

#include "gcdmaster.h"

#include "port.h"

DeviceConfDialog*   deviceConfDialog = NULL;
ProcessMonitor*     PROCESS_MONITOR = NULL;
ProgressDialogPool* PROGRESS_POOL = NULL;
PreferencesDialog*  preferencesDialog = NULL;
ConfigManager*      configManager = NULL;
Glib::RefPtr<Gtk::Application> app;

static int PROCESS_MONITOR_SIGNAL_BLOCKED = 0;

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

int main(int argc, char* argv[])
{
  app = Gtk::Application::create(argc, argv, "Gnome.CDMaster");

  // create GConf configuration manager
  configManager = new ConfigManager();

  // settings
  CdDevice::importSettings();

  // setup process monitor
  PROCESS_MONITOR = new ProcessMonitor;
  installSignalHandler(SIGCHLD, signalHandler);

  // setup periodic GUI updates
  Glib::signal_timeout().connect(sigc::ptr_fun(&guiUpdatePeriodic), 2000);

  installSignalHandler(SIGPIPE, SIG_IGN);

  // scan for SCSI devices
  CdDevice::scan();

  // this forces a CdDevice::updateDeviceStatus() so
  // when gcdmaster is first show we already have the device status
  guiUpdatePeriodic();

  deviceConfDialog = new DeviceConfDialog;
  PROGRESS_POOL = new ProgressDialogPool;

  Glib::RefPtr<Gtk::Builder> builder;
  try {
    builder = Gtk::Builder::create_from_file(CDRDAO_GLADEDIR "/Preferences.glade");
  } catch(std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    exit(1);
  }

  builder->get_widget_derived("PrefDialog", preferencesDialog);
  if (!preferencesDialog) {
      std::cerr << "Unable to create Preferences dialog from glade file\n" 
          CDRDAO_GLADEDIR
	  "/Preferences.glade" << std::endl;
      exit(1);
  }

  GCDMaster* gcdmaster = new GCDMaster();

  bool openChooser = true;

  while (argc > 1) {
    if (gcdmaster->openNewProject(argv[1]))
      openChooser = false; 

    argv++;
    argc--;
  }

  if (openChooser)
    gcdmaster->newChooserWindow();

  //Shows the window and returns when it is closed.
  int retval = app->run(*gcdmaster);

  // save settings
  CdDevice::exportSettings();
  delete configManager;

  return retval;
}
