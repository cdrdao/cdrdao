/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999-2000  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: ToshibaReader.h,v $
 * Revision 1.1  2000/04/23 16:28:49  andreasm
 * Read CD driver for Toshiba CD-ROM drives.
 *
 */

#ifndef __TOSHIBA_READER_H__
#define __TOSHIBA_READER_H__

#include "PlextorReader.h"

class Toc;
class Track;

class ToshibaReader : public PlextorReader {
public:

  ToshibaReader(ScsiIf *scsiIf, unsigned long options);
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

protected:
  int readSubChannels(long lba, long len, SubChannel ***, Sample *);

};

#endif
