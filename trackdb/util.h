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
 * $Log: util.h,v $
 * Revision 1.1.1.1  2000/02/05 01:32:34  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.4  1999/03/27 19:51:04  mueller
 * Added 'strdup3CC()'.
 *
 * Revision 1.3  1999/01/24 15:59:02  mueller
 * Moved 'audioDataLength()', 'audioFileType()' and 'waveHeaderLength()'
 * to class 'AudioData'.
 *
 * Revision 1.2  1998/09/06 12:00:26  mueller
 * Used 'message' function for messages.
 *
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>

class Sample;

char *strdupCC(const char *s);
char *strdup3CC(const char *s1, const char *s2, const char *s3);

long fullRead(int fd, void *buf, long count);
long fullWrite(int fd, const void *buf, long count);
long readLong(FILE *fp);
short readShort(FILE *fp);

void swapSamples(Sample *buf, unsigned long len);

unsigned char int2bcd(int);
int bcd2int(unsigned char);

const char *stripCwd(const char *fname);

void message(int level, const char *fmt, ...);


#endif

