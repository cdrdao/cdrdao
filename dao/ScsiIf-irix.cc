/*
 * Native implementation of cdrdao's SCSI interface for Irix 6.
 * Copyright (C) by Edgar Fuﬂ, Bonn, May 2003, July 2007.
 * Do with this whatever you like, as long as you are either me or you keep
 * this message intact and both
 * - acknowledge that I wrote it for cdrdao in the first place, and
 * - don't blame me if it doesn't do what you like or expect.
 * These routines do exactly what they do. If that's not what you expect them
 * or would like them to do, don't complain with me, the cdrdao project, my
 * neighbour's brother-in-law or anybody else, but rewrite them to your taste.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/dsreq.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "ScsiIf.h"

extern void message(int level, const char *fmt, ...);

#include "decodeSense.cc"

#define MAX_SCAN 32
#define SENSE_MAX 256

class ScsiIfImpl {
	public:
	char *name_;
	int fd_;
	long timeout_; /* in ms */
	struct dsreq dsreq_;
	char *error_;
	unsigned char sensebuf_[SENSE_MAX];
};

ScsiIf::ScsiIf(const char *name)
{
#define PREFIX "/dev/scsi/"
#define PREFIX_SC "/dev/scsi/sc"
	int len;
	int bus, targ, lun, count;

	impl_ = new ScsiIfImpl;
	len = strlen(name);
	if (len == 0) {
		impl_->name_ = NULL;
	} else if (sscanf(name, "%i,%i,%i%n", &bus, &targ, &lun, &count) == 3 &&
	           count == len) {
		if ((bus < 0) || (targ < 0) || (lun < 0))
			impl_->name_ = NULL;
		else {
			impl_->name_ = new char[strlen(PREFIX_SC) + len + 1];
			sprintf(impl_->name_, "%s%dd%dl%d", PREFIX_SC, bus, targ, lun);
		}
	} else if (strncmp(name, "/", 1) == 0 ||
	           strncmp(name, "./", 2) == 0 ||
	           strncmp(name, "../", 3) == 0) {
		impl_->name_ = new char[len + 1];
		strcpy(impl_->name_, name);
	} else {
		impl_->name_ = new char[strlen(PREFIX) + len + 1];
		strcpy(impl_->name_, PREFIX);
		strcat(impl_->name_, name);
	}
	impl_->fd_ = -1;
	impl_->timeout_ = 10*1000;
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
	impl_->dsreq_.ds_cmdbuf = NULL; impl_->dsreq_.ds_cmdlen = 0;
	impl_->dsreq_.ds_databuf = NULL; impl_->dsreq_.ds_datalen = 0;
	impl_->dsreq_.ds_flags = 0;
	impl_->dsreq_.ds_time = impl_->timeout_;
	if (cmdLen == 0) ERROR("cmdLen == 0");
	if (cmdLen > 255) ERROR("cmdLen > 255");
	impl_->dsreq_.ds_cmdbuf = (caddr_t)cmd;
	impl_->dsreq_.ds_cmdlen = cmdLen;
	if (dataOut != NULL && dataIn != NULL) {
		ERROR("Out and In");
	} else if (dataOut != NULL) {
		if (dataOutLen == 0) ERROR("dataOutLen == 0");
		if (dataInLen != 0) ERROR("dataInLen != 0");
		impl_->dsreq_.ds_flags |= DSRQ_WRITE;
		impl_->dsreq_.ds_databuf = (caddr_t)dataOut;
		impl_->dsreq_.ds_datalen = dataOutLen;
	} else if (dataIn != NULL) {
		if (dataInLen == 0) ERROR("dataInLen == 0");
		if (dataOutLen !=0) ERROR("dataOutLen != 0");
		impl_->dsreq_.ds_flags |= DSRQ_READ;
		impl_->dsreq_.ds_databuf = (caddr_t)dataIn;
		impl_->dsreq_.ds_datalen = dataInLen;
	} else {
		if (dataOutLen !=0 || dataInLen != 0) ERROR("dataLen != 0");
		impl_->dsreq_.ds_databuf = NULL;
		impl_->dsreq_.ds_datalen = 0;
	}
	impl_->dsreq_.ds_senselen = SENSE_MAX;
	impl_->dsreq_.ds_sensebuf = (caddr_t)impl_->sensebuf_;
	impl_->dsreq_.ds_flags |= DSRQ_SENSE;
	if (ioctl(impl_->fd_, DS_ENTER, &impl_->dsreq_) < 0) {
		char str[80];
		strcpy(str, "DS_ENTER: "); strcat(str, strerror(errno));
		ERROR(str);
	}
	if (impl_->dsreq_.ds_ret == 0 ||
	    impl_->dsreq_.ds_ret == DSRT_OK ||
	    impl_->dsreq_.ds_ret == DSRT_SHORT)
		return 0;
	else if (impl_->dsreq_.ds_ret == DSRT_SENSE)  {
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
	len = impl_->dsreq_.ds_sensesent;
	return impl_->sensebuf_;
}

void ScsiIf::printError()
{
	if (impl_->dsreq_.ds_cmdbuf != NULL) {
		char s[80];
		char *p = s;
		int i;
		p += snprintf(p, s + sizeof(s) - p, "CDB=");
		for (i = 0; i < impl_->dsreq_.ds_cmdlen; i++) {
			p += snprintf(p, s + sizeof(s) - p, "%.2X ", impl_->dsreq_.ds_cmdbuf[i]);
		}
		p[-1] = ',';
		switch (impl_->dsreq_.ds_flags & (DSRQ_READ | DSRQ_WRITE)) {
			case DSRQ_READ: p += snprintf(p, s + sizeof(s) - p, " RD"); break;
			case DSRQ_WRITE: p += snprintf(p, s + sizeof(s) - p, " WR"); break;
			case DSRQ_READ | DSRQ_WRITE: p += snprintf(p, s + sizeof(s) - p, " RW"); break;
		}
		p += snprintf(p, s + sizeof(s) - p, ", BUF=%p", impl_->dsreq_.ds_databuf);
		p += snprintf(p, s + sizeof(s) - p, ", LEN=%lu", impl_->dsreq_.ds_datalen);
		p += snprintf(p, s + sizeof(s) - p, ", TO=%lu", impl_->dsreq_.ds_time);
		message(-2, s);
	}
	if (impl_->error_ != NULL) {
		message(-2, impl_->error_);
	} else {
		switch (impl_->dsreq_.ds_ret) {
			case DSRT_DEVSCSI: message(-2,"devscsi failure"); break;
			case DSRT_MULT:    message(-2,"request rejected"); break;
			case DSRT_CANCEL:  message(-2,"request cancelled"); break;
			case DSRT_REVCODE: message(-2,"obsolete"); break;
			case DSRT_AGAIN:   message(-2,"try again"); break;
			case DSRT_HOST:    message(-2,"host failure"); break;
			case DSRT_NOSEL:   message(-2,"no select"); break;
			case DSRT_SHORT:   message(-2,"short transfer"); break;
			case DSRT_OK:      message(-2,"complete transfer"); break;
			case DSRT_SENSE:   /* message(-2,"sense"); */
				decodeSense(impl_->sensebuf_,
				            impl_->dsreq_.ds_sensesent);
				break;
			case DSRT_NOSENSE: message(-2,"sense error"); break;
			case DSRT_TIMEOUT: message(-2,"timeout"); break;
			case DSRT_LONG:    message(-2,"overrun"); break;
			case DSRT_PROTO:   message(-2,"protocol failure"); break;
			case DSRT_EBSY:    message(-2,"busy lost"); break;
			case DSRT_REJECT:  message(-2,"mesage reject"); break;
			case DSRT_PARITY:  message(-2,"parity error"); break;
			case DSRT_MEMORY:  message(-2,"memory error"); break;
			case DSRT_CMDO:    message(-2,"command error"); break;
			case DSRT_STAI:    message(-2,"status error"); break;
			case DSRT_UNIMPL:  message(-2,"not implemented"); break;
			default: message(-2, "undefined ds_ret"); break;
		}
		switch (impl_->dsreq_.ds_status) {
			case 0x00: message(-2, "GOOD"); break;
			case 0x02: message(-2, "CHECK CONDITION"); break;
			case 0x04: message(-2, "CONDITION MET"); break;
			case 0x08: message(-2, "BUSY"); break;
			case 0x10: message(-2, "INTERMEDIATE"); break;
			case 0x14: message(-2, "INTERMEDIATE, CONDITION MET"); break;
			case 0x18: message(-2, "RESERVATION CONFLICT"); break;
			case 0x22: message(-2, "COMMAND TERMINATED"); break;
			case 0x28: message(-2, "QUEUE FULL"); break;
			case 0xff: break;
			default: message(-2, "undefined status");
		}
	}
}

int inq(int fd, void *sensebuf, int senselen, char *vend, char *prod, char *rev)
{
	char buf[44];
	char cmd[] = {0x12, 0, 0, 0, sizeof(buf), 0};
	struct dsreq dsreq = {
		/* flags */ DSRQ_READ,
		/* time */ 1000,
		/* private */ 0,
		/* cmdbuf */ (caddr_t)cmd,
		/* cmdlen */ sizeof(cmd),
		/* databuf */ (caddr_t)buf,
		/* datalen */ sizeof(buf),
		/* sensebuf */ (caddr_t)sensebuf,
		/* senselen */ senselen,
		/* iovbuf */ NULL,
		/* iovlen */ 0,
		/* link */ NULL,
		/* sync */ 0,
		/* revcode */ 0,
		/* ret */ 0,
		/* status */ 0,
		/* msg */ 0,
		/* cmdsent */ 0,
		/* datasent */ 0,
		/* sensesent */ 0
	};
	char *p, *q;

	if (sensebuf != NULL && senselen > 0)
		dsreq.ds_flags |= DSRQ_SENSE;
	if (ioctl(fd, DS_ENTER, &dsreq) < 0 ||
	    (dsreq.ds_ret != 0 && dsreq.ds_ret != DSRT_OK &&
	    dsreq.ds_ret != DSRT_SHORT)) {
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
	return inq(impl_->fd_, impl_->sensebuf_, SENSE_MAX, vendor_, product_, revision_);
}

ScsiIf::ScanData *ScsiIf::scan(int *len)
{
	DIR *dirp;
	struct dirent *dp;
	int l;
	int fd;
	ScanData *scanData;
	char s[35];
	int bus, targ, lun, count;

	scanData = new ScanData[MAX_SCAN]; *len = 0;
	if ((dirp = opendir("/dev/scsi")) == 0) { *len = 0; return NULL; }
	while ((dp = readdir(dirp)) != NULL) {
		l = strlen(dp->d_name);
		if (*len < MAX_SCAN && l > 2 &&
		    sscanf(dp->d_name, "sc%dd%dl%d%n", &bus, &targ, &lun, &count) == 3 &&
		    count == l) {
			scanData[*len].bus = bus;
			scanData[*len].id = targ;
			scanData[*len].lun = lun;
			strcpy(s, "/dev/scsi/");
			strcat(s, dp->d_name);
			if ((fd = open(s, O_RDWR, 0)) >= 0) {
				if (inq(fd, NULL, 0,
				    scanData[*len].vendor,
				    scanData[*len].product,
				    scanData[*len].revision) == 0) (*len)++;
			}
			(void)close(fd);
		}
	}
	closedir(dirp);
	return scanData;
}

#include "ScsiIf-common.cc"
