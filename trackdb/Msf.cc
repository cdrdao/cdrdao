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

#include <config.h>

#include <stdio.h>
#include <assert.h>

#include "Msf.h"

Msf::Msf()
{
  min_ = sec_ = frac_ = 0;
  lba_ = 0;
}

Msf::Msf(int min, int sec, int frac)
{
  // assert(frac >= 0 && frac < 75);
  // assert(sec >= 0 && sec < 60);
  // assert(min >= 0);

  min_ = min;
  sec_ = sec;
  frac_ = frac;

  lba_ = min_ * 4500 + sec_ * 75 + frac_;
}

Msf::Msf(long lba)
{
  if (lba < 0)
    lba = 0;

  lba_ = lba;
  lba2Msf();
}

void Msf::lba2Msf()
{
  long lba = lba_;

  min_ = lba / 4500;
  lba %= 4500;

  sec_ = lba / 75;
  lba %= 75;

  frac_ = lba;
}

const char *Msf::str() const
{
  static char buf[20];

  sprintf(buf, "%02d:%02d:%02d", min_, sec_, frac_);

  return buf;
}

Msf operator+(const Msf &m1, const Msf &m2)
{
  return Msf(m1.lba() + m2.lba());
}

Msf operator-(const Msf &m1, const Msf &m2)
{
  return Msf(m1.lba() - m2.lba());
}

