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
 * Revision 1.1  2000/02/05 01:38:57  llanero
 * Initial revision
 *
 * Revision 1.1  1998/11/20 18:58:41  mueller
 * Initial revision
 *
 */

#ifndef __XCDRDAO_H__
#define __XCDRDAO_H__

extern class MainWindow *MAIN_WINDOW;
extern class TrackInfoDialog *TRACK_INFO_DIALOG;
extern class TocInfoDialog *TOC_INFO_DIALOG;
extern class AddSilenceDialog *ADD_SILENCE_DIALOG;
extern class AddFileDialog *ADD_FILE_DIALOG;
extern class DeviceConfDialog *DEVICE_CONF_DIALOG;
extern class ExtractDialog *EXTRACT_DIALOG;
extern class ExtractProgressDialogPool *EXTRACT_PROGRESS_POOL;
extern class RecordDialog *RECORD_DIALOG;
extern class ProcessMonitor *PROCESS_MONITOR;
extern class RecordProgressDialogPool *RECORD_PROGRESS_POOL;
extern class Settings *SETTINGS;

void blockProcessMonitorSignals();
void unblockProcessMonitorSignals();

#endif
