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
 * $Log: RicohMP6200.h,v $
 * Revision 1.1  2000/02/05 01:35:11  llanero
 * Initial revision
 *
 * Revision 1.4  1999/04/05 11:04:10  mueller
 * Added driver option flags.
 *
 * Revision 1.3  1999/03/27 20:50:24  mueller
 * Adapted to changed writing interface.
 * Fixed problems with the ATAPI version of these drives.
 *
 * Revision 1.2  1999/01/24 17:08:38  mueller
 * Tried different load/unload SCSI command for ejecting the disk.
 *
 * Revision 1.1  1998/10/03 15:04:37  mueller
 * Initial revision
 *
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

  int writeData(TrackData::Mode, long &lba, const char *buf, long len);

  int loadUnload(int unload) const;

protected:
  int setWriteParameters();
};

#endif
