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
 * $Log: CDD2600Base.h,v $
 * Revision 1.1  2000/02/05 01:34:48  llanero
 * Initial revision
 *
 * Revision 1.2  1999/03/27 20:52:02  mueller
 * Adapted to changed writing interface.
 *
 * Revision 1.1  1998/10/03 15:03:59  mueller
 * Initial revision
 *
 */

/* Basic disk-at-once writing methods for CDD2x00 writing interface. */

#ifndef __CDD2600_BASE_H__
#define __CDD2600_BASE_H__

#include "CdrDriver.h"

class Toc;
class Track;

class CDD2600Base {
public:
  CDD2600Base(CdrDriver *);
  ~CDD2600Base();

protected:
  int modeSelectBlockSize(int blockSize, int showMsg);
  int modeSelectSpeed(int readSpeed, int writeSpeed, int simulate,
		      int showMessage);
  int modeSelectCatalog(const Toc *);

  int readSessionInfo(long *, long *, int showMessage);
  int writeSession(const Toc *, int multiSession);

private:
  CdrDriver *driver_; // driver for sending SCSI commands
};

#endif
