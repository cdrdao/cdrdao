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

#include <iconv.h>

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Sample.h"
#include "log.h"
#include "util.h"

using namespace std;

char *strdupCC(const char *s)
{
    char *ret;
    long len;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s);

    ret = new char[len + 1];

    strcpy(ret, s);

    return ret;
}

char *strdup3CC(const char *s1, const char *s2, const char *s3)
{
    char *ret;
    long len = 0;

    if (s1 == NULL && s2 == NULL && s3 == NULL)
        return NULL;

    if (s1 != NULL)
        len = strlen(s1);

    if (s2 != NULL)
        len += strlen(s2);

    if (s3 != NULL)
        len += strlen(s3);

    ret = new char[len + 1];

    *ret = 0;

    if (s1 != NULL)
        strcpy(ret, s1);

    if (s2 != NULL)
        strcat(ret, s2);

    if (s3 != NULL)
        strcat(ret, s3);

    return ret;
}

char *strdupvCC(const char *s1, ...)
{
    const char *p;
    char *ret;
    long len;
    va_list ap;

    if (s1 == NULL)
        return NULL;

    len = strlen(s1);

    va_start(ap, s1);

    while ((p = va_arg(ap, const char *)) != NULL)
        len += strlen(p);

    va_end(ap);

    ret = new char[len + 1];

    strcpy(ret, s1);

    va_start(ap, s1);

    while ((p = va_arg(ap, const char *)) != NULL)
        strcat(ret, p);

    va_end(ap);

    return ret;
}

long fullRead(int fd, void *buf, long count)
{
    long n = 0;
    long nread = 0;

    do {
        do {
            n = read(fd, (char *)buf + nread, count);
        } while (n < 0 && (errno == EAGAIN || errno == EINTR));

        if (n < 0) {
            return -1;
        }

        if (n == 0) {
            return nread;
        }

        count -= n;
        nread += n;
    } while (count > 0);

    return nread;
}

long fullWrite(int fd, const void *buf, long count)
{
    long n;
    long nwritten = 0;
    const char *p = (const char *)buf;

    do {
        do {
            n = write(fd, p, count);
        } while (n < 0 && (errno == EAGAIN || errno == EINTR));

        if (n < 0)
            return -1;

        if (n == 0)
            return nwritten;

        count -= n;
        nwritten += n;
        p += n;
    } while (count > 0);

    return nwritten;
}

long readLong(FILE *fp)
{
    unsigned char c1 = getc(fp);
    unsigned char c2 = getc(fp);
    unsigned char c3 = getc(fp);
    unsigned char c4 = getc(fp);

    return ((long)c4 << 24) | ((long)c3 << 16) | ((long)c2 << 8) | (long)c1;
}

short readShort(FILE *fp)
{
    unsigned char c1 = getc(fp);
    unsigned char c2 = getc(fp);

    return ((short)c2 << 8) | (short)c1;
}

void swapSamples(Sample *buf, unsigned long len)
{
    unsigned long i;

    for (i = 0; i < len; i++) {
        buf[i].swap();
    }
}

unsigned char int2bcd(int d)
{
    if (d >= 0 && d <= 99)
        return ((d / 10) << 4) | (d % 10);
    else
        return d;
}

int bcd2int(unsigned char d)
{
    unsigned char d1 = d & 0x0f;
    unsigned char d2 = d >> 4;

    if (d1 <= 9 && d2 <= 9) {
        return d2 * 10 + d1;
    } else {
        return d;
    }
}

const char *stripCwd(const char *fname)
{
    static char *buf = NULL;
    static long bufLen = 0;

    char cwd[PATH_MAX + 1];
    long len;

    if (fname == NULL)
        return NULL;

    len = strlen(fname);

    if (buf == NULL || len >= bufLen) {
        bufLen = len + 1;
        delete[] buf;
        buf = new char[bufLen];
    }

    if (getcwd(cwd, PATH_MAX + 1) == NULL) {
        // if we cannot retrieve the current working directory return 'fname'
        strcpy(buf, fname);
    } else {
        len = strlen(cwd);

        if (strncmp(cwd, fname, len) == 0) {
            if (*(fname + len) == '/')
                strcpy(buf, fname + len + 1);
            else
                strcpy(buf, fname + len);

            if (buf[0] == 0) {
                // resulting filename would be "" -> return 'fname'
                strcpy(buf, fname);
            }
        } else {
            strcpy(buf, fname);
        }
    }

    return buf;
}

namespace Util
{

FileExtension fileExtension(const char *fname)
{
    const char *e;

    if (fname && (e = strrchr(fname, '.'))) {
        e++;

        if (strcasecmp(e, "toc") == 0)
            return FileExtension::TOC;
        if (strcasecmp(e, "cue") == 0)
            return FileExtension::CUE;
        if (strcasecmp(e, "wav") == 0)
            return FileExtension::WAV;
        if (strcasecmp(e, "mp3") == 0)
            return FileExtension::MP3;
        if (strcasecmp(e, "ogg") == 0)
            return FileExtension::OGG;
        if (strcasecmp(e, "m3u") == 0)
            return FileExtension::M3U;
        if (strcasecmp(e, "flac") == 0)
            return FileExtension::FLAC;
    }

    return FileExtension::UNKNOWN;
}

string to_utf8(const u8 *input, size_t input_size, Util::Encoding enc)
{
    const char *from_encoding = "ISO-8859-1";
    if (enc == Util::Encoding::MSJIS)
        from_encoding = "CP932"; // Code Page 932, aka MS-JIS

    // Have to jump through hoops due to silly const iconv nonsense.
    char *abuffer = (char *)alloca(input_size + 1);
    memcpy(abuffer, input, input_size);
    abuffer[input_size] = 0; // Should not be necessary in theory
    ICONV_CONST char *src = (ICONV_CONST char *)abuffer;

    size_t srclen = input_size;
    size_t dstlen = input_size * 4;
    char *dst = (char *)alloca(dstlen);
    char *orig_dst = dst;
    auto icv = iconv_open("UTF-8", from_encoding);
    if (!icv)
        return string((char *)input);
    if (iconv(icv, &src, &srclen, &dst, &dstlen) == (size_t)-1) {
        log_message(-1, strerror(errno));
        return string((char *)input);
    }
    *dst = 0;
    return string(orig_dst);
}

bool from_utf8(const string &input, std::vector<u8> &output, Encoding enc)
{
    const char *to_encoding;
    switch (enc) {
    case Encoding::ASCII:
        to_encoding = "ASCII";
        break;
    case Encoding::MSJIS:
        to_encoding = "CP932";
        break;
    default:
        to_encoding = "ISO-8859-1";
    }

    char *abuffer = (char *)alloca(input.size() + 1);
    strcpy(abuffer, input.c_str());
    ICONV_CONST char *src = (ICONV_CONST char *)abuffer;

    size_t srclen = input.size();
    size_t dstlen = srclen * 4;
    char *dst = (char *)alloca(dstlen);
    char *origdst = dst;
    auto icv = iconv_open(to_encoding, "UTF-8");
    if (!icv)
        return false;
    if (iconv(icv, &src, &srclen, &dst, &dstlen) == (size_t)-1)
        return false;

    while (origdst < dst)
        output.push_back(*origdst++);
    output.push_back(0);
    return true;
}

Encoding characterCodeToEncoding(u8 code)
{
    switch (code) {
    case 0x01:
        return Encoding::ASCII;
    case 0x80:
        return Encoding::MSJIS;
    case 0x81:
        return Encoding::KOREAN;
    case 0x82:
        return Encoding::MANDARIN;
    default:
        return Encoding::LATIN;
    }
}

const char *encodingToString(Encoding e)
{
    switch (e) {
    case Encoding::ASCII:
        return "ENCODING_ASCII";
    case Encoding::MSJIS:
        return "ENCODING_MS_JIS";
    case Encoding::KOREAN:
        return "ENCODING_KOREAN";
    case Encoding::MANDARIN:
        return "ENCODING_MANDARIN";
    default:
        return "ENCODING_ISO_8859_1";
    }
}

bool isStrictAscii(const std::string &str)
{
    for (const auto c : str) {
        if (((u8)(c)) > 127)
            return false;
    }
    return true;
}

bool isValidUTF8(const char *str)
{
    const char *encoding = "UTF-8";

    char *abuffer = (char *)alloca(strlen(str) + 1);
    strcpy(abuffer, str);
    ICONV_CONST char *src = (ICONV_CONST char *)abuffer;

    size_t srclen = strlen(src);
    size_t dstlen = srclen * 2;
    char *dst = (char *)alloca(dstlen);
    auto icv = iconv_open(encoding, encoding);
    if (!icv)
        return true;
    if (iconv(icv, &src, &srclen, &dst, &dstlen) == (size_t)-1)
        return false;
    else
        return true;
}

bool processMixedString(std::string &str, bool &is_utf8)
{
    is_utf8 = !isStrictAscii(str);
    for (size_t i = 0; str.size() >= 4 && i < str.size() - 3; i++) {
        if (str[i] == '\\' && isdigit(str[i + 1]) && isdigit(str[i + 2]) && isdigit(str[i + 3])) {

            if (is_utf8)
                return false;

            std::string singlechar(1, 0);
            singlechar[0] = strtol(str.substr(i + 1, 3).c_str(), NULL, 8);
            str = str.replace(i, 4, singlechar);
        }
    }
    return true;
}

} // namespace Util

bool resolveFilename(std::string &abs, const char *file, const char *path)
{
    struct stat st;

    // First checks if file is already absolute, in which case we do
    // nothing.
    if (file[0] == '/') {
        abs = file;
        return true;
    }

    // Now checks if file is readable in current directory. Current
    // directory has precedence over search path.
    if (stat(file, &st) == 0 && (st.st_mode & S_IFREG)) {
        char cwd[1024];
        if (getcwd(cwd, 1024)) {
            abs = cwd;
            abs += "/";
        }
        abs += file;
        return true;
    }

    // Now check in search path.
    std::string afile = path;
    if (*(afile.end()) != '/')
        afile += "/";
    afile += file;
    if (stat(afile.c_str(), &st) == 0 && (st.st_mode & S_IFREG)) {
        abs = afile;
        return true;
    }

    // File not found.
    abs = "";
    return false;
}
