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
 * $Log: TaiyoYuden.h,v $
 * Revision 1.1  2000/02/05 01:35:14  llanero
 * Initial revision
 *
 * Revision 1.4  1999/04/05 11:04:10  mueller
 * Added driver option flags.
 *
 * Revision 1.3  1999/03/27 20:50:04  mueller
 * Adapted to changed writing interface.
 *
 * Revision 1.2  1999/02/06 20:42:44  mueller
 * Added member function 'checkToc()'.
 *
 * Revision 1.1  1999/02/04 21:05:47  mueller
 * Initial revision
 *
 */

/* Driver for the TaiyoYuden drive created by Henk-Jan Slotboom.
 * Very similar to the Philips CDD2x00 drives.
 */

#ifndef __TAIYO_YUDEN_H__
#define __TAIYO_YUDEN_H__

#include "CdrDriver.h"
#include "PlextorReader.h"
#include "CDD2600Base.h"

class TaiyoYuden : public PlextorReader, private CDD2600Base {
public:
  TaiyoYuden(ScsiIf *scsiIf, unsigned long options);
  ~TaiyoYuden();

  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  // takes little endian samples
  int bigEndianSamples() const { return 0; }

  int speed(int);

  DiskInfo *diskInfo();

  int loadUnload(int) const;

  int checkToc(const Toc *);
  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  int writeData(TrackData::Mode, long &lba, const char *buf, long len);

protected:
  DiskInfo diskInfo_;

private:
  long leadInLength_;
  long leadOutLength_;
};

#endif
