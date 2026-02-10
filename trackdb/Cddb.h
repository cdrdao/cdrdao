/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000 Andreas Mueller <mueller@daneb.ping.de>
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
 * Retrieve CDDB data for a 'Toc'
 */

#ifndef __CDDB_H__
#define __CDDB_H__

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <optional>

#include "CdTextItem.h"

namespace fs = std::filesystem;

class Toc;

class Cddb
{
  public:
    struct QueryResults {
        std::string category;
        std::string diskId;
        std::string title;
        bool exactMatch;
    };

    struct CddbEntry {
        std::string diskTitle;
        std::string diskArtist;
        std::string diskExt;
        int ntracks;
	std::vector<std::string> trackTitles;
	std::vector<std::string> trackExt;

	CddbEntry(int n) : ntracks(n),
			   trackTitles(n, std::string()),
			   trackExt(n, std::string()) {}
    };

    Cddb(Toc *t) : toc_(t) {};
    ~Cddb();

    void localCddbDirectory(const std::string &);
    void appendServer(std::string_view s);

    void timeout(int);

    int connectDb(const std::string& userName, const std::string& hostName,
		  const std::string& clientName, const std::string& version);

    int queryDb();
    const std::vector<QueryResults>& results() { return queryResults_; }

    std::optional<CddbEntry>& readDb(const std::string& category, const std::string& diskId);

    int addAsCdText(Toc *toc);

    void printDbQuery();

    // Print the found CDDB entry to stdout. Returns false if no entry
    // available.
    bool printDbEntry();

    static std::string& convertEscapeSequences(std::string& s);
    static void trimSpaces(std::string& s);

  private:
    struct ServerList {
	std::string server;
        unsigned short port;
	std::string httpCgiBin;
	std::string httpProxyServer;
        unsigned short httpProxyPort;
    };

    Toc *toc_;
    std::vector<ServerList> serverList_;
    ServerList *selectedServer_ = nullptr;

    std::string localCddbDirectory_;

    int fd_ = -1;
    int connected_ = false;
    int timeout_ = 20;

    std::string httpCmd_;
    std::string httpData_;
    
    std::vector<QueryResults> queryResults_;
    std::optional<CddbEntry> cddbEntry_;

    static CdTextItem *createItem(CdTextItem::PackType, const std::string& );

    bool openConnection();
    bool openSingleConnection(ServerList*);
    void closeConnection();
    void setupHttpData(const std::string& userName, const std::string& hostName,
                       const std::string& clientName, const std::string& version);

    void appendQueryResult(const std::string& category, const std::string& diskId,
			   const std::string& title, bool exactMatch);
    bool parseQueryResult(std::string_view line, bool exact);

    std::optional<std::string> readLine();
    std::optional<std::string> getServerResponse(int code[3]);
    int sendCommand(const std::vector<std::string> &cmds);
    std::string calcCddbId();
    int readDbEntry(int);
    void shutdown();
    bool getCode(std::string_view, int code[3]);
    int createLocalCddbFile(const std::string& category, const std::string& diskId);
};

#endif
