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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "log.h"

static struct {
    int level;
} self;

void log_init()
{
    memset(&self, 0, sizeof(self));
}

// Set verbosity of the logging code.
void log_set_verbose(int level)
{
    self.level = level;
}

// Log a message.
void log_message(int level, const char *fmt, ...)
{
    long len = strlen(fmt);
    char last = len > 0 ? fmt[len - 1] : 0;

    va_list args;
    va_start(args, fmt);

    if (level < 0) {
	switch (level) {
	case -1:
	    fprintf(stderr, "WARNING: ");
	    break;
	case -2:
	    fprintf(stderr, "ERROR: ");
	    break;
	case -3:
	    fprintf(stderr, "INTERNAL ERROR: ");
	    break;
	default:
	    fprintf(stderr, "FATAL ERROR: ");
	    break;
	}
	vfprintf(stderr, fmt, args);
	if (last != ' ' && last != '\r')
	    fprintf(stderr, "\n");
    
	fflush(stderr);
	if (level <= -10)
	    exit(1);
    }  else if (level <= self.level) {
	vfprintf(stderr, fmt, args);
	if (last != ' ' && last != '\r')
	    fprintf(stderr, "\n");

	fflush(stderr);
    }

    va_end(args);
}

