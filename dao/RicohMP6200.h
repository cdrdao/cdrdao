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

/* Driver for the Ricoh MP6200 drive. It's mainly SCSI-3/mmc compatible but
 * disk-at-once writing is done with the Philips CDD2x00 commands.
 */

#ifndef __RICOH_MP6200_H__
#define __RICOH_MP6200_H__

#include "GenericMMC.h"
#include "CDD2600Base.h"

class RicohMP6200 : public GenericMMC, private CDD2600Base {
public:
  RicohMP6200(ScsiIf *scsiIf, unsigned long options);
  ~RicohMP6200();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  int initDao(const Toc *);
  int startDao();
  int finishDao();
  void abortDao();

  int writeData(TrackData::Mode, TrackData::SubChannelMode, long &lba,
		const char *buf, long len);

  int loadUnload(int unload) const;

protected:
  int setWriteParameters();
};

#endif
