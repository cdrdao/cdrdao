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

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <filesystem>
#include <array>
#include <stdexcept>

#include "CdTextItem.h"
#include "Toc.h"
#include "log.h"
#include "util.h"

#include "Cddb.h"

#define CDDB_MAX_LINE_LEN 1024
#define CDDB_DEFAULT_PORT_CDDBP 8880
#define CDDB_DEFAULT_PORT_HTTP 80

static int getCode(const char *line, int code[3]);
static unsigned int cddbSum(unsigned int n);

static RETSIGTYPE alarmHandler(int sig)
{
    log_message(0, "ALARM");
#if RETSIGTYPE != void
    return 0;
#endif
}

Cddb::~Cddb()
{
    shutdown();
}

void Cddb::timeout(int t)
{
    if (t > 0)
        timeout_ = t;
}

void Cddb::localCddbDirectory(const std::string &dir)
{
    const char *homeDir = nullptr;

    // replace ~/ by the path to the home directory as indicated by $HOME
    if (dir[0] == '~' && dir[1] == '/' && (homeDir = getenv("HOME")) != nullptr) {
        localCddbDirectory_ = homeDir;
        localCddbDirectory_ += dir.substr(1);
    } else {
        localCddbDirectory_ = dir;
    }
}

void Cddb::appendQueryResult(const std::string& category, const std::string& diskId,
                             const std::string& title, bool exactMatch)
{
    queryResults_.emplace(queryResults_.end(),
                          QueryResults{category, diskId, title, exactMatch});
}

/* NOTE: gnudb is deprecating the HTTP method.
 *
 *
 * Appends a CDDB server name to the server list. Format of server strings:
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
void Cddb::appendServer(std::string_view name)
{
    std::string port;
    std::string httpCgiBin;
    std::string httpProxyServer;
    std::string httpProxyPort;
    unsigned short portNr = CDDB_DEFAULT_PORT_CDDBP;
    unsigned short httpProxyPortNr = CDDB_DEFAULT_PORT_HTTP;
    std::size_t found;

    if (name.empty())
        return;

    if ((found = name.find_first_of(":")) != std::string::npos) {
        port = name.substr(found + 1);
        name = name.substr(0, found);

        try {
            portNr = std::stoi(port);
            if ((found = port.find_first_of(":")) != std::string::npos) {
                httpCgiBin = port.substr(found + 1);
            }
        } catch (...) {
            httpCgiBin = port;
            port.clear();
            portNr = CDDB_DEFAULT_PORT_HTTP;
        }

        if ((found = httpCgiBin.find_first_of(":")) != std::string::npos) {
            httpProxyServer = httpCgiBin.substr(found + 1);
        }

        if ((found = httpProxyServer.find_first_of(":")) != std::string::npos) {
            httpProxyPort = httpProxyServer.substr(found + 1);
        }
    }

    for (auto const& s : serverList_) {
        if (s.server == name)
            return;
    }

    if (!httpProxyPort.empty()) {
        try {
            httpProxyPortNr = std::stoi(httpProxyPort);
        } catch (...) {
            httpProxyPortNr = CDDB_DEFAULT_PORT_HTTP;
        }
    }

    serverList_.emplace(serverList_.end(),
                        ServerList{std::string{name}, portNr, httpCgiBin,
                            httpProxyServer, httpProxyPortNr});
}

/* Tries to connect to a CDDB server. If no server was previously connected
 * (selectedServer_ == NULL) all servers from the server list will be tested
 * and the first successful connected server will be taken.
 * Return: 0: OK
 *         1: could not connect to any server
 */

bool Cddb::openSingleConnection(ServerList* sl)
{
    struct sockaddr_in sockAddr;
    struct hostent *hostEnt;

    auto server = sl->server;
    auto port = sl->port;

    if (!sl->httpCgiBin.empty()) {
        if (!sl->httpProxyServer.empty()) {
            server = sl->httpProxyServer;
            port = sl->httpProxyPort;

            log_message(1, "CDDB: Connecting to http://%s:%u%s via proxy %s:%u ...",
                        sl->server.c_str(), sl->port, sl->httpCgiBin.c_str(), server.c_str(), port);
        } else {
            log_message(1, "CDDB: Connecting to http://%s:%u%s ...",
                        server.c_str(), port,sl->httpCgiBin.c_str());
        }
    } else {
        log_message(1, "CDDB: Connecting to cddbp://%s:%u ...", server.c_str(), port);
    }

#ifdef HAVE_INET_ATON
    if (!inet_aton(server.c_str(), &sockAddr.sin_addr)) {
#else
    if ((inetAddr = (long)inet_addr(server.c_str())) == -1) {
#endif
        if ((hostEnt = gethostbyname(server.c_str())) == NULL || hostEnt->h_addrtype != AF_INET) {
            alarm(0);
            log_message(-1, "CDDB: Cannot resolve hostname '%s' - skipping.", server.c_str());
            return false;
        } else {
            memcpy((char *)&sockAddr.sin_addr, hostEnt->h_addr, hostEnt->h_length);
        }
    }
#ifndef HAVE_INET_ATON
    else {
        memcpy((char *)&sockAddr.sin_addr, (char *)&inetAddr, sizeof(inetAddr));
    }
#endif

    log_message(4, "CDDB: Hostname: %s -> IP: %s", server.c_str(), inet_ntoa(sockAddr.sin_addr));

    if ((fd_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message(-2, "CDDB: Cannot create socket: %s", strerror(errno));
        return false;
    }

    sockAddr.sin_family = AF_INET;
    sockAddr.sin_port = htons(port);

    alarm(timeout_);

    if (connect(fd_, (struct sockaddr *)&sockAddr, sizeof(sockAddr)) == 0) {
        alarm(0);
        log_message(1, "CDDB: Ok.");
        selectedServer_ = sl;
        return true;
    } else {
        alarm(0);
        log_message(-1, "CDDB: Failed to connect to '%s:%u: %s", server, port, strerror(errno));
        closeConnection();
        return false;
    }
}

//
// Returns true if connection is successful.
//
bool Cddb::openConnection()
{
    struct sigaction newAlarmHandler;
    struct sigaction oldAlarmHandler;
#ifndef HAVE_INET_ATON
    long inetAddr;
#endif

    if (fd_ >= 0) // already connected
        return true;

    memset(&newAlarmHandler, 0, sizeof(newAlarmHandler));
    sigemptyset(&(newAlarmHandler.sa_mask));

#ifdef UNIXWARE
    newAlarmHandler.sa_handler = (void (*)())alarmHandler;
#else
    newAlarmHandler.sa_handler = alarmHandler;
#endif

    if (sigaction(SIGALRM, &newAlarmHandler, &oldAlarmHandler) != 0) {
        log_message(-2, "CDDB: Cannot install alarm signal handler: %s", strerror(errno));
        return false;
    }
    alarm(0);

    bool success = false;

    if (selectedServer_)
        success =  openSingleConnection(selectedServer_);
    else {
        for (auto& srv : serverList_) {
            if (success = openSingleConnection(&srv))
                break;
        }
    }

    if (!success) {
        connected_ = 0;
        alarm(0);
        if (sigaction(SIGALRM, &oldAlarmHandler, NULL) != 0) {
            log_message(-1, "CDDB: Cannot restore alarm signal handler: %s", strerror(errno));
        }
    }

    if (fd_ < 0)
        return true;

    return false;
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
void Cddb::setupHttpData(const std::string& userName, const std::string& hostName,
                         const std::string& clientName, const std::string& version)
{
    httpCmd_ = "&hello=";
    httpCmd_ += userName + "+" + hostName + "+" + clientName + "+" + version + "&proto=6";
    httpData_ =
        "User-Agent: " + clientName + "/" + version + "\r\n" + "Accept: text/plain\r\n";
}

/* Tries to connect to a server of the internal server list and performs the
 * the client-server handshake.
 * Return: 0: OK
 *         1: could not connect to any server
 *         2: handshake failed
 */

int Cddb::connectDb(const std::string& userName, const std::string& hostName,
                    const std::string& clientName, const std::string& version)
{
    int code[3];
    std::string response;
    std::array<std::string, 6> cmdArgs;

    if (connected_)
        return 0;

    if (!openConnection())
        return 1;

    if (selectedServer_->httpCgiBin.empty())
        return 1;
    
    setupHttpData(userName, hostName, clientName, version);
    return 0;
}

/* Print query for current toc
 */
void Cddb::printDbQuery()
{
    int ntracks;
    const Track *t;
    Msf start, end;
    long diskLength;

    ntracks = toc_->nofTracks();

    auto cddbId = calcCddbId();

    std::cout << cddbId << " ";
    std::cout << ntracks << " ";

    TrackIterator itr(toc_);

    for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
        long trackStart = start.lba() + 150;

        std::cout << trackStart << " ";
    }

    diskLength = toc_->length().min() * 60 + toc_->length().sec() + 2;
    std::cout << diskLength << "\n";
}

bool Cddb::printDbEntry()
{
    if (!cddbEntry_)
        return false;

    if (!cddbEntry_->diskArtist.empty())
        std::cout << "Artist: " << cddbEntry_->diskArtist << "\n";
    if (!cddbEntry_->diskTitle.empty())
        std::cout << "Title: " << cddbEntry_->diskTitle << "\n";
    if (!cddbEntry_->diskExt.empty())
        std::cout << "Ext: " << cddbEntry_->diskExt << "\n";
    for (int i = 0; i < cddbEntry_->ntracks; i++) {
        std::printf("Track %02d: ", i + 1);
        std::cout << cddbEntry_->trackTitles[i] << "\n";
        if (!cddbEntry_->trackExt[i].empty())
            printf("Track %02d ext: %s\n", i + 1, cddbEntry_->trackExt[i].c_str());
    }

    return true;
}

/* Queries for entries that match the current 'toc_'.
 * 'results' will be filled with a list of matching diskIds/category/title
 * triples. 'results' will be NULL if no matching entry is found.
 * Return: 0: OK
 *         1: communication error occured
 */
int Cddb::queryDb()
{
    std::vector<std::string> args;
    int code[3];
    int err = 0;

    // clear previous results
    queryResults_.clear();

    if (!openConnection())
        return 1;

    args = {"cddb", "query"};

    auto cddbId = calcCddbId();

    args.push_back(cddbId);
    args.push_back(std::to_string(toc_->nofTracks()));
    {
        TrackIterator itr(toc_);
        Msf start, end;
        for (auto t = itr.first(start, end); t != NULL; t = itr.next(start, end))
            args.push_back(std::to_string(start.lba() + 150));
        long diskLength = toc_->length().min() * 60 + toc_->length().sec() + 2;
        args.push_back(std::to_string(diskLength));
    }

    if (sendCommand(args) != 0) {
        log_message(-2, "CDDB: Failed to send QUERY command.");
        closeConnection();
        return 1;
    }

    auto resp = getServerResponse(code);
    if (!resp) {
        log_message(-2, "CDDB: EOF while waiting for QUERY response.");
        closeConnection();
        return 1;
    }

    log_message(4, "CDDB: QUERY response: %s", resp->c_str());

    if (code[0] != 2) {
        log_message(-2, "CDDB: QUERY failed: %s", resp->c_str());
        err = 1;
    } else {
        if (code[2] == 0) {
            // found exact match
            auto resp = readLine();
            if (!parseQueryResult(*resp, true)) {
                log_message(-2, "CDDB: Received invalid QUERY response: %s", resp->c_str());
                err = 1;
            }
        } else if (code[2] == 1) {
            // found inexact matches
            while (true) {
                auto resp = readLine();
                if (!resp || *resp == ".")
                    break;

                log_message(4, "CDDB: Query data: %s", resp->c_str());

                if (!parseQueryResult(*resp, false)) {
                    log_message(-2, "CDDB: Received invalid QUERY data: %s", resp->c_str());
                    err = 1;
                    break;
                }
            }
        }
    }

    if (err)
        closeConnection();

    return err;
}

/* Reads CDDB entry for specified category and disk id. 'entry' will be
 * set to the stucture containing the record data.
 * Return: 0: OK
 *         1: communication error or could not retrieve CDDB entry
 */
std::optional<Cddb::CddbEntry>& Cddb::readDb(const std::string& category, const std::string& diskId)
{
    int code[3];
    int localRecordFd = -1;

    try {

        if (!openConnection())
            throw std::runtime_error("");

        if (sendCommand({"cddb", "read", category, diskId}) != 0) {
            log_message(-2, "CDDB: Failed to send READ command.");
            throw std::runtime_error("");
        }

        auto resp = getServerResponse(code);
        if (!resp) {
            log_message(-2, "CDDB: EOF while waiting for READ response.");
            throw std::runtime_error("");
        }

        log_message(4, "CDDB: READ response: %s", resp->c_str());

        if (code[0] == 2) {
            if ((localRecordFd = createLocalCddbFile(category, diskId)) == -2) {
                log_message(-1, "Existing local CDDB record for %s/%s will not be overwritten.",
                            category, diskId);
            }
            if (readDbEntry(localRecordFd) != 0) {
                log_message(-2, "CDDB: Received invalid database entry.");
                throw std::runtime_error("");
            }
        } else {
            log_message(-2, "CDDB: READ failed: %s", resp);
            throw std::runtime_error("");
        }
    } catch (...) {
        cddbEntry_ = std::nullopt;
    }

    closeConnection();

    if (localRecordFd >= 0)
        close(localRecordFd);

    return cddbEntry_;
}

/* Shuts down the connection to the CDDB server.
 */
void Cddb::shutdown()
{
    int code[3];

    if (fd_ < 0)
        return;

    if (!connected_) {
        closeConnection();
        return;
    }

    if (sendCommand({"quit"}) == 0) {
        auto resp = getServerResponse(code);
        if (!resp) {
            log_message(-1, "CDDB: EOF while waiting for QUIT response.");
        } else {
            log_message(4, "CDDB: QUIT response: %s", resp->c_str());
        }
    } else {
        log_message(-1, "CDDB: Failed to send QUIT command.");
    }

    closeConnection();
}

/* Filter characters of given string so that it is suitable as CD-TEXT data.
 */
static std::string cdTextFilter(std::string_view s)
{
    std::string ret;

    for (auto c : s) {
        if (c == '\n' ||  c == '\t')
            c = ' ';
        ret.push_back(c);
    }
    return ret;
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

    if (!cddbEntry_)
        return 0;

    if (!cddbEntry_->diskTitle.empty())
        haveTitle = 1;

    if (!cddbEntry_->diskArtist.empty())
        havePerformer = 1;

    if (!cddbEntry_->diskExt.empty())
        haveMessage = 1;

    for (trun = 0; trun < cddbEntry_->ntracks; trun++) {
        if (!cddbEntry_->trackTitles[trun].empty())
            haveTitle = 1;

        if (!cddbEntry_->trackExt[trun].empty())
            haveMessage = 1;
    }

    if (!haveTitle && !havePerformer && !haveMessage)
        return 0;

    toc->cdTextLanguage(0, 9);

    if (haveTitle) {
        item =
            createItem(CdTextItem::PackType::TITLE,
                       cddbEntry_->diskTitle.empty() ? "" :
                       cdTextFilter(cddbEntry_->diskTitle.c_str()));
        toc->addCdTextItem(0, item);
    }

    if (havePerformer) {
        item = createItem(CdTextItem::PackType::PERFORMER,
                          cddbEntry_->diskArtist.empty() ? "" :
                          cdTextFilter(cddbEntry_->diskArtist.c_str()));
        toc->addCdTextItem(0, item);
    }

    if (haveMessage) {
        item = createItem(CdTextItem::PackType::MESSAGE,
                          cddbEntry_->diskExt.empty() ? "" :
                          cdTextFilter(cddbEntry_->diskExt.c_str()));
        toc->addCdTextItem(0, item);
    }

    for (trun = 0; trun < toc->nofTracks(); trun++) {

        item = nullptr;
        if (haveTitle) {
            item = createItem(CdTextItem::PackType::TITLE,
                              (trun < cddbEntry_->ntracks && !cddbEntry_->trackTitles[trun].empty())
                                  ? cdTextFilter(cddbEntry_->trackTitles[trun])
                                  : "");
            toc->addCdTextItem(trun + 1, item);
        }
        if (havePerformer) {
            item = createItem(
                CdTextItem::PackType::PERFORMER,
                cddbEntry_->diskArtist.empty() ? "" : cdTextFilter(cddbEntry_->diskArtist));
            toc->addCdTextItem(trun + 1, item);
        }
        if (haveMessage) {
            item = createItem(CdTextItem::PackType::MESSAGE,
                              (trun < cddbEntry_->ntracks && !cddbEntry_->trackExt[trun].empty())
                                  ? cdTextFilter(cddbEntry_->trackExt[trun])
                                  : "");
            toc->addCdTextItem(trun + 1, item);
        }
    }

    return 1;
}

/* Reads a line (until '\n') from 'fd_'. Checks for timeouts.
 */
std::optional<std::string> Cddb::readLine()
{
    static std::vector<char> read_buf_(CDDB_MAX_LINE_LEN);
    static bool eof = false;
    static std::string buffer;

    std::string line;

    while (true) {
        auto nl = buffer.find('\n');
        if (nl != std::string::npos) {
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            break;
        }
        if (eof) {
            if (buffer.empty()) return std::nullopt;
            std::string line = std::move(buffer);
            buffer.clear();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            break;
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        timeval tv;
        tv.tv_sec = timeout_;
        tv.tv_usec = 0;

        auto ret = ::select(fd_ + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0 && errno != EINTR) {
            log_message(-2, "CDDB: Error while waiting for data: %s", strerror(errno));
            return std::nullopt;
        }
        if (ret == 0) {
            log_message(-2, "CDDB: Timeout while reading data.");
            return std::nullopt;
        }

        auto n = ::read(fd_, read_buf_.data(), read_buf_.size());
        if (n < 0) {
            log_message(-2, "CDDB: Error while reading data: %s", strerror(errno));
            return std::nullopt;
        }

        if (n == 0) {
            eof = true;
            continue;
        }
        buffer.append(read_buf_.data(), (size_t)n);
    }

    auto first = line.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos)
        return std::string();
    auto last = line.find_last_not_of(" \t\n\r\f\v");
    line = line.substr(first, (last - first + 1));
    log_message(5, "CDDB: Data read: %s", line.c_str());

    return line;
}

void Cddb::trimSpaces(std::string& str)
{
    auto first = str.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) {
        str.clear();
        return;
    }
    auto last = str.find_last_not_of(" \t\n\r\f\v");
    str = str.substr(first, (last - first + 1));
}

/* Checks if 'line' contains a cddb server status and sets 'code' to the
 * server code.
 * Return: 0: 'line' is not a cddb server status line
 *         1: 'line' is a cddb server status line, 'code' contains valid data
 */
bool Cddb::getCode(std::string_view line, int code[3])
{
    if (isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2]) && isspace(line[3])) {
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
std::optional<std::string> Cddb::getServerResponse(int code[3])
{
    while (auto line = readLine()) {
        if (getCode(*line, code))
            return line;
    }
    return std::nullopt;
}

/* Sends command in 'args' to 'fd_'. cdbbp and http protocols are handled.
 * Return: 0: OK
 *         1: communication error occured.
 */
int Cddb::sendCommand(const std::vector<std::string> &args)
{
    int err = 0;
    std::string cmd;

    for (const auto &s : args) {
        if (!cmd.empty())
            cmd += "+";
        cmd += s;
    }

    std::ostringstream ss;
    if (!selectedServer_->httpProxyServer.empty()) {
        ss << "GET http://" << selectedServer_->server;
        ss << selectedServer_->port << selectedServer_->httpCgiBin;
        ss << "?cmd=" << cmd << httpCmd_ << " HTTP/1.0\r\n";
        ss << "Host: " << selectedServer_->server << "\r\n";
        ss << httpData_ << "\r\n";
    } else {
        ss << "GET " << selectedServer_->httpCgiBin << "?cmd=" << cmd
           << httpCmd_
           << " HTTP/1.0\r\n"
           << "Host: " << selectedServer_->server << "\r\n"
           << httpData_ << "\r\n";
    }

    cmd = ss.str();
    log_message(0, "CDDB: Sending command '%s'...", cmd.c_str());

    auto ret = ::write(fd_, cmd.c_str(), cmd.size());

    if (ret != cmd.size()) {
        log_message(-2, "CDDB: Failed to send command '%s': %s", cmd.c_str(), strerror(errno));
        err = 1;
        goto fail;
    }

    log_message(4, "CDDB: Ok.");

fail:
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

std::string Cddb::calcCddbId()
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
            n += cddbSum(start.min() * 60 + start.sec() + 2 /* gap offset */);
            o = end.min() * 60 + end.sec();
            tcount++;
        }
    }

    id = (n % 0xff) << 24 | o << 8 | tcount;
    snprintf(buf, sizeof(buf), "%08lx", id);

    return buf;
}

std::string& Cddb::convertEscapeSequences(std::string& s)
{
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case 'n':  result += '\n'; ++i; break;
            case 't':  result += '\t'; ++i; break;
            case '\\': result += '\\'; ++i; break;
            default:   result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    s = std::move(result);
    return s;
}

// Retrieves the category, disk ID and title from a query response
// string.  and append to the result list.  Returns false if parsing
// failed.
///
bool Cddb::parseQueryResult(std::string_view line, bool exactMatch)
{
    std::array<std::string, 3> result;
    size_t componentIndex = 0;

    for (char c : line) {
        if (c == '\n') break;

        if (c == ' ' || c == '\t') {
            if (!result[componentIndex].empty()) {
                ++componentIndex;
                if (componentIndex >= 3) break;
            }
        } else {
            result[componentIndex] += c;
        }
    }

    // Check that all three components are present and non-empty
    if (result[0].empty() || result[1].empty() || result[2].empty()) {
        return false;
    }

    appendQueryResult(convertEscapeSequences(result[0]),
                      convertEscapeSequences(result[1]),
                      convertEscapeSequences(result[2]),
                      exactMatch);
    return true;
}

/* Reads a CDDB record from 'fd_' and fills 'cddbEntry_' with the required
 * data.
 * Return: 0: OK
 *         1: communication error occured
 */
int Cddb::readDbEntry(int localRecordFd)
{
    char *line;
    char *p, *s;
    char *val;
    int ntracks = toc_->nofTracks();
    int i, trackNr;

    cddbEntry_.emplace(ntracks);

    while (true) {
        auto resp = readLine();
        if (!resp || *resp == ".")
            break;

        log_message(4, "CDDB: READ data: %s", resp->c_str());

        if (localRecordFd >= 0) {
            // save to local CDDB record file
            fullWrite(localRecordFd, *resp);
            fullWrite(localRecordFd, "\n");
        }

        convertEscapeSequences(*resp);

        auto equal = resp->find('=');
        if (equal != std::string::npos) {
            std::string key = resp->substr(0, equal);
            std::string value = resp->substr(equal+1);

            if (key == "DTITLE") {
                cddbEntry_->diskArtist = value;
            } else if (key == "EXTD") {
                cddbEntry_->diskExt = value;
            } else if (key.substr(0,6) == "TTITLE") {
                try {
                    auto trackNr = std::stoi(key.substr(6));
                    if (trackNr >= 0 && trackNr < ntracks) {
                        cddbEntry_->trackTitles[trackNr] = value;
                    }
                } catch (...) {
                }
            } else if (key.substr(0,4) == "EXTT") {
                try {
                    auto trackNr = std::stoi(key.substr(4));
                    if (trackNr >= 0 && trackNr < ntracks) {
                        cddbEntry_->trackExt[trackNr] = value;
                    }
                } catch (...) {
                }
            }
        }
    }

    auto found = cddbEntry_->diskArtist.find('/');
    if (found != std::string::npos) {
        cddbEntry_->diskArtist = cddbEntry_->diskArtist.substr(0, found);
        cddbEntry_->diskTitle = cddbEntry_->diskArtist.substr(found+1);
    } else {
        cddbEntry_->diskTitle = cddbEntry_->diskArtist;
    }
    trimSpaces(cddbEntry_->diskArtist);
    trimSpaces(cddbEntry_->diskTitle);

    return 0;
}

int Cddb::createLocalCddbFile(const std::string& category, const std::string& diskId)
{
    std::string categoryDir;
    std::string recordFile;
    struct stat sbuf;
    int ret;
    int fd = -1;

    if (localCddbDirectory_.empty())
        return -1;

    ret = stat(localCddbDirectory_.c_str(), &sbuf);

    if (ret != 0 && errno == ENOENT) {
        log_message(-1, "CDDB: Local CDDB directory \"%s\" does not exist.",
                    localCddbDirectory_.c_str());
        return -1;
    } else if (ret == 0) {
        if (!S_ISDIR(sbuf.st_mode)) {
            log_message(-2, "CDDB: \"%s\" is not a directory.", localCddbDirectory_.c_str());
            return -1;
        }
    } else {
        log_message(-2, "CDDB: stat of \"%s\" failed: %s", localCddbDirectory_.c_str(),
                    strerror(errno));
        return -1;
    }

    categoryDir = localCddbDirectory_ + "/" + category;

    ret = stat(categoryDir.c_str(), &sbuf);

    if (ret != 0 && errno == ENOENT) {
        if (mkdir(categoryDir.c_str(), 0777) != 0) {
            log_message(-2, "CDDB: Cannot create directory \"%s\": %s", categoryDir.c_str(),
                        strerror(errno));
            goto fail;
        }
    } else if (ret == 0) {
        if (!S_ISDIR(sbuf.st_mode)) {
            log_message(-2, "CDDB: \"%s\" is not a directory.", categoryDir.c_str());
        }
    } else {
        log_message(-2, "CDDB: stat of \"%s\" failed: %s", categoryDir.c_str(), strerror(errno));
        goto fail;
    }

    recordFile = categoryDir + "/" + diskId;

    ret = stat(recordFile.c_str(), &sbuf);

    if (ret != 0 && errno == ENOENT) {
        if ((fd = open(recordFile.c_str(), O_WRONLY | O_CREAT, 0666)) < 0) {
            log_message(-2, "CDDB: Cannot create CDDB record file \"%s\": %s", recordFile.c_str(),
                        strerror(errno));
            fd = -1;
            goto fail;
        }
    } else if (ret == 0) {
        fd = -2;
        goto fail;
    } else {
        log_message(-2, "CDDB: stat of \"%s\" failed: %s", categoryDir.c_str(), strerror(errno));
        goto fail;
    }

fail:

    return fd;
}

CdTextItem *Cddb::createItem(CdTextItem::PackType ptype, const std::string& text)
{
    auto item = new CdTextItem(ptype, 0);
    item->setText(text.c_str());
    return item;
}
