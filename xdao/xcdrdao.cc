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
#include "ExtractDialog.h"
#include "ExtractProgressDialog.h"
#include "RecordDialog.h"
#include "RecordProgressDialog.h"
#include "guiUpdate.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"
#include "Settings.h"

#include "port.h"

MDIWindow *MDI_WINDOW = NULL;
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


//llanero int main (int argc, char **argv)
int main (int argc, char* argv[])
{
  const char *s;
  string settingsPath;

  Gnome::Main application("StillNoName", "0.0", argc, argv);
   
  Gtk::ButtonBox::set_child_size_default(50, 10);

//not needed by now...
  //glade_gnome_init ();

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
  application.timeout.connect(SigC::slot(&guiUpdatePeriodic), 2000);


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
  
  MDI_WINDOW = new MDIWindow(tocEdit);
//FIXME: MDI STUFF  MDI_WINDOW->open_toplevel();

  MDI_WINDOW->show();

  guiUpdate();

{
/*
  GtkWidget *message_box;
  GtkWidget *boton;
  message_box = gnome_message_box_new("This is a test!",
    GNOME_MESSAGE_BOX_WARNING, GNOME_STOCK_BUTTON_CLOSE, NULL);
  gtk_widget_show(message_box);
  boton = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(message_box), boton);
  gtk_widget_show(boton);
*/    
}
	
  application.run();

  s = settingsPath.c_str();

  if (s != NULL && *s != 0) 
    SETTINGS->write(s);

  return 0;
}

