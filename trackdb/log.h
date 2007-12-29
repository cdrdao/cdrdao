/*  cdrdao - logging/message code
 *
 *  Copyright (C) 2007 Denis Leroy <denis@poolshark.org>
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

#ifndef __LOG_H__
#define __LOG_H__

// Initialize log code. Must be called first.
void log_init();

// Log given formatted message. 
//
// Level definitions:
//   -1 : warning
//   -2 : error
//   -3 : internal error
//   -4 to -9 : fatal error
//   -10 and less : fatal error, call will exit the calling thread.
//
// Levels >= 0 are controlled by the verbosity set below.
void log_message(int level, const char *fmt, ...);

// Set logging verbosity. Only levels <= set level will be output.
void log_set_verbose(int level);

#endif

