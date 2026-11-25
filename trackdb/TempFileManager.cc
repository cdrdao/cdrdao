/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2004  Denis Leroy <denis@poolshark.org>
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

#include "TempFileManager.h"
#include "log.h"

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif

#define DEFAULT_TEMP_PATH "/tmp/"

TempFileManager::TempFileManager()
{
#ifdef _WIN32
    const char *tmp = getenv("TEMP");
    if (!tmp || !*tmp)
        tmp = getenv("TMP");
    if (!tmp || !*tmp)
        tmp = ".";
    path_ = tmp;
    if (path_.back() != '/' && path_.back() != '\\')
        path_ += '\\';
#else
    path_ = DEFAULT_TEMP_PATH;
#endif
    keepTemps_ = false;
    prefix_ = "cdrdao.";
}

TempFileManager::~TempFileManager()
{
    if (!keepTemps_) {
        std::map<std::string, std::string>::const_iterator i = map_.begin();

        for (; i != map_.end(); i++) {
            std::string tmpFile = (*i).second;
            log_message(3, "Removing temp file \"%s\"", tmpFile.c_str());
            unlink(tmpFile.c_str());
        }
    }
}

bool TempFileManager::setTempDirectory(const char *path)
{
    struct stat st;

    int ret = stat(path, &st);

    if (ret != 0) {
        log_message(-2, "Could not find temporary directory %s.", path);
        return false;
    }

    if (!S_ISDIR(st.st_mode) || access(path, W_OK) != 0) {
        log_message(-2, "No permission for temp directory %s.", path);
        return false;
    }

    path_ = path;

    if (!path_.empty() && path_.back() != '/' && path_.back() != '\\')
        path_ += '/';

    return true;
}

bool TempFileManager::getTempFile(std::string &tempname, const char *key, const char *extension)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!map_[key].empty()) {
        tempname = map_[key];
        return true;
    }

    const char *shortname = key;
    for (const char *p = key; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\' || *p == ':')
            shortname = p + 1;
    }

    std::string fname = path_;
    fname += prefix_;
    fname += shortname;

    int id = 1;
    int fd;
    do {
        char tmpbuf[12];
        std::string uniqnm = fname;
        snprintf(tmpbuf, sizeof(tmpbuf), ".%d", id);
        uniqnm += tmpbuf;
        if (extension) {
            uniqnm += ".";
            uniqnm += extension;
        }
        fd = open(uniqnm.c_str(), O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd > 0) {
            fname = uniqnm;
            break;
        }
        id++;

        if (id > 100) {
            log_message(-2, "Unable to create temp file in directory %s.", path_.c_str());
            tempname = "";
            return false;
        }
    } while (1);

    close(fd);
    map_[key] = fname;
    tempname = map_[key];

    log_message(3, "Created temp file \"%s\" for file \"%s\"", fname.c_str(), key);

    return false;
}

void TempFileManager::removeTempFile(const char *key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = map_.find(key);
    if (i == map_.end())
        return;

    if (!i->second.empty()) {
        log_message(3, "Removing temp file \"%s\"", i->second.c_str());
        unlink(i->second.c_str());
    }
    map_.erase(i);
}

TempFileManager tempFileManager;
