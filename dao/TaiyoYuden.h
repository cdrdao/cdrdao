/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

protected:
  DiskInfo diskInfo_;

private:
  long leadInLength_;
  long leadOutLength_;
};

#endif
