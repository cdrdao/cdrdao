/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2026 Denis Leroy <denis@poolshark.org>
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

#ifndef __TOC_PARSER_H__
#define __TOC_PARSER_H__

#include <stdio.h>

class Toc;

// Parse a TOC file. 'filename' is only used for error messages. Returns
// a newly allocated Toc, or NULL if any error was found (errors are
// reported through log_message()).
Toc *parseToc(FILE *fp, const char *filename);
Toc *parseToc(const char *input, const char *filename);

#endif
