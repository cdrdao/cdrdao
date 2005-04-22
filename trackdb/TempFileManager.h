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

#ifndef __TEMPFILEMANAGER_H__
#define __TEMPFILEMANAGER_H__

#include <string>
#include <map>

class TempFileManager {
 public:
    TempFileManager();

    // The destructor will delete all temp files (unless keepTemps is
    // set) that have not expired yet.
    virtual ~TempFileManager();

    bool setTempDirectory(const char* path);
    void setKeepTemps(bool b) { keepTemps_ = b; }
    void setPrefix(const char* prefix) { prefix_ = prefix; }

    // Create a new temporary file, associated with given 'key'. The
    // given name string is set to the temporaty file. Returns false
    // is a new file was created, returns true if a temporary file
    // already exists.
    bool getTempFile(std::string& name, const char* key,
                     const char* extension = NULL);

 private:
    std::string path_;
    std::string prefix_;
    std::map<std::string, std::string> map_;
    bool keepTemps_;
};

extern TempFileManager tempFileManager;

#endif
