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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#define DEFAULT_TEMP_PATH "/tmp/"

TempFileManager::TempFileManager()
{
  path_ = DEFAULT_TEMP_PATH;
  keepTemps_ = false;
  prefix_ = "cdrdao.";
}

TempFileManager::~TempFileManager()
{
    if (!keepTemps_) {
        std::map<std::string, std::string>::const_iterator i = map_.begin();

        for (;i != map_.end(); i++) {
            std::string tmpFile = (*i).second;
            log_message(3, "Removing temp file \"%s\"", tmpFile.c_str());
            unlink(tmpFile.c_str());
        }
    }
}

bool TempFileManager::setTempDirectory(const char* path)
{
  struct stat st;

  int ret = stat(path, &st);

  if (ret != 0) {
    log_message(-2, "Could not find temporary directory %s.",
            path);
    return false;
  }

  if (!S_ISDIR(st.st_mode) || access(path, W_OK) != 0) {
    log_message(-2, "No permission for temp directory %s.",
            path);
    return false;
  }

  path_ = path;

  if (path[path_.size() - 1] != '/')
    path_ += '/';

  return true;
}

bool TempFileManager::getTempFile(std::string& tempname, const char* key,
                                  const char* extension)
{
  if (!map_[key].empty()) {
    tempname = map_[key];
    return true;
  }

  const char* shortname = strrchr(key, '/');
  if (shortname)
    shortname++;
  else
    shortname = key;

  std::string fname = path_;
  fname += prefix_;
  fname += shortname;

  int id = 1;
  int fd;
  do {
    char tmpbuf[12];
    std::string uniqnm = fname;
    sprintf(tmpbuf, ".%d", id);
    uniqnm += tmpbuf;
    if (extension) {
      uniqnm += ".";
      uniqnm += extension;
    }
    fd = open(uniqnm.c_str(), O_CREAT|O_EXCL,
              S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd > 0) {
      fname = uniqnm;
      break;
    }
    id++;

    if (id > 100) {
      log_message(-2, "Unable to create temp file in directory %s.",
              path_.c_str());
      tempname = "";
      return false;
    }
  } while(1);

  close(fd);
  map_[key] = fname;
  tempname = map_[key];

  log_message(3, "Created temp file \"%s\" for file \"%s\"", fname.c_str(), key);
    
  return false;
}

TempFileManager tempFileManager;
