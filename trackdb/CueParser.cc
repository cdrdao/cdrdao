/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2005 Denis Leroy <denis@poolshark.org>
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

#include <iostream>
#include <sstream>
#include <stdio.h>

#include "Cue2Toc.h"
#include "Toc.h"

extern Toc *parseToc(const char* , const char *);

Toc *parseCue(FILE *fp, const char *filename)
{
    struct cuesheet* cue = read_cue(filename, NULL);

    std::ostringstream oss(std::ostringstream::out);
    write_toc(oss, cue, true);

    std::string ossstr = oss.str();
    const char* content = ossstr.c_str();

    return parseToc(content, filename);
}
