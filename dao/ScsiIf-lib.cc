/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: ScsiIf-lib.cc,v $
 * Revision 1.1.1.1  2000/02/05 01:37:05  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.2  1999/04/02 16:44:30  mueller
 * Removed 'revisionDate' because it is not available in general.
 *
 * Revision 1.1  1999/03/27 20:55:18  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: ScsiIf-lib.cc,v 1.1.1.1 2000/02/05 01:37:05 llanero Exp $";

#include <config.h>

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#ifdef linux
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/../scsi/scsi.h>  /* cope with silly includes */
#include <linux/../scsi/sg.h>    /* cope with silly includes */
#endif

#include "ScsiIf.h"
#include "util.h"

#include "xconfig.h"
#include "standard.h"
#include "scg/scgcmd.h"
#include "scg/scsitransp.h"


#define MAX_DATALEN_LIMIT (63 * 1024)
#define MAX_SCAN_DATA_LEN 30

class ScsiIfImpl {
public:
  char *dev_;
  
  int maxSendLen_;
  SCSI *scgp_;

  long timeout_;

  unsigned int pageSize_;
  char *pageAlignedBuffer_;

#ifdef linux
  const char *makeDevName(int k, int do_numeric);
  const char *openScsiDevAsSg(const char *devname);
#endif
};

ScsiIf::ScsiIf(const char *dev)
{
  impl_ = new ScsiIfImpl;

  impl_->dev_ = strdupCC(dev);
  impl_->scgp_ = NULL;
  impl_->pageAlignedBuffer_ = NULL;

  vendor_[0] = 0;
  product_[0] = 0;
  revision_[0] = 0;

  set_progname("cdrdao");


#if defined(HAVE_GETPAGESIZE)
  impl_->pageSize_ = getpagesize();
#elif defined(_SC_PAGESIZE)
  impl_->pageSize_ = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE) // saw this on HPUX 9.05
  impl_->pageSize_ = sysconf(_SC_PAGE_SIZE);
#else
  impl_->pageSize_ = 0;
#endif

  if (impl_->pageSize_ == 0) {
    message(-3, "Cannot determine page size.");
    impl_->pageSize_ = 1;
  }
}

ScsiIf::~ScsiIf()
{
  if (impl_->scgp_ != NULL) {
    close_scsi(impl_->scgp_);
    impl_->scgp_ = NULL;
  }

  impl_->pageAlignedBuffer_ = NULL;
  
  delete[] impl_->dev_;
  impl_->dev_ = NULL;

  delete impl_;
}

// Opens the scsi device. Most of the code originates from cdrecord's 
// initialization function.
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed
int ScsiIf::init()
{
  char errstr[80];

#ifdef linux
  if (impl_->dev_[0] == '/' && strchr(impl_->dev_, ':') == NULL) {
    // we have a device name, check if it is a generic SCSI device or
    // try to map it a generic SCSI device
    const char *fname = impl_->openScsiDevAsSg(impl_->dev_);

    if (fname != NULL) {
      char *old = impl_->dev_;
      impl_->dev_ = strdupCC(fname);
      delete[] old;
    }
  }
#endif

  if ((impl_->scgp_ = open_scsi(impl_->dev_, errstr, sizeof(errstr), 0, 0))
	 == NULL) {
    message(-2, "Cannot open SCSI device '%s': %s", impl_->dev_, errstr);
    return 1;
  }

  impl_->timeout_ = impl_->scgp_->deftimeout;

  maxDataLen_ = scsi_bufsize(impl_->scgp_, 0);
  
  if (maxDataLen_ > MAX_DATALEN_LIMIT)
    maxDataLen_ = MAX_DATALEN_LIMIT;

  impl_->pageAlignedBuffer_ = (char *)scsi_getbuf(impl_->scgp_, maxDataLen_);

  if (impl_->pageAlignedBuffer_ == NULL) {
    message(-2, "Cannot get SCSI buffer.");
    return 1;
  }

  if (inquiry() != 0)
    return 2;

  return 0;

}

// Sets given timeout value in seconds and returns old timeout.
// return: old timeout
int ScsiIf::timeout(int t)
{
  long oldTimeout = impl_->timeout_;

  if (t > 0) 
    impl_->timeout_ = t;

  return oldTimeout;
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
  assert(cmdLen > 0 && cmdLen <= 12);

  assert(dataOutLen == 0 || dataInLen == 0);

  assert(dataOutLen <= maxDataLen_);
  assert(dataInLen <= maxDataLen_);

  struct scg_cmd *scmd = impl_->scgp_->scmd;
  int usedPageAlignedBuffer = 0;

  memset(scmd, 0, sizeof(*scmd));

  memcpy(scmd->cdb.cmd_cdb, cmd, cmdLen);
  scmd->cdb_len = cmdLen;

  if (dataOutLen > 0) {
    if (((unsigned)dataOut % impl_->pageSize_) != 0) {
      //message(0, "Use SCSI buffer for data out.");
      memcpy(impl_->pageAlignedBuffer_, dataOut, dataOutLen);
      scmd->addr = impl_->pageAlignedBuffer_;
    }
    else {
      //message(0, "Data out is page aligned.");
      scmd->addr = (char*)dataOut;
    }

    scmd->size = dataOutLen;
  }
  else if (dataInLen > 0) {
    if (((unsigned)dataIn % impl_->pageSize_) != 0) {
      //message(0, "Use SCSI buffer for data in.");
      scmd->addr = impl_->pageAlignedBuffer_;
      usedPageAlignedBuffer = 1;
    }
    else {
      //message(0, "Data in is page aligned.");
      scmd->addr = (char*)dataIn;
    }

    scmd->size = dataInLen;
    scmd->flags = SCG_RECV_DATA;
  }

  scmd->flags |= SCG_DISRE_ENA;
  scmd->timeout = impl_->timeout_;

  scmd->sense_len = CCS_SENSE_LEN;
  scmd->target = impl_->scgp_->target;
	
  impl_->scgp_->cmdname = "";
  impl_->scgp_->verbose = 0;
  impl_->scgp_->silent = showMessage ? 0 : 1;
  
  if (scsicmd(impl_->scgp_) < 0) {
    return scmd->sense_count > 0 ?  2 : 1;
  }

  if (usedPageAlignedBuffer)
    memcpy(dataIn, impl_->pageAlignedBuffer_, dataInLen);

  return 0;
}

const unsigned char *ScsiIf::getSense(int &len) const
{
  len = impl_->scgp_->scmd->sense_count;

  return impl_->scgp_->scmd->u_sense.cmd_sense;
}

void ScsiIf::printError()
{
  scsiprinterr(impl_->scgp_);
}

int ScsiIf::inquiry()
{
  unsigned char cmd[6];
  unsigned char result[0x2c];
  int i;

  cmd[0] = 0x12; // INQUIRY
  cmd[1] = cmd[2] = cmd[3] = 0;
  cmd[4] = 0x2c;
  cmd[5] = 0;


  if (sendCmd(cmd, 6, NULL, 0, result, 0x2c, 1) != 0) {
    message (-2, "Inquiry command failed on '%s'.", impl_->dev_);
    return 1;
  }

  strncpy(vendor_, (char *)(result + 0x08), 8);
  vendor_[8] = 0;

  strncpy(product_, (char *)(result + 0x10), 16);
  product_[16] = 0;

  strncpy(revision_, (char *)(result + 0x20), 4);
  revision_[4] = 0;

  for (i = 7; i >= 0 && vendor_[i] == ' '; i--) {
    vendor_[i] = 0;
  }

  for (i = 15; i >= 0 && product_[i] == ' '; i--) {
    product_[i] = 0;
  }

  for (i = 3; i >= 0 && revision_[i] == ' '; i--) {
    revision_[i] = 0;
  }

  return 0;
}

static int scanInquiry(SCSI *scgp, unsigned char *buf, ScsiIf::ScanData *sdata)
{
  scg_cmd *cmd = scgp->scmd;
  int i;

  memset(buf, 0, 36);
  memset(cmd, 0, sizeof(scg_cmd));

  cmd->cdb.g0_cdb.cmd = 0x12; // INQUIRY
  cmd->cdb.g0_cdb.count = 36;

  cmd->addr = (char*)buf;
  cmd->size = 36;
  cmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
  cmd->cdb_len = SC_G0_CDBLEN;
  cmd->sense_len = CCS_SENSE_LEN;
  scgp->silent = 1;
  scgp->cmdname = "inquiry";

  if (scsicmd(scgp) == 0) {
    if ((buf[0] & 0x1f) == 0x04 || (buf[0] & 0x1f) == 0x05) {
      sdata->bus = scgp->scsibus;
      sdata->id = scgp->target;
      sdata->lun = scgp->lun;

      strncpy(sdata->vendor, (char *)(buf + 0x08), 8);
      sdata->vendor[8] = 0;

      strncpy(sdata->product, (char *)(buf + 0x10), 16);
      sdata->product[16] = 0;

      strncpy(sdata->revision, (char *)(buf + 0x20), 4);
      sdata->revision[4] = 0;

      for (i = 7; i >= 0 && sdata->vendor[i] == ' '; i--) {
	sdata->vendor[i] = 0;
      }

      for (i = 15; i >= 0 && sdata->product[i] == ' '; i--) {
	sdata->product[i] = 0;
      }

      for (i = 3; i >= 0 && sdata->revision[i] == ' '; i--) {
	sdata->revision[i] = 0;
      }

      return 1;
    }
  }

  return 0;
}

ScsiIf::ScanData *ScsiIf::scan(int *len)
{
  ScanData *sdata = NULL;
  SCSI *scgp;
  unsigned char *buf;
  char errstr[80];

  *len = 0;

  set_progname("cdrdao");

  if ((scgp = open_scsi(NULL, errstr, sizeof(errstr), 0, 0)) == NULL)
    return NULL;

  // allocate buffer for inquiry data
  if ((buf = (unsigned char *)scsi_getbuf(scgp, 100)) == NULL) {
    close_scsi(scgp);
    return NULL;
  }

  sdata = new ScanData[MAX_SCAN_DATA_LEN];
  
  for (scgp->scsibus = 0; scgp->scsibus < 16 && *len < MAX_SCAN_DATA_LEN;
       scgp->scsibus++) {

    if (scsi_havebus(scgp, scgp->scsibus)) {
      scgp->lun = 0;

      for (scgp->target=0; scgp->target < 16 && *len < MAX_SCAN_DATA_LEN;
	   scgp->target++) {
	if (scanInquiry(scgp, buf, &(sdata[*len])))
	  *len += 1;
      }
    }
  }
  
  
  close_scsi(scgp);

  return sdata;
}

#include "ScsiIf-common.cc"

#ifdef linux

/* Function for mapping any SCSI device to the corresponding SG device.
 * Taken from D. Gilbert's example code.
 */

#define MAX_SG_DEVS 26

#define SCAN_ALPHA 0
#define SCAN_NUMERIC 1
#define DEF_SCAN SCAN_ALPHA

const char *ScsiIfImpl::makeDevName(int k, int do_numeric)
{
  static char filename[100];
  char buf[20];

  strcpy(filename, "/dev/sg");

  if (do_numeric) {
    sprintf(buf, "%d", k);
    strcat(filename, buf);
  }
  else {
    if (k <= 26) {
      buf[0] = 'a' + (char)k;
      buf[1] = '\0';
      strcat(filename, buf);
    }
    else {
      strcat(filename, "xxxx");
    }
  }

  return filename;
}

const char *ScsiIfImpl::openScsiDevAsSg(const char *devname)
{
  int fd, bus, bbus, k;
  int do_numeric = DEF_SCAN;
  const char *fname = devname;
  struct {
    int mux4;
    int hostUniqueId;
  } m_idlun, mm_idlun;

  if ((fd = open(fname, O_RDONLY | O_NONBLOCK)) < 0) {
    if (EACCES == errno)
      fd = open(fname, O_RDWR | O_NONBLOCK);
  }

  if (fd < 0) {
    message(-2, "Cannot open \"%s\": %s", fname, strerror(errno));
    return NULL;
  }

  if (ioctl(fd, SG_GET_TIMEOUT, 0) < 0) { /* not a sg device ? */
    if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &bus) < 0) {
      message(-2, "%s: Need a filename that resolves to a SCSI device.",
	      fname);
      close(fd);
      return NULL;
    }
    if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &m_idlun) < 0) {
      message(-2, "%s: Need a filename that resolves to a SCSI device (2).",
	      fname);
      close(fd);
      return NULL;
    }
    close(fd);
    
    for (k = 0; k < MAX_SG_DEVS; k++) {
      fname = makeDevName(k, do_numeric);
      if ((fd = open(fname, O_RDONLY | O_NONBLOCK)) < 0) {
	if (EACCES == errno) 
	  fd = open(fname, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
	  if ((ENOENT == errno) && (0 == k) && (do_numeric == DEF_SCAN)) {
	    do_numeric = ! DEF_SCAN;
	    fname = makeDevName(k, do_numeric);
	    if ((fd = open(fname, O_RDONLY | O_NONBLOCK)) < 0) {
	      if (EACCES == errno) 
		fd = open(fname, O_RDWR | O_NONBLOCK);
	    }
	  }
	  if (fd < 0) {
	    if (EBUSY == errno)
	      continue;  /* step over if O_EXCL already on it */
	    else
	      break;
	  }
	}
      }
      if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &bbus) < 0) {
	message(-2, "%s: SG: ioctl SCSI_IOCTL_GET_BUS_NUMBER failed: %s",
		fname, strerror(errno));
	close(fd);
	fd = -9999;
      }
      if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &mm_idlun) < 0) {
	message(-2, "%s: SG: ioctl SCSI_IOCTL_GET_IDLUN failed: %s", 
		fname, strerror(errno));
	close(fd);
	fd = -9999;
      }
      if ((bus == bbus) && 
	  ((m_idlun.mux4 & 0xff) == (mm_idlun.mux4 & 0xff)) &&
	  (((m_idlun.mux4 >> 8) & 0xff) == 
	   ((mm_idlun.mux4 >> 8) & 0xff)) &&
	  (((m_idlun.mux4 >> 16) & 0xff) == 
	   ((mm_idlun.mux4 >> 16) & 0xff))) {
	message(3, "Mapping %s to sg device: %s", devname, fname);
	break;
      }
      else {
	close(fd);
	fd = -9999;
      }
    }
  }

  if (fd >= 0) {
    close(fd);
    return fname;
  }
  else {
    message(-2, "Cannot map \"%s\" to a SG device.", devname);
    return NULL;
  }
}

#endif /* linux */
