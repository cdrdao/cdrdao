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
 * $Log: dao.h,v $
 * Revision 1.2  2000/10/08 16:39:41  andreasm
 * Remote progress message now always contain the track relative and total
 * progress and the total number of processed tracks.
 *
 * Revision 1.1.1.1  2000/02/05 01:35:20  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.3  1999/01/24 16:03:57  mueller
 * Applied Radek Doulik's ring buffer patch. Added some cleanups and
 * improved behavior in error case.
 *
 * Revision 1.2  1998/08/15 20:47:16  mueller
 * Added support for GenericMMC driver.
 *
 */

#ifndef __DAO_H__
#define __DAO_H__

#include "Toc.h"
#include "CdrDriver.h"

int writeDiskAtOnce(const Toc *, CdrDriver *, int nofBuffers, int swap,
		    int testMode);

#endif
