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

#ifndef __MSF_H__
#define __MSF_H__

#include "Sample.h"

#ifdef min
/* I stupidly named a member function 'min' which clashes with the macro 'min'
 * on some operating systems. The macro is simply undefined here since it is
 * not used in this package anyway.
 */
#undef min
#endif

class Msf {
private:

  int min_; // minutes
  int sec_; // seconds 
  int frac_; // fractions (blocks)

  long lba_; // logical block address

  void lba2Msf();

public:
  Msf(int min, int sec, int frac);
  Msf(long lba);
  Msf();

  int min() const { return min_; }
  int sec() const { return sec_; }
  int frac() const { return frac_; }
  
  long lba() const { return lba_; }

  unsigned long samples() const { return lba_ * SAMPLES_PER_BLOCK; }

  const char *str() const;

};

Msf operator+(const Msf &m1, const Msf &m2);
Msf operator-(const Msf &m1, const Msf &m2);

#endif
