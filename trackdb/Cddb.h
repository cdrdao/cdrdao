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

class Toc;

class Cddb {
public:
  struct QueryResults {
    char *category;
    char *diskId;
    char *title;
    int exactMatch;
    struct QueryResults *next;
  };

  struct CddbEntry {
    char *diskTitle;
    char *diskArtist;
    char *diskExt;
    int ntracks;
    char **trackTitles;
    char **trackExt;
  };

  Cddb(const Toc *);
  ~Cddb();

  void localCddbDirectory(const char *);
  void appendServer(const char *s);

  void timeout(int);

  int connectDb(const char *userName, const char *hostName,
		const char *clientName, const char *version);

  int queryDb(QueryResults **);

  int readDb(const char *category, const char *diskId, CddbEntry **);

  int addAsCdText(Toc *toc);

  void printDbQuery();

  // Print the found CDDB entry to stdout. Returns false if no entry
  // available.
  bool printDbEntry();
    
private:
  struct ServerList {
    char *server;
    unsigned short port;
    char *httpCgiBin;
    char *httpProxyServer;
    unsigned short httpProxyPort;
    struct ServerList *next;
  };

  
  const Toc *toc_;
  ServerList *serverList_; // list of CDDB servers
  ServerList *selectedServer_;

  char *localCddbDirectory_;

  int fd_; // file descriptor for connection to CDDB server
  int connected_; // 1 if connection to CDDB server was established, else 0
  int timeout_; // timeout in seconds

  int httpMode_;
  char *httpCmd_;
  char *httpData_;

  QueryResults *queryResults_;
  CddbEntry *cddbEntry_;

  int openConnection();
  void closeConnection();
  void setupHttpData(const char *userName, const char *hostName,
		     const char *clientName, const char *version);

  void appendQueryResult(const char *category, const char *diskId,
			 const char *title, int exactMatch);
  void clearQueryResults();
  void clearCddbEntry();

  const char *readLine();
  const char *getServerResponse(int code[3]);
  int sendCommand(int nargs, const char *args[]);
  const char *calcCddbId();
  int readDbEntry(int);
  void shutdown();
  int createLocalCddbFile(const char *category, const char *diskId);
};

#endif
