/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999-2001  Andreas Mueller <andreas@daneb.de>
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

#ifndef __SONY_CDU948_H__
#define __SONY_CDU948_H__

#include "SonyCDU920.h"

class Toc;
class Track;

class CdTextEncoder;

class SonyCDU948 : public SonyCDU920 {
public:
  SonyCDU948(ScsiIf *scsiIf, unsigned long options);
  ~SonyCDU948();
  static CdrDriver *instance(ScsiIf *scsiIf, unsigned long options);

  int checkToc(const Toc *);

  int multiSession(int);
  int speed(int);

  int initDao(const Toc *);
  int startDao();

protected:
  CdTextEncoder *cdTextEncoder_;

  int selectSpeed();
  int setWriteParameters();

  int writeCdTextLeadIn();

};

#endif
