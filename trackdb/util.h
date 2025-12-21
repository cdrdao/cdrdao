/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2022  Andreas Mueller <andreas@daneb.de>,
 *                           Denis Leroy <denis@poolshark.org>
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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

class Sample;

char *strdupCC(const char *s);
char *strdup3CC(const char *s1, const char *s2, const char *s3);
char *strdupvCC(const char *s1, ...);

long fullRead(int fd, void *buf, long count);
long fullWrite(int fd, const void *buf, long count);
long readLong(FILE *fp);
short readShort(FILE *fp);

void swapSamples(Sample *buf, unsigned long len);

unsigned char int2bcd(int);
int bcd2int(unsigned char);

const char *stripCwd(const char *fname);

bool resolveFilename(std::string &dest, const char *file, const char *path);

namespace Util
{

enum class FileExtension {
    UNKNOWN = 0,
    TOC,
    CUE,
    WAV,
    MP3,
    OGG,
    M3U,
    FLAC
};

FileExtension fileExtension(const char *fname);

enum class Encoding {
    UNSET,
    LATIN,
    ASCII,
    MSJIS,
    KOREAN,
    MANDARIN,
    UTF8
};

bool from_utf8(const std::string &input, std::vector<u8> &output, Encoding enc);
std::string to_utf8(const u8 *input, size_t input_size, Encoding enc);

Encoding characterCodeToEncoding(u8);
const char *encodingToString(Encoding);

bool isStrictAscii(const char *ptr);
bool isValidUTF8(const char *ptr);
bool processMixedString(std::string &str, bool &is_utf8);

} // namespace Util

struct PrintParams {
    PrintParams() : conversions(false), to_file(false), no_utf8(false)
    {
    }
    bool conversions;
    bool to_file;
    bool no_utf8;
};

#endif
