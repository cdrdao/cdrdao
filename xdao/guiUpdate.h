/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: guiUpdate.h,v $
 * Revision 1.1.1.1  2000/02/05 01:38:55  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:28:40  mueller
 * Initial revision
 *
 */

#ifndef __GUIUPDATE_H__
#define __GUIUPDATE_H__

#define UPD_ALL              0xffffffff
#define UPD_TOC_DATA         0x00000001
#define UPD_TRACK_DATA       0x00000002
#define UPD_SAMPLES          0x00000004
#define UPD_TRACK_MARK_SEL   0x00000008
#define UPD_SAMPLE_SEL       0x00000010
#define UPD_SAMPLE_MARKER    0x00000020
#define UPD_EDITABLE_STATE   0x00000040
#define UPD_TOC_DIRTY        0x00000080
#define UPD_CD_DEVICES       0x00000100
#define UPD_CD_DEVICE_STATUS 0x00000200
#define UPD_PROGRESS_STATUS  0x00000400

extern void guiUpdate(unsigned long level = 0);
extern int guiUpdatePeriodic();

#endif
