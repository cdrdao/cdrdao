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

#include <config.h>

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Toc.h"
#include "CdTextItem.h"
#include "util.h"
#include "log.h"

#include "Cddb.h"


#define CDDB_MAX_LINE_LEN 1024
#define CDDB_DEFAULT_PORT_CDDBP 888
#define CDDB_DEFAULT_PORT_HTTP 80


static int getCode(const char *line, int code[3]);
static unsigned int cddbSum(unsigned int n);
static void convertEscapeSequences(const char *in, char *out);
static int parseQueryResult(char *line, char *category, char *diskId,
			    char *title);


static RETSIGTYPE alarmHandler(int sig)
{
  log_message(0, "ALARM");
#if RETSIGTYPE != void
  return 0;
#endif
}

Cddb::Cddb(const Toc *t)
{
  toc_ = t;

  serverList_ = NULL;
  selectedServer_ = NULL;
  localCddbDirectory_ = NULL;

  fd_ = -1;
  connected_ = 0;
  queryResults_ = NULL;
  cddbEntry_ = NULL;
  httpCmd_ = NULL;
  httpData_ = NULL;
  httpMode_ = 0;

  timeout_ = 20;
}

Cddb::~Cddb()
{
  ServerList *snext;

  shutdown();

  clearQueryResults();
  clearCddbEntry();

  delete[] localCddbDirectory_;
  localCddbDirectory_ = NULL;

  delete[] httpCmd_;
  httpCmd_ = NULL;

  delete[] httpData_;
  httpData_ = NULL;

  while (serverList_ != NULL) {
    snext = serverList_->next;

    delete[] serverList_->server;
    serverList_->server = NULL;

    delete[] serverList_->httpCgiBin;
    serverList_->httpCgiBin = NULL;

    delete[] serverList_->httpProxyServer;
    serverList_->httpProxyServer = NULL;

    delete serverList_;

    serverList_ = snext;
  }
}

void Cddb::timeout(int t)
{
  if (t > 0)
    timeout_ = t;
}

void Cddb::localCddbDirectory(const char *dir)
{
  const char *homeDir;

  delete[] localCddbDirectory_;

  // replace ~/ by the path to the home directory as indicated by $HOME
  if (dir[0] == '~' && dir[1] == '/' && (homeDir = getenv("HOME")) != NULL) {
    localCddbDirectory_ = strdupvCC(homeDir, dir + 1, NULL);
  }
  else {
    localCddbDirectory_ = strdupCC(dir);
  }
}

void Cddb::appendQueryResult(const char *category, const char *diskId,
			     const char *title, int exactMatch)
{
  QueryResults *run, *ent;

  for (run = queryResults_; run != NULL && run->next != NULL;
       run = run->next) ;

  ent = new QueryResults;

  ent->category = strdupCC(category);
  ent->diskId = strdupCC(diskId);
  ent->title = strdupCC(title);
  ent->exactMatch = (exactMatch != 0) ? 1 : 0;

  ent->next = NULL;

  if (run == NULL)
    queryResults_ = ent;
  else
    run->next = ent;
}

void Cddb::clearQueryResults()
{
  QueryResults *next;

  while (queryResults_ != NULL) {
    next = queryResults_->next;

    delete[] queryResults_->category;
    queryResults_->category = NULL;

    delete[] queryResults_->diskId;
    queryResults_->diskId = NULL;
    
    delete[] queryResults_->title;
    queryResults_->title = NULL;

    delete queryResults_;
    queryResults_ = next;
  }
}

void Cddb::clearCddbEntry()
{
  int i;

  if (cddbEntry_ != NULL) {
    delete[] cddbEntry_->diskTitle;
    cddbEntry_->diskTitle = NULL;

    delete[] cddbEntry_->diskArtist;
    cddbEntry_->diskArtist = NULL;

    delete[] cddbEntry_->diskExt;
    cddbEntry_->diskExt = NULL;

    for (i = 0; i < cddbEntry_->ntracks; i++) {
      delete[] cddbEntry_->trackTitles[i];
      cddbEntry_->trackTitles[i] = NULL;

      delete[] cddbEntry_->trackExt[i];
      cddbEntry_->trackExt[i] = NULL;
    }

    delete[] cddbEntry_->trackTitles;
    cddbEntry_->trackTitles = NULL;

    delete[] cddbEntry_->trackExt;
    cddbEntry_->trackExt = NULL;

    delete cddbEntry_;
    cddbEntry_ = NULL;
  }
}

/* Appends a CDDB server name to the server list. Format of server strings:
 * <server>      
 *   connect to <server>, default cddbp port, use cddbp protocol 
 *
 * <server>:<port> 
 *   connect to <server>, port <port>, use cddbp protocol
 * 
 * <server>:<cgi-bin-path>
 *   connect to <server>, default http port, use http protocol,
 *   url: <cgi-bin-path>
 *
 * <server>:<port>:<cgi-bin-path>
 *   connect to <server>, port <port>, use http protocol, url: <cgi-bin-path>
 *
 * <server>:<port>:<cgi-bin-path>:<proxy-server>
 *   connect to <proxy-server>, default http port, use http protocol,
 *   url: http://<server>:<port>/<cgi-bin-path>
 *
 * <server>:<port>:<cgi-bin-path>:<proxy-server>:<proxy-port>
 *   connect to <proxy-server>, port <proxy-port>, use http protocol,
 *   url: http://<server>:<port>/<cgi-bin-path>
 */
void Cddb::appendServer(const char *s)
{
  ServerList *run, *ent;
  char *name;
  char *port;
  char *httpCgiBin = NULL;
  char *httpProxyServer = NULL;
  char *httpProxyPort = NULL;
  unsigned short portNr = CDDB_DEFAULT_PORT_CDDBP;
  unsigned short httpProxyPortNr = CDDB_DEFAULT_PORT_HTTP;

  if (s == NULL || *s == 0)
    return;

  name = strdupCC(s);

  if ((port = strchr(name, ':')) != NULL) {
    *port = 0;
    port++;

    if (!isdigit(*port)) {
      httpCgiBin = port;
      port = NULL;
      portNr = CDDB_DEFAULT_PORT_HTTP;
    }
    else {
      if ((httpCgiBin = strchr(port, ':')) != NULL) {
	*httpCgiBin = 0;
	httpCgiBin++;
      }
    }

    if (httpCgiBin != NULL && 
	(httpProxyServer = strchr(httpCgiBin, ':')) != NULL) {
      *httpProxyServer = 0;
      httpProxyServer++;
    }

    if (httpProxyServer != NULL &&
	(httpProxyPort = strchr(httpProxyServer, ':')) != NULL) {
      *httpProxyPort = 0;
      httpProxyPort++;
    }
  }

  for (run = serverList_; run != NULL && run->next != NULL; run = run->next) {
    if (strcmp(run->server, name) == 0) {
      delete[] name;
      return;
    }
  }

  if (run != NULL && strcmp(run->server, s) == 0) {
    delete[] name;
    return;
  }

  if (port != NULL)
    portNr = (unsigned short)strtoul(port, NULL, 0);

  if (httpProxyPort != NULL)
    httpProxyPortNr = (unsigned short)strtoul(httpProxyPort, NULL, 0);

  ent = new ServerList;
  ent->server = name;
  ent->port = portNr;
  ent->httpCgiBin = (httpCgiBin != NULL) ? strdupCC(httpCgiBin) : NULL;
  if (httpProxyServer != NULL) {
    ent->httpProxyServer = strdupCC(httpProxyServer);
    ent->httpProxyPort = httpProxyPortNr;
  }
  else {
    ent->httpProxyServer = NULL;
    ent->httpProxyPort = 0;
  }
  ent->next = NULL;

  if (run == NULL)
    serverList_ = ent;
  else
    run->next = ent;
}

/* Tries to connect to a CDDB server. If no server was previously connected
 * (selectedServer_ == NULL) all servers from the server list will be tested
 * and the first successful connected server will be taken.
 * Return: 0: OK
 *         1: could not connect to any server
 */

int Cddb::openConnection()
{
  ServerList *run;
  struct hostent *hostEnt;
  struct sockaddr_in sockAddr;
  const char *server;
  unsigned short port;
  struct sigaction newAlarmHandler;
  struct sigaction oldAlarmHandler;
#ifndef HAVE_INET_ATON
  long inetAddr;
#endif

  if (fd_ >= 0) // already connected
    return 0;

  memset(&newAlarmHandler, 0, sizeof(newAlarmHandler));
  sigemptyset(&(newAlarmHandler.sa_mask));

#ifdef UNIXWARE
  newAlarmHandler.sa_handler = (void (*)()) alarmHandler;
#else
  newAlarmHandler.sa_handler = alarmHandler;
#endif
  
  if (sigaction(SIGALRM, &newAlarmHandler, &oldAlarmHandler) != 0) {
    log_message(-2, "CDDB: Cannot install alarm signal handler: %s",
	    strerror(errno));
    return 1;
  }
  alarm(0);

  for (run = (selectedServer_ != NULL) ? selectedServer_ : serverList_;
       run != NULL; 
       run = (selectedServer_ != NULL) ? (ServerList*)0 : run->next) {

    server = run->server;
    port = run->port;

    if (run->httpCgiBin != NULL) {
      if (run->httpProxyServer != NULL) {
	server = run->httpProxyServer;
	port = run->httpProxyPort;

	log_message(1,
		"CDDB: Connecting to http://%s:%u%s via proxy %s:%u ...",
		run->server, run->port, run->httpCgiBin, server, port);
      }
      else {
	log_message(1,
		"CDDB: Connecting to http://%s:%u%s ...", server, port,
		run->httpCgiBin);
      }
    }
    else {
      log_message(1, "CDDB: Connecting to cddbp://%s:%u ...", server, port);
    }

#ifdef HAVE_INET_ATON
    if (!inet_aton(server, &sockAddr.sin_addr)) {
#else
    if ((inetAddr = (long)inet_addr(server)) == -1) {
#endif
      if ((hostEnt = gethostbyname(server)) == NULL ||
	  hostEnt->h_addrtype != AF_INET) {
	alarm(0);
	log_message(-1, "CDDB: Cannot resolve hostname '%s' - skipping.", server);
	continue;
      }
      else {
	memcpy((char*) &sockAddr.sin_addr, hostEnt->h_addr, hostEnt->h_length);
      }
    }
#ifndef HAVE_INET_ATON
    else {
      memcpy((char*)&sockAddr.sin_addr, (char*)&inetAddr, sizeof(inetAddr));
    }
#endif

    log_message(4, "CDDB: Hostname: %s -> IP: %s", server,
	    inet_ntoa(sockAddr.sin_addr));

    if ((fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      log_message(-2, "CDDB: Cannot create socket: %s", strerror(errno));
      goto fail;
    }

    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(port);

    alarm(timeout_);

    if (connect(fd_, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) == 0) {
      alarm(0);
      log_message(1, "CDDB: Ok.");
      selectedServer_ = run;
      break;
    }
    else {
      alarm(0);
      log_message(-1, "CDDB: Failed to connect to '%s:%u: %s", server, port,
	      strerror(errno));
      closeConnection();
    }
  }

 fail:
  connected_ = 0;

  alarm(0);

  if (sigaction(SIGALRM, &oldAlarmHandler, NULL) != 0) {
    log_message(-1, "CDDB: Cannot restore alarm signal handler: %s",
	    strerror(errno));
  }
  
  if (fd_ < 0)
    return 1;

  return 0;
}

/* Closes connection.
 */

void Cddb::closeConnection()
{
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
    connected_ = 0;
  }
}

/* Create some strings that are used for all communications via the http
 * protocol.
 */
void Cddb::setupHttpData(const char *userName, const char *hostName,
			 const char *clientName, const char *version)
{
  delete[] httpCmd_;

  httpCmd_ = strdupvCC("&hello=", userName, "+",  hostName, "+",
		       clientName, "+", version, 
		       "&proto=1", NULL);

  delete[] httpData_;

  httpData_ = strdupvCC("User-Agent: ", clientName, "/", version, "\r\n",
			"Accept: text/plain\r\n", NULL);

}

/* Tries to connect to a server of the internal server list and performs the
 * the client-server handshake.
 * Return: 0: OK
 *         1: could not connect to any server
 *         2: handshake failed
 */

int Cddb::connectDb(const char *userName, const char *hostName,
		    const char *clientName, const char *version)
{
  int code[3];
  const char *response;
  const char *cmdArgs[6];

  if (connected_)
    return 0;

  if (openConnection() != 0)
    return 1;

  if (selectedServer_->httpCgiBin != NULL) {
    httpMode_ = 1;
    setupHttpData(userName, hostName, clientName, version);
    return 0;
  }

  response = getServerResponse(code);

  if (response == NULL) {
    log_message(-2, "CDDB: EOF while waiting for server greeting.");
    closeConnection();
    return 2;
  }

  if (code[0] != 2) {
    log_message(-2, "CDDB: Connection to server denied: %s", response);
    return 2;
  }

  log_message(4, "CDDB: Server greeting: %s", response);

  connected_ = 1;

  cmdArgs[0] = "cddb";
  cmdArgs[1] = "hello";
  cmdArgs[2] = userName;
  cmdArgs[3] = hostName;
  cmdArgs[4] = clientName;
  cmdArgs[5] = version;

  if (sendCommand(6, cmdArgs) != 0) {
    log_message(-2, "CDDB: Failed to send handshake command.");
    return 2;
  }

  response = getServerResponse(code);

  if (response == NULL) {
    log_message(-2, "CDDB: EOF while waiting for server handshake response.");
    return 2;
  }

  if (code[0] != 2) {
    log_message(-2, "CDDB: Server handshake failed: %s", response);
    return 2;
  }

  log_message(4, "CDDB: Handshake response: %s", response);

  return 0;
}

 
/* Print query for current toc
 */
void Cddb::printDbQuery()
{
  const char *cddbId;
  int ntracks;
  const Track *t;
  Msf start, end;
  long diskLength;

  ntracks = toc_->nofTracks();

  cddbId = calcCddbId();

  printf("%s ", cddbId);

  printf("%d ", ntracks);

  TrackIterator itr(toc_);

  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    long trackStart = start.lba() + 150;

    printf("%ld ", trackStart);
  }

  diskLength = toc_->length().min() * 60 + toc_->length().sec() + 2;
  printf("%ld\n", diskLength);
}

bool Cddb::printDbEntry()
{
    if (!cddbEntry_)
        return false;

    if (cddbEntry_->diskArtist)
        printf("Artist: %s\n", cddbEntry_->diskArtist);
    if (cddbEntry_->diskTitle)
        printf("Title: %s\n", cddbEntry_->diskTitle);
    if (cddbEntry_->diskExt)
        printf("Ext: %s\n", cddbEntry_->diskExt);
    for (int i = 0; i < cddbEntry_->ntracks; i++) {
        printf("Track %02d: %s\n", i+1, cddbEntry_->trackTitles[i]);
        if (cddbEntry_->trackExt &&
            cddbEntry_->trackExt[i])
            printf("Trach %02d ext: %s\n", i+1, cddbEntry_->trackExt[i]);
    }

    return true;
}

/* Queries for entries that match the current 'toc_'.
 * 'results' will be filled with a list of matching diskIds/category/title
 * triples. 'results' will be NULL if no matching entry is found.
 * Return: 0: OK
 *         1: communication error occured
 */
int Cddb::queryDb(QueryResults **results)
{
  const char *cddbId;
  const char **args;
  const char *resp;
  char qtitle[CDDB_MAX_LINE_LEN];
  char qcategory[CDDB_MAX_LINE_LEN];
  char qdiskId[CDDB_MAX_LINE_LEN];
  char respBuf[CDDB_MAX_LINE_LEN];
  char *buf;
  int code[3];
  int err = 0;
  int nargs;
  int arg, i;
  int ntracks;
  const Track *t;
  Msf start, end;
  long diskLength;

  // clear previous results
  clearQueryResults();

  if (httpMode_) {
    if (openConnection() != 0)
      return 1;
  }

  ntracks = toc_->nofTracks();

  nargs = ntracks + 5;

  args = new const char*[nargs];
  arg = 0;

  args[arg++] = "cddb";
  args[arg++] = "query";

  cddbId = calcCddbId();

  args[arg++] = cddbId;

  buf = new char[20];
  sprintf(buf, "%d", ntracks);
  args[arg++] = buf;

  TrackIterator itr(toc_);

  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    long trackStart = start.lba() + 150;
    buf = new char[20];

    sprintf(buf, "%ld", trackStart);
    args[arg++] = buf;
  }

  buf = new char[20];
  diskLength = toc_->length().min() * 60 + toc_->length().sec() + 2;
  sprintf(buf, "%ld", diskLength);
  args[arg++] = buf;
  
  if (sendCommand(nargs, args) != 0) {
    log_message(-2, "CDDB: Failed to send QUERY command.");
    err = 1; goto fail;
  }

  if ((resp = getServerResponse(code)) == NULL) {
    log_message(-2, "CDDB: EOF while waiting for QUERY response.");
    err = 1; goto fail;
  }    

  log_message(4, "CDDB: QUERY response: %s", resp);

  if (code[0] != 2) {
    log_message(-2, "CDDB: QUERY failed: %s", resp);
    err = 1; goto fail;
  }
  else {
    if (code[2] == 0) {
      // found exact match
      strcpy(respBuf, resp + 3);
      if (parseQueryResult(respBuf, qcategory, qdiskId, qtitle)) {
	appendQueryResult(qcategory, qdiskId, qtitle, 1);
      }
      else {
	log_message(-2, "CDDB: Received invalid QUERY response: %s", resp);
	err = 1; goto fail;
      }
    }
    else if(code[2] == 1) {
      // found inexact matches
      while ((resp = readLine()) != NULL &&
	     strcmp(resp, ".") != 0) {
	strcpy(respBuf, resp);

	log_message(4, "CDDB: Query data: %s", resp);

	if (parseQueryResult(respBuf, qcategory, qdiskId, qtitle)) {
	  appendQueryResult(qcategory, qdiskId, qtitle, 0);
	}
	else {
	  log_message(-2, "CDDB: Received invalid QUERY data: %s", resp);
	  err = 1; goto fail;
	}
      }

      if (resp == NULL) {
	log_message(-2, "CDDB: EOF while reading QUERY data.");
	err = 1; goto fail;
      }	
    }
    else {
      // found no match
    }
  }
  
  
 fail:

  if (httpMode_)
    closeConnection();

  for (i = 3; i < arg; i++)
    delete[] args[i];

  delete[] args;
  
  *results = queryResults_;
  return err;
  
}

/* Reads CDDB entry for specified category and disk id. 'entry' will be
 * set to the stucture containing the record data.
 * Return: 0: OK
 *         1: communication error or could not retrieve CDDB entry
 */
int Cddb::readDb(const char *category, const char *diskId, CddbEntry **entry)
{
  int code[3];
  const char *args[4];
  const char *resp;
  int localRecordFd = -1;

  clearCddbEntry();

  if (httpMode_) {
    if (openConnection() != 0)
      return 1;
  }

  args[0] = "cddb";
  args[1] = "read";
  args[2] = category;
  args[3] = diskId;

  if (sendCommand(4, args) != 0) {
    log_message(-2, "CDDB: Failed to send READ command.");
    goto fail;
  }
  
  if ((resp = getServerResponse(code)) == NULL) {
    log_message(-2, "CDDB: EOF while waiting for READ response.");
    goto fail;
  }

  log_message(4, "CDDB: READ response: %s", resp);

  if (code[0] == 2) {
    if ((localRecordFd = createLocalCddbFile(category, diskId)) == -2) {
      log_message(-1, "Existing local CDDB record for %s/%s will not be overwritten.", category, diskId);
    }
    if (readDbEntry(localRecordFd) != 0) {
      log_message(-2, "CDDB: Received invalid database entry.");
      goto fail;
    }
  }
  else {
    log_message(-2, "CDDB: READ failed: %s", resp);
    goto fail;
  }
  
  *entry = cddbEntry_;

  if (httpMode_)
    closeConnection();

  if (localRecordFd >= 0)
    close(localRecordFd);

  return 0;

 fail:

  if (httpMode_)
    closeConnection();

  if (localRecordFd >= 0)
    close(localRecordFd);

  *entry = NULL;
  return 1;
}

/* Shuts down the connection to the CDDB server.
 */
void Cddb::shutdown()
{
  const char *resp;
  const char *args[1];
  int code[3];

  if (fd_ < 0) 
    return;

  if (!connected_) {
    closeConnection();
    return;
  }

  args[0] = "quit";

  if (sendCommand(1, args) == 0) {
    if ((resp = getServerResponse(code)) == NULL) {
      log_message(-1, "CDDB: EOF while waiting for QUIT response.");
    }
    else {
      log_message(4, "CDDB: QUIT response: %s", resp);
    }
  }
  else {
    log_message(-1, "CDDB: Failed to send QUIT command.");
  }

  closeConnection();
}


/* Filter characters of given string so that it is suitable as CD-TEXT data.
 */
static char *cdTextFilter(char *s)
{
  char *p = s;

  while (*p != 0) {
    if (*p == '\n' || *p == '\t')
      *p = ' ';

    p++;
  }

  return s;
}

/* Adds the data of the retrieved CDDB record stored in 'cddbEntry_' to
 * the given toc as CD-TEXT data.
 * Return: 1: CD-TEXT data was added
 *         0: toc was not modified
 */
int Cddb::addAsCdText(Toc *toc)
{
  int havePerformer = 0;
  int haveTitle = 0;
  int haveMessage = 0;
  int trun;
  CdTextItem *item;

  if (cddbEntry_ == NULL)
    return 0;

  if (cddbEntry_->diskTitle != NULL)
    haveTitle = 1;

  if (cddbEntry_->diskArtist != NULL)
    havePerformer = 1;

  if (cddbEntry_->diskExt != NULL)
    haveMessage = 1;

  for (trun = 0; trun < cddbEntry_->ntracks; trun++) {
    if (cddbEntry_->trackTitles[trun] != NULL)
      haveTitle = 1;

    if (cddbEntry_->trackExt[trun] != NULL) 
      haveMessage = 1;
  }

  if (!haveTitle && !havePerformer && !haveMessage)
    return 0;

  toc->cdTextLanguage(0, 9);

  if (haveTitle) {
    item = new CdTextItem(CdTextItem::CDTEXT_TITLE, 0,
      (cddbEntry_->diskTitle != NULL) ? cdTextFilter(cddbEntry_->diskTitle) : ""); 

    toc->addCdTextItem(0, item);
  }

  if (havePerformer) {
    item = new CdTextItem(CdTextItem::CDTEXT_PERFORMER, 0,
      (cddbEntry_->diskArtist != NULL) ? cdTextFilter(cddbEntry_->diskArtist) : "");

    toc->addCdTextItem(0, item);
  }

  if (haveMessage) {
    item = new CdTextItem(CdTextItem::CDTEXT_MESSAGE, 0,
      (cddbEntry_->diskExt != NULL) ? cdTextFilter(cddbEntry_->diskExt) : "");

    toc->addCdTextItem(0, item);
  }
    
  for (trun = 0; trun < toc->nofTracks(); trun++) {
    if (haveTitle) {
      
      item = new CdTextItem(CdTextItem::CDTEXT_TITLE, 0,
        (trun < cddbEntry_->ntracks && cddbEntry_->trackTitles[trun] != NULL) ? cdTextFilter(cddbEntry_->trackTitles[trun]) : ""); 

      toc->addCdTextItem(trun + 1, item);
    }

    if (havePerformer) {
      item = new CdTextItem(CdTextItem::CDTEXT_PERFORMER, 0,
        (cddbEntry_->diskArtist != NULL) ? cdTextFilter(cddbEntry_->diskArtist) : "");

      toc->addCdTextItem(trun + 1, item);
    }

    if (haveMessage) {
      item = new CdTextItem(CdTextItem::CDTEXT_MESSAGE, 0,
        (trun < cddbEntry_->ntracks && cddbEntry_->trackExt[trun] != NULL) ? cdTextFilter(cddbEntry_->trackExt[trun]) : "");

      toc->addCdTextItem(trun + 1, item);
    }
  }

  return 1;
}


/* Reads a line (until '\n') from 'fd_'. Checks for timeouts.
 */
const char *Cddb::readLine()
{
  static char buf[CDDB_MAX_LINE_LEN];
  int pos = 0;
  struct timeval tv;
  fd_set readFds;
  int ret;
  char *s;
  int characterRead = 0;

  while (pos < CDDB_MAX_LINE_LEN) {
    FD_ZERO(&readFds);
    FD_SET(fd_, &readFds);

    tv.tv_sec = timeout_;
    tv.tv_usec = 0;

    ret = select(fd_ + 1, &readFds, NULL, NULL, &tv);

    if (ret == 0) {
      log_message(-2, "CDDB: Timeout while reading data.");
      return NULL;
    }
    
    if (ret < 0) {
      log_message(-2, "CDDB: Error while waiting for data: %s", strerror(errno));
      return NULL;
    }

    ret = read(fd_, &(buf[pos]), 1);

    if (ret == 0) {
      // end of file
      break;
    }
    
    if (ret < 0) {
      log_message(-2, "CDDB: Error while reading data: %s", strerror(errno));
      return NULL;
    }
    
    characterRead = 1;

    if (buf[pos] == '\n')
      break;

    pos++;
  }

  if (pos >= CDDB_MAX_LINE_LEN)
    buf[CDDB_MAX_LINE_LEN - 1] = 0;
  else
    buf[pos] = 0;

  if (buf[0] == 0 && !characterRead) {
    // end of file
    return NULL;
  }

  // skip leading blanks
  for (s = buf; *s != 0 && isspace(*s); s++) ;

  // skip trailing blanks
  for (pos = strlen(s) - 1; pos >= 0 && isspace(s[pos]); pos--) 
    s[pos] = 0;

  log_message(5, "CDDB: Data read: %s", s);

  return s;
}

/* Checks if 'line' contains a cddb server status and sets 'code' to the
 * server code.
 * Return: 0: 'line' is not a cddb server status line
 *         1: 'line' is a cddb server status line, 'code' contains valid data
 */
static int getCode(const char *line, int code[3])
{
  if (isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) &&
      isspace(line[3])) {
    code[0] = line[0] - '0';
    code[1] = line[1] - '0';
    code[2] = line[2] - '0';

    return 1;
  }

  return 0;
}


/* Reads lines from 'fd_' until a valid server status line is encountered.
 * 'code' is set to the server status code on success.
 * Return: server status line or 'NULL' on timeout or communication error
 */
const char *Cddb::getServerResponse(int code[3])
{
  const char *line;

  while ((line = readLine()) != NULL &&
	 !getCode(line, code)) ;

  return line;
}

/* Sends command in 'args' to 'fd_'. cdbbp and http protocols are handled.
 * Return: 0: OK
 *         1: communication error occured.
 */
int Cddb::sendCommand(int nargs, const char *args[])
{
  char portBuf[20];
  int len = 0;
  int err = 0;
  char *cmd, *p;
  char *httpCmd = NULL;
  int run, ret;
  struct timeval tv;
  fd_set writeFds;

  // build command line
  for (run = 0; run < nargs; run++)
    len += strlen(args[run]) + 1;

  cmd = new char[len + 1];
  *cmd = 0;

  for (run = 0; run < nargs; run++) {
    strcat(cmd, args[run]);
    if (run != nargs - 1) {
      if (httpMode_)
	strcat(cmd, "+");
      else
	strcat(cmd, " ");
    }
  }

  if (httpMode_) {
    if (selectedServer_->httpProxyServer != NULL) {
      sprintf(portBuf, ":%u", selectedServer_->port);
      httpCmd = strdupvCC("GET http://", selectedServer_->server,
			portBuf, selectedServer_->httpCgiBin, 
			"?cmd=", cmd, httpCmd_, " HTTP/1.0\r\n", 
			"Host: ", selectedServer_->server, "\r\n",
			httpData_, "\r\n", NULL);
    }
    else {
      httpCmd = strdupvCC("GET ", selectedServer_->httpCgiBin, 
			  "?cmd=", cmd, httpCmd_, " HTTP/1.0\r\n", 
			  "Host: ", selectedServer_->server, "\r\n",
			  httpData_, "\r\n", NULL);
    }

    delete[] cmd;
    cmd = httpCmd;
    httpCmd = NULL;

    log_message(4, "CDDB: Sending command '%s'...", cmd);
  }
  else {
    log_message(4, "CDDB: Sending command '%s'...", cmd);
    
    strcat(cmd, "\n");
  }

  len = strlen(cmd);
  p = cmd;

  while (len > 0) {
    FD_ZERO(&writeFds);
    FD_SET(fd_, &writeFds);

    tv.tv_sec = timeout_;
    tv.tv_usec = 0;

    ret = select(fd_ + 1, NULL, &writeFds, NULL, &tv);

    if (ret == 0) {
      log_message(-2, "CDDB: Timeout while sending data.");
      err = 1; goto fail;
    }
    
    if (ret < 0) {
      log_message(-2, "CDDB: Error while waiting for send: %s", strerror(errno));
      err = 1; goto fail;
    }
 
    ret = write(fd_, p, 1);

    if (ret < 0) {
      log_message(-2, "CDDB: Failed to send command '%s': %s", cmd,
	      strerror(errno));
      err = 1; goto fail;
    }

    if (ret != 1) {
      log_message(-2, "CDDB: Failed to send command '%s'.", cmd);
      err = 1; goto fail;
    }

    len--;
    p++;
  }

  log_message(4, "CDDB: Ok.");

  fail:
  delete[] cmd;

  return err;
}

static unsigned int cddbSum(unsigned int n)
{
  unsigned int ret;

  ret = 0;
  while (n > 0) {
    ret += (n % 10);
    n /= 10;
  }

  return ret;
}

const char *Cddb::calcCddbId()
{
  const Track *t;
  Msf start, end;
  unsigned int n = 0;
  unsigned int o = 0;
  int tcount = 0;
  static char buf[20];
  unsigned long id;

  TrackIterator itr(toc_);

  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    if (t->type() == TrackData::AUDIO) {
      n += cddbSum(start.min() * 60 + start.sec() + 2/* gap offset */);
      o  = end.min() * 60 + end.sec();
      tcount++;
    }
  }

  id = (n % 0xff) << 24 | o << 8 | tcount;
  sprintf(buf, "%08lx", id);

  return buf;
} 

static void convertEscapeSequences(const char *in, char *out)
{
  while (*in != 0) {
    if (*in == '\\') {
      switch (*(in + 1)) {
      case 'n':
	*out++ = '\n';
	in++;
	break;

      case 't':
	*out++ = '\t';
	in++;
	break;

      case '\\':
	*out++ = '\\';
	in++;
	break;

      default:
	*out++ = '\\';
	break;
      }
    }
    else {
      *out++ = *in;
    }
	
    in++;
  }

  *out = 0;
}

/* Retrieves the category, disk ID and title from a query response string.
 * The provided strings 'category', 'diskId' and 'title' must point to
 * existing buffers with at least the same length as 'line'.
 * Return: 1 if 'line' was successfully parsed, else 0
 */
static int parseQueryResult(char *line, char *category, char *diskId,
			    char *title)
{
  const char *sep = " \t";
  char *p;
  
  if ((p = strtok(line, sep)) != NULL) {
    strcpy(category, p);

    if ((p = strtok(NULL, sep)) != NULL) {
      strcpy(diskId, p);

      if ((p = strtok(NULL, "")) != NULL) {
	// remove leading white space
	while (*p != 0 && isspace(*p))
	  p++;

	convertEscapeSequences(p, title);

	// remove newline from title string
	if ((p = strchr(title, '\n')) != NULL)
	  *p = 0;

	return 1;
      }
    }
  }

  return 0;
}

/* Reads a CDDB record from 'fd_' and fills 'cddbEntry_' with the required
 * data.
 * Return: 0: OK
 *         1: communication error occured
 */
int Cddb::readDbEntry(int localRecordFd)
{
  const char *resp;
  char buf[CDDB_MAX_LINE_LEN];
  char *line;
  char *p, *s;
  char *val;
  int ntracks = toc_->nofTracks();
  int i, trackNr;
  
  cddbEntry_ = new CddbEntry;

  cddbEntry_->diskTitle = NULL;
  cddbEntry_->diskArtist = NULL;
  cddbEntry_->diskExt = NULL;
  cddbEntry_->ntracks = ntracks;
  cddbEntry_->trackTitles = new char*[ntracks];
  cddbEntry_->trackExt = new char*[ntracks];

  for (i = 0; i < ntracks; i++) {
    cddbEntry_->trackTitles[i] = NULL;
    cddbEntry_->trackExt[i] = NULL;
  }
  

  
  while ((resp = readLine()) != NULL && strcmp(resp, ".") != 0) {
    log_message(4, "CDDB: READ data: %s", resp);

    if (localRecordFd >= 0) {
      // save to local CDDB record file
      fullWrite(localRecordFd, resp, strlen(resp));
      fullWrite(localRecordFd, "\n", 1);
    }

    convertEscapeSequences(resp, buf);

    // remove comments
    //if ((p = strchr(buf, '#')) != NULL)
    //  *p = 0;
    if (buf[0] == '#')
      buf[0] = 0;  // xxam!

    // remove leading blanks
    for (line = buf; *line != 0 && isspace(*line); line++) ;

    if ((val = strchr(line, '=')) != NULL) {
      *val = 0;
      val++;

      if (strcmp(line, "DTITLE") == 0) {
	if (*val != 0) {
	  if (cddbEntry_->diskArtist == NULL) {
	    cddbEntry_->diskArtist = strdupCC(val);
	  }
	  else {
	    s = strdup3CC(cddbEntry_->diskArtist, val, NULL);
	    delete[] cddbEntry_->diskArtist;
	    cddbEntry_->diskArtist = s;
	  }
	}
      }
      else if (strcmp(line, "EXTD") == 0) {
	if (*val != 0) {
	  if (cddbEntry_->diskExt == NULL) {
	    cddbEntry_->diskExt = strdupCC(val);
	  }
	  else {
	    s = strdup3CC(cddbEntry_->diskExt, val, NULL);
	    delete[] cddbEntry_->diskExt;
	    cddbEntry_->diskExt = s;
	  }
	}
      }
      else if (strncmp(line, "TTITLE", 6) == 0) {
	if (*val != 0) {
	  trackNr = atoi(line + 6);
	  if (trackNr >= 0 && trackNr < ntracks) {
	    if (cddbEntry_->trackTitles[trackNr] == NULL) {
	      cddbEntry_->trackTitles[trackNr] = strdupCC(val);
	    }
	    else {
	      s = strdup3CC(cddbEntry_->trackTitles[trackNr], val, NULL);
	      delete[] cddbEntry_->trackTitles[trackNr];
	      cddbEntry_->trackTitles[trackNr] = s;
	    }
	  }
	}
      }
      else if (strncmp(line, "EXTT", 4) == 0) {
	if (*val != 0) {
	  trackNr = atoi(line + 4);
	  if (trackNr >= 0 && trackNr < ntracks) {
	    if (cddbEntry_->trackExt[trackNr] == NULL) {
	      cddbEntry_->trackExt[trackNr] = strdupCC(val);
	    }
	    else {
	      s = strdup3CC(cddbEntry_->trackExt[trackNr], val, NULL);
	      delete[] cddbEntry_->trackExt[trackNr];
	      cddbEntry_->trackExt[trackNr] = s;
	    }
	  }
	}
      }
    }
  }

  if (resp == NULL) {
    log_message(-2, "CDDB: EOF while reading database entry.");
    goto fail;
  }


  if (cddbEntry_->diskArtist != NULL) {
    if ((p = strchr(cddbEntry_->diskArtist, '/')) != NULL) {
      *p = 0;

      // remove leading white space of disk title
      for (s = p + 1; *s != 0 && isspace(*s); s++) ;
      
      cddbEntry_->diskTitle = strdupCC(s);

      // remove trailing white space of disk artist
      for (p = p - 1; p >= cddbEntry_->diskArtist && isspace(*p); p--)
	*p = 0;
    }
    else {
      cddbEntry_->diskTitle = strdupCC(cddbEntry_->diskArtist);
    }
  }

  return 0;

 fail:
  clearCddbEntry();

  return 1;
}


int Cddb::createLocalCddbFile(const char *category, const char *diskId)
{
  char *categoryDir = NULL;
  char *recordFile = NULL;
  struct stat sbuf;
  int ret;
  int fd = -1;

  if (localCddbDirectory_ == NULL)
    return -1;

  ret = stat(localCddbDirectory_, &sbuf);

  if (ret != 0 && errno == ENOENT) {
    log_message(-1, "CDDB: Local CDDB directory \"%s\" does not exist.",
	    localCddbDirectory_);
    return -1;
  }
  else if (ret == 0) {
    if (!S_ISDIR(sbuf.st_mode)) {
      log_message(-2, "CDDB: \"%s\" is not a directory.", localCddbDirectory_);
      return -1;
    }
  }
  else {
    log_message(-2, "CDDB: stat of \"%s\" failed: %s", localCddbDirectory_,
	    strerror(errno));
    return -1;
  }

  categoryDir = strdup3CC(localCddbDirectory_, "/", category);

  ret = stat(categoryDir, &sbuf);

  if (ret != 0 && errno == ENOENT) {
    if (mkdir(categoryDir, 0777) != 0) {
      log_message(-2, "CDDB: Cannot create directory \"%s\": %s", categoryDir,
	      strerror(errno));
      goto fail;
    }
  }
  else if (ret == 0) {
    if (!S_ISDIR(sbuf.st_mode)) {
      log_message(-2, "CDDB: \"%s\" is not a directory.", categoryDir);
    }
  }
  else {
    log_message(-2, "CDDB: stat of \"%s\" failed: %s", categoryDir,
	    strerror(errno));
    goto fail;
  }

  recordFile = strdup3CC(categoryDir, "/", diskId);
  
  ret = stat(recordFile, &sbuf);

  if (ret != 0 && errno == ENOENT) {
    if ((fd = open(recordFile, O_WRONLY|O_CREAT, 0666)) < 0) {
      log_message(-2, "CDDB: Cannot create CDDB record file \"%s\": %s",
	      recordFile, strerror(errno));
      fd = -1;
      goto fail;
    }
  }
  else if (ret == 0) {
    fd = -2;
    goto fail;
  }
  else {
    log_message(-2, "CDDB: stat of \"%s\" failed: %s", categoryDir,
	    strerror(errno));
    goto fail;
  }
  
 fail:

  delete[] categoryDir;
  delete[] recordFile;

  return fd;
}
