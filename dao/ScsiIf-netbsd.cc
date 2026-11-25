/*
 * Native implementation of cdrdao's SCSI interface for NetBSD.
 * Copyright (C) by Edgar Fuﬂ, Bonn, May 2003, July 2007.
 * Do with this whatever you like, as long as you are either me or you keep
 * this message intact and both
 * - acknowledge that I wrote it for cdrdao and NetBSD in the first place, and
 * - don't blame me if it doesn't do what you like or expect.
 * These routines do exactly what they do. If that's not what you expect them
 * or would like them to do, don't complain with me, the cdrdao project, my
 * neighbour's brother-in-law or anybody else, but rewrite them to your taste.
 */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <sys/types.h>
/* avoid ../trackdb/util.h */
#include "/usr/include/util.h"
#include <unistd.h>

#include <string>
#include <vector>

#include "ScsiIf.h"
#include "log.h"

#include "decodeSense.cc"

class ScsiIfNetBSD : public ScsiIf
{
  public:
    explicit ScsiIfNetBSD(const std::string &dev);
    ~ScsiIfNetBSD() override;

    int init() override;
    int timeout(int) override;
    int sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                int dataInLen, int showMessage = 1) override;
    const u8 *getSense(int &len) const override;
    void printError() override;

  private:
    std::string name_;
    int fd_ = -1;
    long timeout_ = 5 * 1000; /* in ms */
    struct scsireq screq_;
    std::string error_;
};

ScsiIfNetBSD::ScsiIfNetBSD(const std::string &dev)
{
    const char *PREFIX = "/dev/";
    const char *name = dev.c_str();
    size_t len = dev.size();

    dev_ = dev;
    memset(&screq_, 0, sizeof(screq_));

    if (len == 0) {
        name_.clear();
    } else if (strncmp(name, "/", 1) == 0 || strncmp(name, "./", 2) == 0 ||
               strncmp(name, "../", 3) == 0) {
        name_ = dev;
    } else if (isdigit((unsigned char)name[len - 1]) &&
               (strncmp(name, "sd", 2) == 0 || strncmp(name, "rsd", 3) == 0 ||
                strncmp(name, "cd", 2) == 0 || strncmp(name, "rcd", 3) == 0)) {
        name_ = PREFIX + dev;
        name_ += (char)('a' + getrawpartition());
    } else {
        name_ = PREFIX + dev;
    }

    maxDataLen_ = 64 * 1024;
}

ScsiIfNetBSD::~ScsiIfNetBSD()
{
    if (fd_ >= 0)
        (void)close(fd_);
}

int ScsiIfNetBSD::init()
{
    if (name_.empty())
        return 1;
    if ((fd_ = open(name_.c_str(), O_RDWR, 0)) < 0) {
        log_message(-2, "init: %s", strerror(errno));
        return 1;
    }
    if (inquiry())
        return 2;
    return 0;
}

int ScsiIfNetBSD::timeout(int t)
{
    int ret = timeout_ / 1000;
    timeout_ = t * 1000;
    return ret;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)
int ScsiIfNetBSD::sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                          int dataInLen, int showMessage)
{
    auto fail = [&](const std::string &msg) {
        error_ = "sendCmd: " + msg;
        if (showMessage)
            printError();
        return 1;
    };

    error_.clear();
    /* for printError: */
    screq_.cmdlen = 0;
    screq_.databuf = NULL;
    screq_.datalen = 0;
    screq_.flags = 0;
    screq_.timeout = timeout_;
    if (cmdLen > 16)
        return fail("cmdLen > 16");
    if (cmdLen == 0)
        return fail("cmdLen == 0");
    memcpy(screq_.cmd, cmd, cmdLen);
    screq_.cmdlen = cmdLen;
    if (dataOut != NULL && dataIn != NULL) {
        return fail("Out and In");
    } else if (dataOut != NULL) {
        if (dataOutLen == 0)
            return fail("dataOutLen == 0");
        if (dataInLen != 0)
            return fail("dataInLen != 0");
        screq_.flags |= SCCMD_WRITE;
        screq_.databuf = (caddr_t)dataOut;
        screq_.datalen = dataOutLen;
    } else if (dataIn != NULL) {
        if (dataInLen == 0)
            return fail("dataInLen == 0");
        if (dataOutLen != 0)
            return fail("dataOutLen != 0");
        screq_.flags |= SCCMD_READ;
        screq_.databuf = (caddr_t)dataIn;
        screq_.datalen = dataInLen;
    } else {
        if (dataOutLen != 0 || dataInLen != 0)
            return fail("dataLen != 0");
        screq_.databuf = NULL;
        screq_.datalen = 0;
    }
    screq_.senselen = SENSEBUFLEN;
    if (ioctl(fd_, SCIOCCOMMAND, &screq_) < 0)
        return fail(std::string("SCIOCOMMAND: ") + strerror(errno));
    if (screq_.retsts == SCCMD_OK && screq_.status == 0)
        return 0;
    else if (screq_.retsts == SCCMD_SENSE && screq_.status == 0) {
        if (showMessage)
            printError();
        return 1;
    } else {
        if (showMessage)
            printError();
        return 2;
    }
}

const u8 *ScsiIfNetBSD::getSense(int &len) const
{
    len = screq_.senselen_used;
    return (const u8 *)screq_.sense;
}

void ScsiIfNetBSD::printError()
{
    if (screq_.cmdlen > 0) {
        char s[80];
        char *p = s;
        int i;
        p += snprintf(p, s + sizeof(s) - p, "CDB=");
        for (i = 0; i < screq_.cmdlen; i++) {
            p += snprintf(p, s + sizeof(s) - p, "%.2X ", screq_.cmd[i]);
        }
        p[-1] = ',';
        switch (screq_.flags & (SCCMD_READ | SCCMD_WRITE)) {
        case SCCMD_READ:
            p += snprintf(p, s + sizeof(s) - p, " RD");
            break;
        case SCCMD_WRITE:
            p += snprintf(p, s + sizeof(s) - p, " WR");
            break;
        case SCCMD_READ | SCCMD_WRITE:
            p += snprintf(p, s + sizeof(s) - p, " RW");
            break;
        }
        p += snprintf(p, s + sizeof(s) - p, ", BUF=%p", screq_.databuf);
        p += snprintf(p, s + sizeof(s) - p, ", LEN=%lu", (unsigned long)screq_.datalen);
        p += snprintf(p, s + sizeof(s) - p, ", TO=%lu", (unsigned long)screq_.timeout);
        log_message(-2, "%s", s);
    }
    if (!error_.empty()) {
        log_message(-2, "%s", error_.c_str());
    } else
        switch (screq_.retsts) {
        case SCCMD_OK:
            switch (screq_.status) {
            case 0x00:
                log_message(-2, "GOOD");
                break;
            case 0x02:
                log_message(-2, "CHECK CONDITION");
                break;
            case 0x04:
                log_message(-2, "CONDITION MET");
                break;
            case 0x08:
                log_message(-2, "BUSY");
                break;
            case 0x10:
                log_message(-2, "INTERMEDIATE");
                break;
            case 0x14:
                log_message(-2, "INTERMEDIATE, CONDITION MET");
                break;
            case 0x18:
                log_message(-2, "RESERVATION CONFLICT");
                break;
            case 0x22:
                log_message(-2, "COMMAND TERMINATED");
                break;
            case 0x28:
                log_message(-2, "QUEUE FULL");
                break;
            default:
                log_message(-2, "undefined status");
            }
            break;
        case SCCMD_TIMEOUT:
            log_message(-2, "timeout");
            break;
        case SCCMD_BUSY:
            log_message(-2, "busy");
            break;
        case SCCMD_SENSE:
            decodeSense((const u8 *)screq_.sense, screq_.senselen_used);
            break;
        case SCCMD_UNKNOWN:
            log_message(-2, "unknown error");
            break;
        default:
            log_message(-2, "undefined retsts");
            break;
        }
}

// INQUIRY on an already open descriptor, used by scan() which doesn't
// go through a ScsiIf instance.
static int inq(int fd, std::string &vend, std::string &prod, std::string &rev)
{
    char buf[44];
    struct scsireq screq;

    memset(&screq, 0, sizeof(screq));
    screq.flags = SCCMD_READ;
    screq.timeout = 1000;
    screq.cmd[0] = 0x12;
    screq.cmd[4] = sizeof(buf);
    screq.cmdlen = 6;
    screq.databuf = (caddr_t)&buf;
    screq.datalen = sizeof(buf);
    screq.senselen = SENSEBUFLEN;

    if (ioctl(fd, SCIOCCOMMAND, &screq) < 0 || screq.status != 0 || screq.retsts != SCCMD_OK) {
        vend.clear();
        prod.clear();
        rev.clear();
        return 1;
    }

    auto field = [&](int from, int to) {
        while (to > from && buf[to - 1] == ' ')
            to--;
        return std::string(buf + from, to - from);
    };
    vend = field(8, 16);
    prod = field(16, 32);
    rev = field(32, 36);
    return 0;
}

std::shared_ptr<ScsiIf> ScsiIf::create(const std::string &dev)
{
    return std::make_shared<ScsiIfNetBSD>(dev);
}

ScsiIf::ScanData *ScsiIf::scan(int *len, char *scsi_dev_path)
{
    DIR *dirp;
    struct dirent *dp;
    char c;
    std::vector<ScanData> found;

    *len = 0;
    c = 'a' + getrawpartition();
    if ((dirp = opendir("/dev")) == 0)
        return NULL;

    while ((dp = readdir(dirp)) != NULL) {
        int l = strlen(dp->d_name);
        if (l > 2 && (((strncmp(dp->d_name, "rsd", 3) == 0 || strncmp(dp->d_name, "rcd", 3) == 0) &&
                       isdigit((unsigned char)dp->d_name[l - 2]) && dp->d_name[l - 1] == c) ||
                      ((strncmp(dp->d_name, "enrst", 5) == 0 || strncmp(dp->d_name, "ch", 2) == 0 ||
                        strncmp(dp->d_name, "enss", 4) == 0 || strncmp(dp->d_name, "uk", 2) == 0) &&
                       isdigit((unsigned char)dp->d_name[l - 1])))) {
            std::string s = std::string("/dev/") + dp->d_name;
            int fd = open(s.c_str(), O_RDWR, 0);
            if (fd >= 0) {
                struct scsi_addr saddr;
                ScanData d;
                if (ioctl(fd, SCIOCIDENTIFY, &saddr) >= 0 &&
                    inq(fd, d.vendor, d.product, d.revision) == 0) {
                    d.dev = s;
                    found.push_back(std::move(d));
                }
                (void)close(fd);
            }
        }
    }
    closedir(dirp);

    if (found.empty())
        return NULL;

    ScanData *scanData = new ScanData[found.size()];
    for (size_t i = 0; i < found.size(); i++)
        scanData[i] = found[i];
    *len = (int)found.size();
    return scanData;
}
