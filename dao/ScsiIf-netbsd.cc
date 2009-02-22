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
#include <string.h>
#include <errno.h>
#include <sys/scsiio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
/* avoid ../trackdb/util.h */
#include "/usr/include/util.h"
#include <unistd.h>

#include "ScsiIf.h"

/* can't include trackdb/util.h */
extern void message(int level, const char *fmt, ...);

#include "decodeSense.cc"

#define MAX_SCAN 32

class ScsiIfImpl {
public:
	char *name_;
	int fd_;
	long timeout_; /* in ms */
	struct scsireq screq_;
	char *error_;
};

ScsiIf::ScsiIf(const char *name)
{
#define PREFIX "/dev/"
	int len;

	impl_ = new ScsiIfImpl;
	len = strlen(name);
	if (len == 0) {
		impl_->name_ = NULL;
	} else if (strncmp(name, "/", 1) == 0 ||
	           strncmp(name, "./", 2) == 0 ||
	           strncmp(name, "../", 3) == 0) {
		impl_->name_ = new char[len + 1];
		strcpy(impl_->name_, name);
	} else if (isdigit(name[len-1]) &&
	           (strncmp(name, "sd", 2) == 0 ||
	            strncmp(name, "rsd", 3) == 0 ||
	            strncmp(name, "cd", 2) == 0 ||
	            strncmp(name, "rcd", 3) == 0)) {
		impl_->name_ = new char[strlen(PREFIX) + len + 1 + 1];
		strcpy(impl_->name_, PREFIX);
		strcat(impl_->name_, name);
		impl_->name_[strlen(PREFIX) + len] = 'a' + getrawpartition();
		impl_->name_[strlen(PREFIX) + len + 1] = '\0';
	} else {
		impl_->name_ = new char[strlen(PREFIX) + len + 1];
		strcpy(impl_->name_, PREFIX);
		strcat(impl_->name_, name);
	}
	impl_->fd_ = -1;
	impl_->timeout_ = 5*1000;
	impl_->error_ = NULL;

	maxDataLen_ = 64 * 1024;
	vendor_[0] = 0;
	product_[0] = 0;
	revision_[0] = 0;
#undef PREFIX
}

ScsiIf::~ScsiIf()
{
	if (impl_->fd_ >= 0) (void)close(impl_->fd_);
	if (impl_->name_ != NULL) delete[] impl_->name_;
	if (impl_->error_ != NULL) delete[] impl_->error_;
	delete impl_;
}

int ScsiIf::init()
{
	if (impl_->name_ == NULL) return 1;
	if ((impl_->fd_ = open(impl_->name_, O_RDWR, 0)) < 0) {
		message(-2, "init: %s", strerror(errno));
		return 1;
	}
	if (inquiry()) return 2;
	return 0;
}

int ScsiIf::timeout(int t)
{
	int ret = impl_->timeout_/1000;
	impl_->timeout_ = t*1000;
	return ret;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)
int ScsiIf::sendCmd(const unsigned char *cmd, int cmdLen, 
                    const unsigned char *dataOut, int dataOutLen,
                    unsigned char *dataIn, int dataInLen,
                    int showMessage)
{
#define ERROR(msg) {\
	impl_->error_ = new char[9 + strlen(msg) + 1];\
	strcpy(impl_->error_, "sendCmd: ");\
	strcat(impl_->error_, msg);\
	if (showMessage) printError();\
	return 1;\
}
	if (impl_->error_ != NULL) { delete[] impl_->error_; impl_->error_ = NULL; }
	/* for printError: */
	impl_->screq_.cmdlen = 0;
	impl_->screq_.databuf = NULL; impl_->screq_.datalen = 0;
	impl_->screq_.flags = 0;
	impl_->screq_.timeout = impl_->timeout_;
	if (cmdLen > 16) ERROR("cmdLen > 16");
	if (cmdLen == 0) ERROR("cmdLen == 0");
	memcpy(impl_->screq_.cmd, cmd, cmdLen);
	impl_->screq_.cmdlen = cmdLen;
	if (dataOut != NULL && dataIn != NULL) {
		ERROR("Out and In");
	} else if (dataOut != NULL) {
		if (dataOutLen == 0) ERROR("dataOutLen == 0");
		if (dataInLen != 0) ERROR("dataInLen != 0");
		impl_->screq_.flags |= SCCMD_WRITE;
		impl_->screq_.databuf = (caddr_t)dataOut;
		impl_->screq_.datalen = dataOutLen;
	} else if (dataIn != NULL) {
		if (dataInLen == 0) ERROR("dataInLen == 0");
		if (dataOutLen !=0) ERROR("dataOutLen != 0");
		impl_->screq_.flags |= SCCMD_READ;
		impl_->screq_.databuf = (caddr_t)dataIn;
		impl_->screq_.datalen = dataInLen;
	} else {
		if (dataOutLen !=0 || dataInLen != 0) ERROR("dataLen != 0");
		impl_->screq_.databuf = NULL;
		impl_->screq_.datalen = 0;
	}
	impl_->screq_.senselen = SENSEBUFLEN;
	if (ioctl(impl_->fd_, SCIOCCOMMAND, &impl_->screq_) < 0) {
		char str[80];
		strcpy(str, "SCIOCOMMAND: "); strcat(str, strerror(errno));
		ERROR(str);
	}
	if (impl_->screq_.retsts == SCCMD_OK && impl_->screq_.status == 0) 
		return 0;
	else if (impl_->screq_.retsts == SCCMD_SENSE && impl_->screq_.status == 0)  {
		if (showMessage) printError();
		return 1;
	} else {
		if (showMessage) printError();
		return 2;
	}
#undef ERROR
}

const unsigned char *ScsiIf::getSense(int &len) const
{
	len = impl_->screq_.senselen_used;
	return impl_->screq_.sense;
}

void ScsiIf::printError()
{
	if (impl_->screq_.cmdlen > 0) {
		char s[80];
		char *p = s;
		int i;
		p += snprintf(p, s + sizeof(s) - p, "CDB=");
		for (i = 0; i < impl_->screq_.cmdlen; i++) {
			p += snprintf(p, s + sizeof(s) - p, "%.2X ", impl_->screq_.cmd[i]);
		}
		p[-1] = ',';
		switch (impl_->screq_.flags & (SCCMD_READ | SCCMD_WRITE)) {
			case SCCMD_READ: p += snprintf(p, s + sizeof(s) - p, " RD"); break;
			case SCCMD_WRITE: p += snprintf(p, s + sizeof(s) - p, " WR"); break;
			case SCCMD_READ | SCCMD_WRITE: p += snprintf(p, s + sizeof(s) - p, " RW"); break;
		}
		p += snprintf(p, s + sizeof(s) - p, ", BUF=%p", impl_->screq_.databuf);
		p += snprintf(p, s + sizeof(s) - p, ", LEN=%lu", impl_->screq_.datalen);
		p += snprintf(p, s + sizeof(s) - p, ", TO=%lu", impl_->screq_.timeout);
		message(-2, s);
	}
	if (impl_->error_ != NULL) {
		message(-2, impl_->error_);
	} else switch (impl_->screq_.retsts) {
		case SCCMD_OK: switch (impl_->screq_.status) {
			case 0x00: message(-2, "GOOD"); break;
			case 0x02: message(-2, "CHECK CONDITION"); break;
			case 0x04: message(-2, "CONDITION MET"); break;
			case 0x08: message(-2, "BUSY"); break;
			case 0x10: message(-2, "INTERMEDIATE"); break;
			case 0x14: message(-2, "INTERMEDIATE, CONDITION MET"); break;
			case 0x18: message(-2, "RESERVATION CONFLICT"); break;
			case 0x22: message(-2, "COMMAND TERMINATED"); break;
			case 0x28: message(-2, "QUEUE FULL"); break;
			default: message(-2, "undefined status");
		} break;
		case SCCMD_TIMEOUT: message(-2, "timeout"); break;
		case SCCMD_BUSY: message(-2, "busy"); break;
		case SCCMD_SENSE: decodeSense(impl_->screq_.sense,
		                              impl_->screq_.senselen_used); break;
		case SCCMD_UNKNOWN: message(-2, "unknown error"); break;
		default: message(-2, "undefined retsts"); break;
	}
}

int inq(int fd, char *vend, char *prod, char *rev)
{
	char buf[44];
	struct scsireq screq = {
		/* flags */ SCCMD_READ,
		/* timeout */ 1000,
		/* cmd */ {0x12, 0, 0, 0, sizeof(buf), 0},
		/* cmdlen */ 6,
		/* databuf */ (caddr_t)&buf,
		/* datalen */ sizeof(buf),
		/* datalen_used */ 0,
		/* sense */ {},
		/* senselen */ SENSEBUFLEN,
		/* senselen_used */ 0,
		/* status */ 0,
		/* retsts */ 0,
		/* error */ 0
	};
	char *p, *q;

	if (ioctl(fd, SCIOCCOMMAND, &screq) < 0 ||
	          screq.status != 0 ||
	          screq.retsts != SCCMD_OK) {
		vend[0] = prod[0] = rev[0] = '\0';
		return 1;
	}
	p = buf + 8; q = buf + 16; while (q > p && q[-1] == ' ') q--;
	memcpy(vend, p, q - p); vend[q - p] = '\0';
	p = buf + 16; q = buf + 32; while (q > p && q[-1] == ' ') q--;
	memcpy(prod, p, q - p); prod[q - p] = '\0';
	p = buf + 32; q = buf + 36; while (q > p && q[-1] == ' ') q--;
	memcpy(rev, p, q - p); rev[q - p] = '\0';
	return 0;
}

int ScsiIf::inquiry()
{
	return inq(impl_->fd_, vendor_, product_, revision_);
}

ScsiIf::ScanData *ScsiIf::scan(int *len)
{
	DIR *dirp;
	struct dirent *dp;
	char c;
	int l;
	int fd;
	struct scsi_addr saddr;
	ScanData *scanData;
	char *s;

	scanData = new ScanData[MAX_SCAN]; *len = 0;
	c = 'a' + getrawpartition();
	if ((dirp = opendir("/dev")) == 0) { *len = 0; return NULL; }
	while ((dp = readdir(dirp)) != NULL) {
		l = strlen(dp->d_name);
		if (*len < MAX_SCAN && l > 2 &&
		    (((strncmp(dp->d_name, "rsd", 3) == 0 ||
		       strncmp(dp->d_name, "rcd", 3) == 0) &&
		       isdigit(dp->d_name[l-2]) && dp->d_name[l-1] == c) ||
		    ((strncmp(dp->d_name, "enrst", 5) == 0 ||
		      strncmp(dp->d_name, "ch", 2) == 0 ||
		      strncmp(dp->d_name, "enss", 4) == 0 ||
		      strncmp(dp->d_name, "uk", 2) == 0) &&
		      isdigit(dp->d_name[l-1])))) {
			s = new char[5 + l + 1];
			strcpy(s, "/dev/");
			strcat(s, dp->d_name);
			if ((fd = open(s, O_RDWR, 0)) >= 0) {
				if (ioctl(fd, SCIOCIDENTIFY, &saddr) >= 0) {
					switch (saddr.type) {
						case TYPE_SCSI:
							scanData[*len].bus = saddr.addr.scsi.scbus;
							scanData[*len].id = saddr.addr.scsi.target;
							scanData[*len].lun = saddr.addr.scsi.lun;
							break;
						case TYPE_ATAPI:
							scanData[*len].bus = saddr.addr.atapi.atbus;
							scanData[*len].id = saddr.addr.atapi.drive;
							scanData[*len].lun = -1;
							break;
						default:
							scanData[*len].bus =
							scanData[*len].id =
							scanData[*len].lun = -1;
					}
					if (inq(fd, scanData[*len].vendor,
					            scanData[*len].product,
					            scanData[*len].revision) == 0) (*len)++;
				}
				(void)close(fd);
			}
			delete[] s;
		}
	}
	closedir(dirp);
	return scanData;
}

#include "ScsiIf-common.cc"
