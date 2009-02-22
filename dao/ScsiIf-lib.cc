/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include "ScsiIf.h"
#include "log.h"
#include "util.h"

#include "xconfig.h"

#include "standard.h"
#include "scg/scgcmd.h"
#include "scg/scsitransp.h"
#include "scg/scsireg.h"

// schily/standard.h define these, i don't know what they smoked the
// day they came up with those defines.
#undef vendor
#undef product
#undef revision

static void printVersionInfo(SCSI *scgp);

#define MAX_DATALEN_LIMIT (63 * 1024)
#define MAX_SCAN_DATA_LEN 30

static int VERSION_INFO_PRINTED = 0;

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

  //set_progname("cdrdao");


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
    log_message(-3, "Cannot determine page size.");
    impl_->pageSize_ = 1;
  }

}

ScsiIf::~ScsiIf()
{
  if (impl_->scgp_ != NULL) {
    scg_close(impl_->scgp_);
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

  if ((impl_->scgp_ = scg_open(impl_->dev_, errstr, sizeof(errstr), 0, 0))
	 == NULL) {
    log_message(-2, "Cannot open SCSI device '%s': %s", impl_->dev_, errstr);
    return 1;
  }

  impl_->timeout_ = impl_->scgp_->deftimeout;

  maxDataLen_ = scg_bufsize(impl_->scgp_, MAX_DATALEN_LIMIT);

  log_message(5, "SCSI: max DMA: %ld", maxDataLen_);

  if (maxDataLen_ > MAX_DATALEN_LIMIT)
    maxDataLen_ = MAX_DATALEN_LIMIT;

  impl_->pageAlignedBuffer_ = (char *)scg_getbuf(impl_->scgp_, maxDataLen_);

  if (impl_->pageAlignedBuffer_ == NULL) {
    log_message(-2, "Cannot get SCSI buffer.");
    return 1;
  }

  printVersionInfo(impl_->scgp_);

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
    if (((size_t)dataOut % impl_->pageSize_) != 0) {
      //log_message(0, "Use SCSI buffer for data out.");
      memcpy(impl_->pageAlignedBuffer_, dataOut, dataOutLen);
      scmd->addr = impl_->pageAlignedBuffer_;
    }
    else {
      //log_message(0, "Data out is page aligned.");
      scmd->addr = (char*)dataOut;
    }

    scmd->size = dataOutLen;
  }
  else if (dataInLen > 0) {
    if (((size_t)dataIn % impl_->pageSize_) != 0) {
      //log_message(0, "Use SCSI buffer for data in.");
      scmd->addr = impl_->pageAlignedBuffer_;
      usedPageAlignedBuffer = 1;
    }
    else {
      //log_message(0, "Data in is page aligned.");
      scmd->addr = (char*)dataIn;
    }

    scmd->size = dataInLen;
    scmd->flags = SCG_RECV_DATA;
  }

  scmd->flags |= SCG_DISRE_ENA;
  scmd->timeout = impl_->timeout_;

  scmd->sense_len = CCS_SENSE_LEN;
  //scmd->target = impl_->scgp_->target;
	
  impl_->scgp_->cmdname = (char*)" ";
  impl_->scgp_->verbose = 0;
  impl_->scgp_->silent = 1;
  
  if (scg_cmd(impl_->scgp_) < 0) {
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
  scg_printerr(impl_->scgp_);
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


  if (sendCmd(cmd, 6, NULL, 0, result, 0x2c, 0) != 0) {
    log_message(-2, "Inquiry command failed on '%s'.", impl_->dev_);
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
  struct scg_cmd *cmd = scgp->scmd;
  int i;

  int dev_sense = scg_sense_key(scgp);

  memset(buf, 0, 36);
  memset(cmd, 0, sizeof(struct scg_cmd));

  cmd->cdb.g0_cdb.cmd = 0x12; // INQUIRY
  cmd->cdb.g0_cdb.count = 36;

  cmd->addr = (char*)buf;
  cmd->size = 36;
  cmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
  cmd->cdb_len = SC_G0_CDBLEN;
  cmd->sense_len = CCS_SENSE_LEN;
  scgp->silent = 1;
  scgp->cmdname = (char*)"inquiry";

#ifdef SCSI_ATAPI
  int is_atapi = scg_isatapi(scgp);
#else
  int is_atapi = 0;
#endif
  int cmd_status = scg_cmd(scgp);

  if (cmd_status == 0 || (is_atapi && dev_sense >= 0)) {
    struct scsi_inquiry* inq = (struct scsi_inquiry*)buf;
    if (inq->type == INQ_OPTD || inq->type == INQ_ROMD) {
      char buf[16];
      sprintf(buf, "%d,%d,%d", scg_scsibus(scgp), scg_target(scgp),
              scg_lun(scgp));
      sdata->dev +=  buf;

      strncpy(sdata->vendor, inq->vendor_info, 8);
      sdata->vendor[8] = 0;
      strncpy(sdata->product, inq->prod_ident, 16);
      sdata->product[16] = 0;
      strncpy(sdata->revision, inq->prod_revision, 4);
      sdata->revision[4] = 0;

      return 1;
    }
  }

  return 0;
}

ScsiIf::ScanData *ScsiIf::scan(int *len, char* scsi_dev)
{
  ScanData *sdata = NULL;
  SCSI *scgp;
  unsigned char *buf;
  char errstr[80];

  *len = 0;

  if ((scgp = scg_open(scsi_dev, errstr, sizeof(errstr), 0, 0)) == NULL)
    return NULL;

  printVersionInfo(scgp);

  // allocate buffer for inquiry data
  if ((buf = (unsigned char *)scg_getbuf(scgp, 100)) == NULL) {
    scg_close(scgp);
    return NULL;
  }

  sdata = new ScanData[MAX_SCAN_DATA_LEN];
  
  for (int bus = 0; bus < 16 && *len < MAX_SCAN_DATA_LEN; bus++) {

    if (scg_havebus(scgp, bus)) {
      int lun = 0;

      for (int target=0; target < 16 && *len < MAX_SCAN_DATA_LEN; target++) {
	scg_settarget(scgp, bus, target, lun);
        if (scsi_dev) {
          sdata[*len].dev = scsi_dev;
          sdata[*len].dev += ":";
        }
	if (scanInquiry(scgp, buf, &(sdata[*len])))
	  *len += 1;
      }
    }
  }
  
  
  scg_close(scgp);

  return sdata;
}

static void printVersionInfo(SCSI *scgp)
{
  if (!VERSION_INFO_PRINTED) {
    VERSION_INFO_PRINTED = 1;

    const char *vers = scg_version(0, SCG_VERSION);
    const char *auth = scg_version(0, SCG_AUTHOR);
    log_message(3, "Using libscg version '%s-%s'", auth, vers);
  
    vers = scg_version(scgp, SCG_VERSION);
    auth = scg_version(scgp, SCG_AUTHOR);
  
    log_message(3, "Using libscg transport code version '%s-%s'", auth, vers);

    vers = scg_version(scgp, SCG_RVERSION);
    auth = scg_version(scgp, SCG_RAUTHOR);

    if (vers != NULL && auth != NULL) {
      log_message(3, "Using remote transport code version '%s-%s'", auth, vers);
    }

    log_message(2, "");
  }
}

const int ScsiIf::bus ()
{
	return scg_scsibus (this->impl_->scgp_);
}

const int ScsiIf::id ()
{
	return scg_target (this->impl_->scgp_);
}

const int ScsiIf::lun ()
{
	return scg_lun (this->impl_->scgp_);
}

#include "ScsiIf-common.cc"

#ifdef linux

#include <sys/ioctl.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

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
    log_message(-2, "Cannot open \"%s\": %s", fname, strerror(errno));
    return NULL;
  }

  if (ioctl(fd, SG_GET_TIMEOUT, 0) < 0) { /* not a sg device ? */

#ifdef SCSI_IOCTL_GET_BUS_NUMBER
    if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &bus) < 0) {
      log_message(-2, "%s: Need a filename that resolves to a SCSI device.",
	      fname);
      close(fd);
      return NULL;
    }
#else
    bus = 0;
#endif


    if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &m_idlun) < 0) {
      log_message(-2, "%s: Need a filename that resolves to a SCSI device (2).",
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

#ifdef SCSI_IOCTL_GET_BUS_NUMBER
      if (ioctl(fd, SCSI_IOCTL_GET_BUS_NUMBER, &bbus) < 0) {
	log_message(-2, "%s: SG: ioctl SCSI_IOCTL_GET_BUS_NUMBER failed: %s",
		fname, strerror(errno));
	close(fd);
	fd = -9999;
      }
#else
      bbus = 0;
#endif

      if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &mm_idlun) < 0) {
	log_message(-2, "%s: SG: ioctl SCSI_IOCTL_GET_IDLUN failed: %s",
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
	log_message(4, "Mapping %s to sg device: %s", devname, fname);
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
    log_message(-2, "Cannot map \"%s\" to a SG device.", devname);
    return NULL;
  }
}

#endif
