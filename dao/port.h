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
 * $Log: port.h,v $
 * Revision 1.2  2000/11/12 16:50:44  andreasm
 * Fixes for compilation under Win32.
 *
 * Revision 1.1.1.1  2000/02/05 01:35:20  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/05/11 20:03:29  mueller
 * Initial revision
 *
 */

#ifndef __PORT_H__
#define __PORT_H__

#include "config.h"

typedef RETSIGTYPE (*SignalHandler)(int);

void mSleep(long milliSeconds);
void installSignalHandler(int sig, SignalHandler);
void blockSignal(int sig);
void unblockSignal(int sig);
int setRealTimeScheduling(int priority);

#endif
