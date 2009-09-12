/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002 Andreas Mueller <andreas@daneb.de>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include "CdrDriver.h"
#include "PWSubChannel96.h"
#include "Toc.h"
#include "util.h"
#include "log.h"
#include "CdTextItem.h"
#include "data.h"
#include "port.h"

// all drivers
#include "CDD2600.h"
#include "PlextorReader.h"
#include "PlextorReaderScan.h"
#include "GenericMMC.h"
#include "GenericMMCraw.h"
#include "RicohMP6200.h"
#include "TaiyoYuden.h"
#include "YamahaCDR10x.h"
#include "TeacCdr55.h"
#include "SonyCDU920.h"
#include "SonyCDU948.h"
#include "ToshibaReader.h"

// Paranoia DAE related
#include "cdda_interface.h"
#include "../paranoia/cdda_paranoia.h"


typedef CdrDriver *(*CdrDriverConstructor)(ScsiIf *, unsigned long);

struct DriverSelectTable {
  const char *driverId;
  const char *vendor;
  const char *model;
  unsigned long options;
  struct DriverSelectTable *next;
};

struct DriverTable {
  const char *driverId;
  CdrDriverConstructor constructor;
};

static DriverSelectTable *READ_DRIVER_TABLE = NULL;
static DriverSelectTable *WRITE_DRIVER_TABLE = NULL;

static DriverSelectTable BUILTIN_READ_DRIVER_TABLE[] = {
{ "generic-mmc", "ASUS", "CD-S340", 0, NULL },
{ "generic-mmc", "ASUS", "CD-S400", 0, NULL },
{ "generic-mmc", "ASUS", "CD-S500/A", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "ASUS", "DVD-ROM E608", 0, NULL },
{ "generic-mmc", "E-IDE", "CD-950E/TKU", 0, NULL },
{ "generic-mmc", "E-IDE", "CD-ROM 36X/AKU", 0, NULL },
{ "generic-mmc", "E-IDE", "CD-ROM 52X/AKH", 0, NULL },
{ "generic-mmc", "FUNAI", "E295X", 0, NULL },
{ "generic-mmc", "Goldstar", "CD-RW CED-8042B", 0, NULL },
{ "generic-mmc", "HITACHI", "CDR-7730", 0, NULL },
{ "generic-mmc", "HITACHI", "CDR-8435", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "LG", "CD-ROM CRD-8480C", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "LG", "CD-ROM CRD-8482B", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "LG", "CD-ROM CRD-8521B", 0, NULL },
{ "generic-mmc", "LG", "DVD-ROM DRN8080B", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "generic-mmc", "LITE-ON", "CD-ROM", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "generic-mmc", "LITE-ON", "LTD-163", 0, NULL },
{ "generic-mmc", "LITEON", "DVD-ROM LTD163D", 0, NULL },
{ "generic-mmc", "MATSHITA", "CD-ROM CR-588", 0, NULL },
{ "generic-mmc", "MATSHITA", "CD-ROM CR-589", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "MATSHITA", "DVD-ROM SR-8585", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "MEMOREX", "CD-233E", 0, NULL },
{ "generic-mmc", "MITSUMI", "CD-ROM FX4820", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "OPTICS_S", "8622", 0, NULL },
{ "generic-mmc", "PHILIPS", "36X/AKU", 0, NULL },
{ "generic-mmc", "PHILIPS", "CD-ROM PCCD052", 0, NULL },
{ "generic-mmc", "PHILIPS", "E-IDE CD-ROM 36X", 0, NULL },
{ "generic-mmc", "PIONEER", "CD-ROM DR-U32", OPT_DRV_GET_TOC_GENERIC|OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "PIONEER", "DVD-103", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "PIONEER", "DVD-104", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "PIONEER", "DVD-105", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "SONY", "CD-ROM CDU31A-02", 0, NULL },
{ "generic-mmc", "SONY", "CD-ROM CDU4821", 0, NULL },
{ "generic-mmc", "SONY", "CDU5211", 0, NULL },
{ "generic-mmc", "TEAC", "CD-524E", OPT_DRV_GET_TOC_GENERIC|OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "TEAC", "CD-532E", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc", "TEAC", "CD-540E", 0, NULL },
{ "generic-mmc", "TOSHIBA", "CD-ROM XM-3206B", 0, NULL },
{ "generic-mmc", "TOSHIBA", "CD-ROM XM-6102B", 0, NULL },
{ "generic-mmc", "TOSHIBA", "CD-ROM XM-6302B", 0, NULL },
{ "generic-mmc", "TOSHIBA", "CD-ROM XM-6402B", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-C2202", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-C2302", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-C2402", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-M1102", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-M1401", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-M1402", 0, NULL },
{ "plextor", "HITACHI", "DVD-ROM GD-2500", 0, NULL },
{ "plextor", "MATSHITA", "CD-ROM CR-506", OPT_PLEX_DAE_D4_12, NULL },
{ "plextor", "MATSHITA", "CR-8008", 0, NULL },
{ "plextor", "NAKAMICH", "MJ-5.16S", 0, NULL },
{ "plextor", "PIONEER", "CD-ROM DR-U03", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "plextor", "PIONEER", "CD-ROM DR-U06", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "plextor", "PIONEER", "CD-ROM DR-U10", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "plextor", "PIONEER", "CD-ROM DR-U12", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "plextor", "PIONEER", "CD-ROM DR-U16", OPT_DRV_GET_TOC_GENERIC, NULL },
{ "plextor", "PIONEER", "DVD-303", 0, NULL },
{ "plextor", "PIONEER", "DVD-305", 0, NULL },
{ "plextor", "SAF", "CD-R2006PLUS", 0, NULL },
{ "plextor", "SONY", "CD-ROM", 0, NULL },
{ "plextor", "SONY", "CD-ROM CDU-76", 0, NULL },
{ "plextor", "TOSHIBA", "XM-5401", 0, NULL },
{ "plextor-scan", "PLEXTOR", "CD-ROM", 0, NULL },
{ "plextor-scan", "PLEXTOR", "PX-40TS", 0, NULL },
{ "plextor-scan", "PLEXTOR", "PX-40TW", 0, NULL },
{ "plextor-scan", "PLEXTOR", "PX-63", 0, NULL },
{ "plextor-scan", "TEAC", "CD-ROM CD-532S", OPT_PLEX_USE_PQ|OPT_PLEX_PQ_BCD, NULL },
{ "teac-cdr55", "TEAC", "CD-532S", 0, NULL },
{ "toshiba", "TOSHIBA", "1504", 0, NULL },
{ "toshiba", "TOSHIBA", "CD-ROM XM-3601B", 0, NULL },
{ "toshiba", "TOSHIBA", "CD-ROM XM-5302TA", 0, NULL },
{ "toshiba", "TOSHIBA", "CD-ROM XM-5701TA", 0, NULL },
{ "toshiba", "TOSHIBA", "CD-ROM XM-6201TA", 0, NULL },
{ "toshiba", "TOSHIBA", "CD-ROM XM-6401TA", 0, NULL },
{ "toshiba", "TOSHIBA", "DVD-ROM SD-2102", 0, NULL },
{ NULL, NULL, NULL, 0, NULL }};

static DriverSelectTable BUILTIN_WRITE_DRIVER_TABLE[] = {
{ "cdd2600", "GRUNDIG", "CDR100IPW", 0, NULL },
{ "cdd2600", "HP", "CD-Writer 4020", 0, NULL },
{ "cdd2600", "HP", "CD-Writer 6020", 0, NULL },
{ "cdd2600", "IMS", "522", 0, NULL },
{ "cdd2600", "IMS", "CDD2000", 0, NULL },
{ "cdd2600", "KODAK", "PCD-225", 0, NULL },
{ "cdd2600", "PHILIPS", "CDD2000", 0, NULL },
{ "cdd2600", "PHILIPS", "CDD2600", 0, NULL },
{ "cdd2600", "PHILIPS", "CDD522", 0, NULL },
{ "generic-mmc", "AOPEN", "CD-RW CRW1632", 0, NULL },
{ "generic-mmc", "AOPEN", "CD-RW CRW2040", 0, NULL },
{ "generic-mmc", "AOPEN", "CD-RW-241040", 0, NULL },
{ "generic-mmc", "AOPEN", "CRW9624", 0, NULL },
{ "generic-mmc", "CD-RW", "CDR-2440MB", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "CREATIVE", "CD-RW RW1210E", 0, NULL },
{ "generic-mmc", "CREATIVE", "CD-RW RW4424", 0, NULL },
{ "generic-mmc", "CREATIVE", "CD-RW RW8433E", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "CREATIVE", "CD5233E", 0, NULL },
{ "generic-mmc", "DELTA", "OME-W141", 0, NULL },
{ "generic-mmc", "GENERIC", "CRD-BP1600P", 0, NULL },
{ "generic-mmc", "GENERIC", "CRD-R800S", 0, NULL },
{ "generic-mmc", "GENERIC", "CRD-RW2", 0, NULL },
{ "generic-mmc", "HL-DT-ST", "RW/DVD GCC-4120B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "9510i", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 7570", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 8100", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 8200", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 8290", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 9100", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 9110", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 9200", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 9300", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 9600", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "CD-Writer+ 9700", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "HP", "DVD Writer 100j", 0, NULL },
{ "generic-mmc", "IDE-CD", "R/RW 16x10A", 0, NULL },
{ "generic-mmc", "IMATION", "IMW121032IAB", 0, NULL },
{ "generic-mmc", "LG", "8088B", 0, NULL },
{ "generic-mmc", "LG", "8120B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "LG", "CD-ROM CDR-8428B", 0, NULL },
{ "generic-mmc", "LG", "CD-RW CED-8080B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "LG", "CD-RW CED-8081B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "LG", "CD-RW CED-8083B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "LG", "CD-RW GCE-8240B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "LG", "COMBO", 0, NULL },
{ "generic-mmc", "LG", "HL-DT-ST RW/DVD GCC-4080N", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "LITE-ON", "LTR-0841", OPT_MMC_CD_TEXT|OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "LITE-ON", "LTR-24102B", 0, NULL },
{ "generic-mmc", "LITE-ON", "LTR-32125W", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "MATSHITA", "CD-R   CW-7502", 0, NULL },
{ "generic-mmc", "MATSHITA", "CD-R   CW-7503", 0, NULL },
{ "generic-mmc", "MATSHITA", "CD-R   CW-7582", 0, NULL },
{ "generic-mmc", "MATSHITA", "CD-R   CW-7585", 0, NULL },
{ "generic-mmc", "MATSHITA", "CD-R   CW-7586", 0, NULL },
{ "generic-mmc", "MATSHITA", "CDRRW01", 0, NULL },
{ "generic-mmc", "MATSHITA", "UJDA360", 0, NULL },
{ "generic-mmc", "MATSHITA", "UJDA710", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "MATSHITA", "UJDA720", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "MEMOREX", "24MAX 1040", 0, NULL },
{ "generic-mmc", "MEMOREX", "40MAXX 1248AJ", 0, NULL },
{ "generic-mmc", "MEMOREX", "CD-RW4224", 0, NULL },
{ "generic-mmc", "MICROSOLUTIONS", "BACKPACK CD REWRITER", 0, NULL },
{ "generic-mmc", "MITSUMI", "CR-4801", 0, NULL },
{ "generic-mmc", "MITSUMI", "CR-48X5", 0, NULL },
{ "generic-mmc", "MITSUMI", "CR-48X5TE", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "MITSUMI", "CR-48X8TE", 0, NULL },
{ "generic-mmc", "MITSUMI", "CR-48XATE", 0, NULL },
{ "generic-mmc", "OLYMPIC", "RWD RW4224", OPT_DRV_GET_TOC_GENERIC|OPT_MMC_USE_PQ, NULL },
{ "generic-mmc", "PANASONIC", "CD-R   CW-7582", 0, NULL },
{ "generic-mmc", "PHILIPS", "CDRW1610A", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PHILIPS", "CDRW2412A", 0, NULL },
{ "generic-mmc", "PHILIPS", "PCA460RW", 0, NULL },
{ "generic-mmc", "PIONEER", "DVD-ROM DVD-114", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-R412", OPT_MMC_USE_PQ|OPT_MMC_READ_ISRC, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-R820", 0, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-W1210", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-W124", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-W1610", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-W4220", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-W8220", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R   PX-W8432", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R PX-W241040", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R PX-W2410a", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "PLEXTOR", "CD-R PX-W4012A", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "RICOH", "CD-R/RW MP7040", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "RICOH", "CD-R/RW MP7060", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "RICOH", "CD-R/RW MP7063A", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "RICOH", "CD-R/RW MP7080", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "RICOH", "CD-R/RW MP7083A", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "RICOH", "DVD/CDRW MP9060", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SAF", "CD-R8020", 0, NULL },
{ "generic-mmc", "SAF", "CD-RW4224A", 0, NULL },
{ "generic-mmc", "SAF", "CD-RW6424", 0, NULL },
{ "generic-mmc", "SAMSUNG", "CD-R/RW SW-206", 0, NULL },
{ "generic-mmc", "SAMSUNG", "CD-R/RW SW-408B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SAMSUNG", "CDRW/DVD SM-308B", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SANYO", "CRD-BP3", 0, NULL },
{ "generic-mmc", "SONY", "CD-RW  CRX700E", 0, NULL },
{ "generic-mmc", "SONY", "CRX-815", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX100", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX120", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX140", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX145", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX160E", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX175A1", 0, NULL },
{ "generic-mmc", "SONY", "CRX175E", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "SONY", "CRX185E1", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TDK", "4800", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TDK", "CDRW121032", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TDK", "CDRW321040B", 0, NULL },
{ "generic-mmc", "TDK", "CDRW8432", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-R56", OPT_MMC_USE_PQ|OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-R58", OPT_MMC_USE_PQ|OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W216E", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W512EB", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W512SB", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W516EB", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W516EC", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W524E", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "TEAC", "CD-W54E", 0, NULL },
{ "generic-mmc", "TORiSAN", "CDW-U4424", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-M1612", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-R1002", 0, NULL },
{ "generic-mmc", "TOSHIBA", "DVD-ROM SD-R1202", 0, NULL },
{ "generic-mmc", "TRAXDATA", "241040", 0, NULL },
{ "generic-mmc", "TRAXDATA", "CDRW4260", 0, NULL },
{ "generic-mmc", "WAITEC", "WT624", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc", "YAMAHA", "CDR200", 0, NULL },
{ "generic-mmc", "YAMAHA", "CDR400", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW2100", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "YAMAHA", "CRW2200", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "YAMAHA", "CRW2260", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW3200", OPT_MMC_CD_TEXT, NULL },
{ "generic-mmc", "YAMAHA", "CRW4001", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW4260", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW4416", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW6416", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW8424", 0, NULL },
{ "generic-mmc", "YAMAHA", "CRW8824", 0, NULL },
{ "generic-mmc", "_NEC", "NR-7700A", 0, NULL },
{ "generic-mmc-raw", "ACER", "10x8x32", 0, NULL },
{ "generic-mmc-raw", "ACER", "2010A", 0, NULL },
{ "generic-mmc-raw", "ACER", "20x10x40", 0, NULL },
{ "generic-mmc-raw", "ACER", "4406EU", 0, NULL },
{ "generic-mmc-raw", "ACER", "4x4x6", 0, NULL },
{ "generic-mmc-raw", "ACER", "8X4X32", 0, NULL },
{ "generic-mmc-raw", "ACER", "CD-R/RW 4X4X32", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc-raw", "AOPEN", "CD-RW CRW3248", 0, NULL },
{ "generic-mmc-raw", "AOPEN", "CRW1232", 0, NULL },
{ "generic-mmc-raw", "ARTEC", "RW241040", 0, NULL },
{ "generic-mmc-raw", "ARTEC", "WRA-WA48", 0, NULL },
{ "generic-mmc-raw", "ARTEC", "WRR-4048", 0, NULL },
{ "generic-mmc-raw", "ASUS", "CRW-1610A", 0, NULL },
{ "generic-mmc-raw", "ASUS", "CRW-3212A", 0, NULL },
{ "generic-mmc-raw", "ATAPI", "CD-R/RW 12X8X32", 0, NULL },
{ "generic-mmc-raw", "ATAPI", "CD-R/RW 4X4X32", 0, NULL },
{ "generic-mmc-raw", "ATAPI", "CD-R/RW CRW6206A", 0, NULL },
{ "generic-mmc-raw", "BENQ", "CRW2410A", 0, NULL },
{ "generic-mmc-raw", "BTC", "BCE1610IM", 0, NULL },
{ "generic-mmc-raw", "BTC", "BCE2410IM", 0, NULL },
{ "generic-mmc-raw", "BTC", "BCE621E", 0, NULL },
{ "generic-mmc-raw", "CyberDrv", "CW018D", 0, NULL },
{ "generic-mmc-raw", "CyberDrv", "CW038D", 0, NULL },
{ "generic-mmc-raw", "CyberDrv", "CW058D", 0, NULL },
{ "generic-mmc-raw", "Goldstar", "8120B", 0, NULL },
{ "generic-mmc-raw", "HL-DT-ST", "CD-RW GCE-8160B", 0, NULL },
{ "generic-mmc-raw", "HL-DT-ST", "CD-RW GCE-8320B", 0, NULL },
{ "generic-mmc-raw", "HP", "CD-Writer+ 7100", 0, NULL },
{ "generic-mmc-raw", "HP", "CD-Writer+ 7200", 0, NULL },
{ "generic-mmc-raw", "HP", "DVD Writer 200j", 0, NULL },
{ "generic-mmc-raw", "IDE-CD", "R/RW 2x2x24", 0, NULL },
{ "generic-mmc-raw", "IDE-CD", "R/RW 4x4x24", 0, NULL },
{ "generic-mmc-raw", "IDE-CD", "R/RW 4x4x32", 0, NULL },
{ "generic-mmc-raw", "IDE-CD", "R/RW 8x4x32", 0, NULL },
{ "generic-mmc-raw", "IDE-CD", "ReWritable-2x2x6", 0, NULL },
{ "generic-mmc-raw", "IOMEGA", "ZIPCD 4x650", 0, NULL },
{ "generic-mmc-raw", "LITE-ON", "LTR-12101B", 0, NULL },
{ "generic-mmc-raw", "LITE-ON", "LTR-16101B", 0, NULL },
{ "generic-mmc-raw", "LITE-ON", "LTR-16102C", 0, NULL },
{ "generic-mmc-raw", "LITE-ON", "LTR-32123S", 0, NULL },
{ "generic-mmc-raw", "LITE-ON", "LTR-40125S", 0, NULL },
{ "generic-mmc-raw", "LITE-ON", "LTR-48125W", 0, NULL },
{ "generic-mmc-raw", "MEMOREX", "CDRW-2216", 0, NULL },
{ "generic-mmc-raw", "MEMOREX", "CR-622", 0, NULL },
{ "generic-mmc-raw", "MEMOREX", "CRW-1662", 0, NULL },
{ "generic-mmc-raw", "MITSUMI", "2801", 0, NULL },
{ "generic-mmc-raw", "MITSUMI", "CR-4802", 0, NULL },
{ "generic-mmc-raw", "MITSUMI", "CR-4804", 0, NULL },
{ "generic-mmc-raw", "OTI", "-975 SOCRATES", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "CDD 3801/31", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "CDD3600", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "CDD3610", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "CDD4201", OPT_MMC_NO_SUBCHAN, NULL },
{ "generic-mmc-raw", "PHILIPS", "CDD4801", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "CDRW400", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "PCRW1208", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "PCRW120899", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "PCRW404", 0, NULL },
{ "generic-mmc-raw", "PHILIPS", "PCRW804", 0, NULL },
{ "generic-mmc-raw", "QPS", "CRD-BP 1500P", 0, NULL },
{ "generic-mmc-raw", "SAMSUNG", "CD-R/RW SW-204B", 0, NULL },
{ "generic-mmc-raw", "SAMSUNG", "CD-R/RW SW-208", 0, NULL },
{ "generic-mmc-raw", "SAMSUNG", "CD-R/RW SW-212B", 0, NULL },
{ "generic-mmc-raw", "SAMSUNG", "CD-R/RW SW-224", 0, NULL },
{ "generic-mmc-raw", "SAMSUNG", "SW-232", 0, NULL },
{ "generic-mmc-raw", "SONY", "CRX195E1", 0, NULL },
{ "generic-mmc-raw", "TEAC", "CD-W58E", OPT_MMC_USE_PQ|OPT_MMC_PQ_BCD, NULL },
{ "generic-mmc-raw", "TOSHIBA", "R/RW 4x4x24", 0, NULL },
{ "generic-mmc-raw", "TRAXDATA", "2832", 0, NULL },
{ "generic-mmc-raw", "TRAXDATA", "CDRW2260+", 0, NULL },
{ "generic-mmc-raw", "TRAXDATA", "CRW2260 PRO", 0, NULL },
{ "generic-mmc-raw", "WAITEC", "WT2444EI", 0, NULL },
{ "generic-mmc-raw", "WAITEC", "WT4424", 0, NULL },
{ "generic-mmc-raw", "_NEC", "7900", 0, NULL },
{ "generic-mmc-raw", "_NEC", "NR-7800A", 0, NULL },
{ "ricoh-mp6200", "AOPEN", "CRW620", 0, NULL },
{ "ricoh-mp6200", "MEMOREX", "CRW620", 0, NULL },
{ "ricoh-mp6200", "PHILIPS", "OMNIWRITER26", 0, NULL },
{ "ricoh-mp6200", "RICOH", "MP6200", 0, NULL },
{ "ricoh-mp6200", "RICOH", "MP6201", 0, NULL },
{ "sony-cdu920", "SONY", "CD-R   CDU920", 0, NULL },
{ "sony-cdu920", "SONY", "CD-R   CDU924", 0, NULL },
{ "sony-cdu948", "SONY", "CD-R   CDU948", 0, NULL },
{ "taiyo-yuden", "T.YUDEN", "CD-WO EW-50", 0, NULL },
{ "teac-cdr55", "JVC", "R2626", 0, NULL },
{ "teac-cdr55", "JVC", "XR-W2010", 0, NULL },
{ "teac-cdr55", "SAF", "CD-R2006PLUS", 0, NULL },
{ "teac-cdr55", "SAF", "CD-R4012", 0, NULL },
{ "teac-cdr55", "SAF", "CD-RW 226", 0, NULL },
{ "teac-cdr55", "TEAC", "CD-R50", 0, NULL },
{ "teac-cdr55", "TEAC", "CD-R55", 0, NULL },
{ "teac-cdr55", "TRAXDATA", "CDR4120", 0, NULL },
{ "toshiba", "TOSHIBA", "DVD-ROM SD-R2002", 0, NULL },
{ "toshiba", "TOSHIBA", "DVD-ROM SD-R2102", 0, NULL },
{ "yamaha-cdr10x", "YAMAHA", "CDR100", 0, NULL },
{ "yamaha-cdr10x", "YAMAHA", "CDR102", 0, NULL },
{ NULL, NULL, NULL, 0, NULL }};

static DriverTable DRIVERS[] = {
{ "cdd2600", &CDD2600::instance },
{ "generic-mmc", &GenericMMC::instance },
{ "generic-mmc-raw", &GenericMMCraw::instance },
{ "plextor", &PlextorReader::instance },
{ "plextor-scan", &PlextorReaderScan::instance },
{ "ricoh-mp6200", &RicohMP6200::instance },
{ "sony-cdu920", &SonyCDU920::instance },
{ "sony-cdu948", &SonyCDU948::instance },
{ "taiyo-yuden", &TaiyoYuden::instance },
{ "teac-cdr55", &TeacCdr55::instance },
{ "toshiba", &ToshibaReader::instance },
{ "yamaha-cdr10x", &YamahaCDR10x::instance },
{ NULL, NULL }};

struct CDRVendorTable {
  char m1, s1, f1; // 1st vendor code
  char m2, s2, f2; // 2nd vendor code
  const char *id;  // vendor ID
};

static CDRVendorTable VENDOR_TABLE[] = {
  // permanent codes
  { 97,28,30,  97,46,50, "Auvistar Industry Co.,Ltd." },
  { 97,26,60,  97,46,60, "CMC Magnetics Corporation" },
  { 97,23,10,  0,0,0,    "Doremi Media Co., Ltd." },
  { 97,26,00,  97,45,00, "FORNET INTERNATIONAL PTE LTD." },
  { 97,46,40,  97,46,40, "FUJI Photo Film Co., Ltd." },
  { 97,26,40,  0,0,0,    "FUJI Photo Film Co., Ltd." },
  { 97,28,10,  97,49,10, "GIGASTORAGE CORPORATION" },
  { 97,25,20,  97,47,10, "Hitachi Maxell, Ltd." },
  { 97,27,40,  97,48,10, "Kodak Japan Limited" },
  { 97,26,50,  97,48,60, "Lead Data Inc." },
  { 97,27,50,  97,48,50, "Mitsui Chemicals, Inc." },
  { 97,34,20,  97,50,20, "Mitsubishi Chemical Corporation" },
  { 97,28,20,  97,46,20, "Multi Media Masters & Machinary SA" },
  { 97,21,40,  0,0,0,    "Optical Disc Manufacturing Equipment" },
  { 97,27,30,  97,48,30, "Pioneer Video Corporation" },
  { 97,27,10,  97,48,20, "Plasmon Data systems Ltd." },
  { 97,26,10,  97,47,40, "POSTECH Corporation" },
  { 97,27,20,  97,47,20, "Princo Corporation" },
  { 97,32,10,  0,0,0,    "Prodisc Technology Inc." },
  { 97,27,60,  97,48,00, "Ricoh Company Limited" },
  { 97,31,00,  97,47,50, "Ritek Co." },
  { 97,26,20,  0,0,0,    "SKC Co., Ltd." },
  { 97,24,10,  0,0,0,    "SONY Corporation" },
  { 97,24,00,  97,46,00, "Taiyo Yuden Company Limited" },
  { 97,32,00,  97,49,00, "TDK Corporation" },
  { 97,25,60,  97,45,60, "Xcitek Inc." },

  // tentative codes
  { 97,22,60,  97,45,20, "Acer Media Technology, Inc" },
  { 97,25,50,  0,0,0,    "AMS Technology Inc." },
  { 97,23,30,  0,0,0,    "AUDIO DISTRIBUTORS CO., LTD." },
  { 97,21,30,  0,0,0,    "Bestdisc Technology Corporation" },
  { 97,30,10,  97,50,30, "CDA Datentraeger Albrechts GmbH" },
  { 97,22,40,  97,45,40, "CIS Technology Inc." },
  { 97,24,20,  97,46,30, "Computer Support Italy s.r.l." },
  { 97,23,60,  0,0,0,    "Customer Pressing Oosterhout" },
  { 97,28,50,  0,0,0,    "DELPHI TECHNOLOGY INC." },
  { 97,27,00,  97,48,40, "DIGITAL STORAGE TECHNOLOGY CO.,LTD" },
  { 97,22,30,  0,0,0,    "EXIMPO" }, 
  { 97,28,60,  0,0,0,    "Friendly CD-Tek Co." },
  { 97,31,30,  97,51,10, "Grand Advance Technology Ltd." },
  { 97,29,50,  0,0,0,    "General Magnetics Ld" },
  { 97,24,50,  97,45,50, "Guann Yinn Co.,Ltd." },
  { 97,29,00,  0,0,0,    "Harmonic Hall Optical Disc Ltd." },
  { 97,29,30,  97,51,50, "Hile Optical Disc Technology Corp." },
  { 97,46,10,  97,22,50, "Hong Kong Digital Technology Co., Ltd." },
  { 97,25,30,  97,51,20, "INFODISC Technology Co., Ltd." },
  { 97,24,40,  0,0,0,    "kdg mediatech AG" },
  { 97,28,40,  97,49,20, "King Pro Mediatek Inc." },
  { 97,23,00,  97,49,60, "Matsushita Electric Industrial Co., Ltd." },
  { 97,15,20,  0,0,0,    "Mitsubishi Chemical Corporation" },
  { 97,25,00,  0,0,0,    "MPO" },
  { 97,23,20,  0,0,0,    "Nacar Media sr" },
  { 97,26,30,  0,0,0,    "OPTICAL DISC CORPRATION" },
  { 97,28,00,  97,49,30, "Opti.Me.S. S.p.A." },
  { 97,23,50,  0,0,0,    "OPTROM.INC." },
  { 97,47,60,  0,0,0,    "Prodisc Technology Inc." },
  { 97,15,10,  0,0,0,    "Ritek Co." },
  { 97,22,10,  0,0,0,    "Seantram Technology Inc." },
  { 97,21,50,  0,0,0,    "Sound Sound Multi-Media Development Limited" },
  { 97,29,00,  0,0,0,    "Taeil Media Co.,Ltd." },
  { 97,18,60,  0,0,0,    "TAROKO INTERNATIONAL CO.,LTD." },
  { 97,15,00,  0,0,0,    "TDK Corporation." },
  { 97,29,20,  0,0,0,    "UNIDISC TECHNOLOGY CO.,LTD" }, 
  { 97,24,30,  97,45,10, "UNITECH JAPAN INC." },
  { 97,29,10,  97,50,10, "Vanguard Disc Inc." },
  { 97,49,40,  97,23,40, "VICTOR COMPANY OF JAPAN, LIMITED" }, 
  { 97,29,40,  0,0,0,    "VIVA MAGNETICS LIMITED" },
  { 97,25,40,  0,0,0,    "VIVASTAR AG" },
  { 97,18,10,  0,0,0,    "WEALTH FAIR INVESTMENT LIMITED" },
  { 97,22,00,  0,0,0,    "Woongjin Media corp." },

  { 0, 0, 0,  0, 0, 0,  NULL}
};

unsigned char CdrDriver::syncPattern[12] = {
  0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0
};

char CdrDriver::REMOTE_MSG_SYNC_[4] = { 0xff, 0x00, 0xff, 0x00 };


/* Maps a string to the corresponding driver option value 
 * Return: 0: string is not a valid driver option sting
 *         else: option value for string
 */
static unsigned long string2DriverOption(const char *s)
{
  if (strcmp(s, "OPT_DRV_GET_TOC_GENERIC") == 0)
    return OPT_DRV_GET_TOC_GENERIC;
  else if (strcmp(s, "OPT_DRV_SWAP_READ_SAMPLES") == 0)
    return OPT_DRV_SWAP_READ_SAMPLES;
  else if (strcmp(s, "OPT_DRV_NO_PREGAP_READ") == 0)
    return OPT_DRV_NO_PREGAP_READ;
  else if (strcmp(s, "OPT_DRV_RAW_TOC_BCD") == 0)
    return OPT_DRV_RAW_TOC_BCD;
  else if (strcmp(s, "OPT_DRV_RAW_TOC_HEX") == 0)
    return OPT_DRV_RAW_TOC_HEX;
  else if (strcmp(s, "OPT_MMC_USE_PQ") == 0)
    return OPT_MMC_USE_PQ;
  else if (strcmp(s, "OPT_MMC_PQ_BCD") == 0)
    return OPT_MMC_PQ_BCD;
  else if (strcmp(s, "OPT_MMC_READ_ISRC") == 0)
    return OPT_MMC_READ_ISRC;
  else if (strcmp(s, "OPT_MMC_SCAN_MCN") == 0)
    return OPT_MMC_SCAN_MCN;
  else if (strcmp(s, "OPT_MMC_CD_TEXT") == 0)
    return OPT_MMC_CD_TEXT;
  else if (strcmp(s, "OPT_MMC_NO_SUBCHAN") == 0)
    return OPT_MMC_NO_SUBCHAN;
  else if (strcmp(s, "OPT_MMC_NO_BURNPROOF") == 0)
    return OPT_MMC_NO_BURNPROOF;
  else if (strcmp(s, "OPT_MMC_NO_RW_PACKED") == 0)
    return OPT_MMC_NO_RW_PACKED;
  else if (strcmp(s, "OPT_MMC_USE_RAW_RW") == 0)
    return OPT_MMC_USE_RAW_RW;
  else if (strcmp(s, "OPT_MMC_YAMAHA_FORCE_SPEED") == 0)
    return OPT_MMC_YAMAHA_FORCE_SPEED;
  else if (strcmp(s, "OPT_PLEX_USE_PARANOIA") == 0)
    return OPT_PLEX_USE_PARANOIA;
  else if (strcmp(s, "OPT_PLEX_DAE_READ10") == 0)
    return OPT_PLEX_DAE_READ10;
  else if (strcmp(s, "OPT_PLEX_DAE_D4_12") == 0)
    return OPT_PLEX_DAE_D4_12;
  else if (strcmp(s, "OPT_PLEX_USE_PQ") == 0)
    return OPT_PLEX_USE_PQ;
  else if (strcmp(s, "OPT_PLEX_PQ_BCD") == 0)
    return OPT_PLEX_PQ_BCD;
  else if (strcmp(s, "OPT_PLEX_READ_ISRC") == 0)
    return OPT_PLEX_READ_ISRC;
  else
    return 0;
}


/* Checks if 'n' is a valid driver name and returns the driver id string
 * from the 'DRIVERS' on success.
 * Return: driver id string
 *         NULL: if 'n' is not a valid driver id
 */
static const char *checkDriverName(const char *n)
{
  DriverTable *run = DRIVERS;

  while (run->driverId != NULL) {
    if (strcmp(run->driverId, n) == 0)
      return run->driverId;

    run++;
  }

  return NULL;
}

/* Reads driver table from specified file.
   Return: 0: OK, driver table file could be opened
           1: could not open driver table file
 */

#define MAX_DRIVER_TABLE_LINE_LEN 1024

static int readDriverTable(const char *filename)
{
  FILE *fp;
  DriverSelectTable *ent;
  long i;
  int lineNr = 0;
  int count = 0;
  int rw;
  int err;
  char buf[MAX_DRIVER_TABLE_LINE_LEN];
  char *p, *l;
  const char *sep = "|";
  char *vendor;
  char *model;
  char *driver;
  const char *lastDriverName = NULL;
  const char *driverName;
  unsigned long opt, options;

  if ((fp = fopen(filename, "r")) == NULL)
    return 1;

  log_message(4, "Reading driver table from file \"%s\".", filename);

  while (fgets(buf, MAX_DRIVER_TABLE_LINE_LEN, fp) != NULL) {
    lineNr++;

    vendor = model = driver = NULL;
    rw = 0;
    options = 0;
    err = 0;

    // remove comment
    if ((p = strchr(buf, '#')) != NULL)
      *p = 0;

    // remove leading white space
    for (l = buf; *l != 0 && isspace(*l); l++) ;

    // remove trailing white space
    for (i = strlen(l) - 1; i >= 0 && isspace(l[i]); i--)
      l[i] = 0;


    if ((p = strtok(l, sep)) != NULL) {
      if (strcmp(p, "R") == 0) {
	rw = 1;
      }
      else if (strcmp(p, "W") == 0) {
	rw = 2;
      }
      else {
	log_message(-1,
		"%s:%d: Expecting 'R' or 'W' as first token - line ignored.",
		filename, lineNr);
      }

      if (rw > 0) {
	if ((p = strtok(NULL, sep)) != NULL) {
	  vendor = strdupCC(p);
	  
	  if ((p = strtok(NULL, sep)) != NULL) {
	    model = strdupCC(p);
	    
	    if ((p = strtok(NULL, sep)) != NULL) {
	      driver = strdupCC(p);

	      if (lastDriverName == NULL ||
		  strcmp(lastDriverName, driver) != 0) {
		if ((driverName = checkDriverName(driver)) == NULL) {
		  log_message(-1, "%s:%d: Driver '%s' not defined - line ignored.",
			  filename, lineNr, driver);
		  err = 1;
		}
		else {
		  lastDriverName = driverName;
		}
	      }

	      while (!err && (p = strtok(NULL, sep)) != NULL) {
		if ((opt = string2DriverOption(p)) == 0) {
		  log_message(-1, "%s:%d: Driver option string '%s' not defined - line ignored.",
			  filename, lineNr, p);
		  err = 1;
		}

		options |= opt;
	      }

	      if (!err) {
		ent = new DriverSelectTable;
	      
		ent->vendor = vendor;
		vendor = NULL;
		ent->model = model;
		model = NULL;
		ent->driverId = driver;
		driver = NULL;
		ent->options = options;
		
		if (rw == 1) {
		  ent->next = READ_DRIVER_TABLE;
		  READ_DRIVER_TABLE = ent;
		}
		else {
		  ent->next = WRITE_DRIVER_TABLE;
		  WRITE_DRIVER_TABLE = ent;
		}
		
		count++;
	      }
	    }
	    else {
	      log_message(-1, "%s:%d: Missing driver name - line ignored.",
		      filename, lineNr);
	    }
	  }
	  else {
	    log_message(-1, "%s:%d: Missing model name - line ignored.",
		      filename, lineNr);
	  }
	}
	else {
	  log_message(-1, "%s:%d: Missing vendor name - line ignored.",
		      filename, lineNr);
	}

	delete[] vendor;
	delete[] model;
	delete[] driver;
      }
    }
  }

  fclose(fp);

  log_message(4, "Found %d valid driver table entries.", count);

  return 0;
}

/* Create driver tables from built-in driver table data.
 */
static void createDriverTable()
{
  DriverSelectTable *run;
  DriverSelectTable *ent;

  for (run = BUILTIN_READ_DRIVER_TABLE; run->driverId != NULL; run++) {
    ent = new DriverSelectTable;

    ent->driverId = strdupCC(run->driverId);
    ent->vendor = strdupCC(run->vendor);
    ent->model = strdupCC(run->model);
    ent->options = run->options;

    ent->next = READ_DRIVER_TABLE;
    READ_DRIVER_TABLE = ent;
  }

  for (run = BUILTIN_WRITE_DRIVER_TABLE; run->driverId != NULL; run++) {
    ent = new DriverSelectTable;

    ent->driverId = strdupCC(run->driverId);
    ent->vendor = strdupCC(run->vendor);
    ent->model = strdupCC(run->model);
    ent->options = run->options;

    ent->next = WRITE_DRIVER_TABLE;
    WRITE_DRIVER_TABLE = ent;
  }
}

/* Initialize driver table. First try to read the driver table from file
 * 'DRIVER_TABLE_FILE'. If it does not exist the built-in driver table data
 * will be used. After that an user specific driver table file is loaded if
 * available.
 */
static void initDriverTable()
{
  static int initialized = 0;
  const char *home;
  char *path;

  if (initialized) 
    return;

  if (readDriverTable(DRIVER_TABLE_FILE) != 0) {
    log_message(2, "Cannot read driver table from file \"%s\" - using built-in table.", DRIVER_TABLE_FILE);
    createDriverTable();
  }

  /* read driver table from $HOME/.cdrdao-drivers */
  if ((home = getenv("HOME")) != NULL) {
    path = strdup3CC(home, "/.cdrdao-drivers", NULL);
    readDriverTable(path);
    delete[] path;
  }

  initialized = 1;
}

const char *CdrDriver::selectDriver(int readWrite, const char *vendor,
				    const char *model, unsigned long *options)
{
  DriverSelectTable *run;
  DriverSelectTable *match = NULL;
  unsigned int matchLen = 0;
  unsigned int len = 0;

  initDriverTable();

  run = (readWrite == 0) ? READ_DRIVER_TABLE : WRITE_DRIVER_TABLE;

  while (run != NULL) {
    if (strcmp(run->vendor, vendor) == 0 && 
	strstr(model, run->model) != NULL) {
      if (match == NULL || (len = strlen(run->model)) > matchLen) {
	matchLen = (match == NULL) ? strlen(run->model) : len;
	match = run;
      }
    }

    run = run->next;
  }

  if (match != NULL) {
    *options = match->options;
    return match->driverId;
  }

  return NULL;
}


CdrDriver *CdrDriver::createDriver(const char *driverId, unsigned long options,
				   ScsiIf *scsiIf)
{
  DriverTable *run = DRIVERS;

  while (run->driverId != NULL) {
    if (strcmp(run->driverId, driverId) == 0) 
      return run->constructor(scsiIf, options);

    run++;
  }

  return NULL;
}

const char *CdrDriver::detectDriver(ScsiIf *scsiIf, unsigned long *options)
{
  bool cd_r_read, cd_r_write, cd_rw_read, cd_rw_write;
  if (scsiIf->checkMmc(&cd_r_read, &cd_r_write, &cd_rw_read, &cd_rw_write)) {
    return "generic-mmc";
  }

  return NULL;
}

void CdrDriver::printDriverIds()
{
  DriverTable *run = DRIVERS;

  while (run->driverId != NULL) {
    log_message(0, "%s", run->driverId);
    run++;
  }
}

CdrDriver::CdrDriver(ScsiIf *scsiIf, unsigned long options) 
{
  size16 byteOrderTest = 1;
  char *byteOrderTestP = (char*)&byteOrderTest;

  options_ = options;
  scsiIf_ = scsiIf;
  toc_ = NULL;

  if (*byteOrderTestP == 1)
    hostByteOrder_ = 0; // little endian
  else
    hostByteOrder_ = 1; // big endian


  enableBufferUnderRunProtection_ = 1;
  enableWriteSpeedControl_ = 1;

  readCapabilities_ = 0; // reading capabilities are determined dynamically

  audioDataByteOrder_ = 0; // default to little endian

  fastTocReading_ = false;
  rawDataReading_ = false;
  mode2Mixed_ = true;
  subChanReadMode_ = TrackData::SUBCHAN_NONE;
  taoSource_ = 0;
  taoSourceAdjust_ = 2; // usually we have 2 unreadable sectors between tracks
                        // written in TAO mode
  padFirstPregap_ = 1;
  onTheFly_ = 0;
  onTheFlyFd_ = -1;
  multiSession_ = false;
  encodingMode_ = 0;
  force_ = false;
  remote_ = 0;
  remoteFd_ = -1;

  blockLength_ = 0;
  blocksPerWrite_ = 0;
  zeroBuffer_ = NULL;
  
  userCapacity_ = 0;
  fullBurn_ = false;

  scsiMaxDataLen_ = scsiIf_->maxDataLen();

  transferBuffer_ = new unsigned char[scsiMaxDataLen_];

  maxScannedSubChannels_ = scsiMaxDataLen_ / (AUDIO_BLOCK_LEN + PW_SUBCHANNEL_LEN);
  scannedSubChannels_ = new SubChannel*[maxScannedSubChannels_];

  paranoia_ = NULL;
  paranoiaDrive_ = NULL;
  paranoiaMode(3); // full paranoia but allow skip
}

CdrDriver::~CdrDriver()
{
  toc_ = NULL;

  delete[] zeroBuffer_;
  zeroBuffer_ = NULL;

  delete[] transferBuffer_;
  transferBuffer_ = NULL;

  delete [] scannedSubChannels_;
  scannedSubChannels_ = NULL;
}

// Sets multi session mode. 0: close session, 1: open next session
// Return: 0: OK
//         1: multi session not supported by driver
int CdrDriver::multiSession(bool m)
{
    multiSession_ = m;

    return 0;
}

// Sets number of adjust sectors for reading TAO source disks.
void CdrDriver::taoSourceAdjust(int val)
{
  if (val >= 0 && val < 100) {
    taoSourceAdjust_ = val;
  }
} 


void CdrDriver::onTheFly(int fd)
{
  if (fd >= 0) {
    onTheFly_ = 1;
    onTheFlyFd_ = fd;
  }
  else {
    onTheFly_ = 0;
    onTheFlyFd_ = -1;
  }
}

void CdrDriver::remote(int f, int fd)
{
  if (f != 0 && fd >= 0) {
    int flags;

    remote_ = 1;
    remoteFd_ = fd;

    // switch 'fd' to non blocking IO mode
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
      log_message(-1, "Cannot get flags of remote stream: %s", strerror(errno));
      remote_ = 0;
      remoteFd_ = -1;
      return;
    }

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) < 0) {
      log_message(-1, "Cannot set flags of remote stream: %s", strerror(errno));
      remote_ = 0;
      remoteFd_ = -1;
    }
  }
  else {
    remote_ = 0;
    remoteFd_ = -1;
  }
}

// Returns acceptable sub-channel encoding mode for given sub-channel type:
//  -1: writing of sub-channel type not supported at all
//   0: accepts plain data without encoding
//   1: accepts only completely encoded data
int CdrDriver::subChannelEncodingMode(TrackData::SubChannelMode sm) const
{
  if (sm == TrackData::SUBCHAN_NONE)
    return 0;
  else
    return -1;
}


int CdrDriver::cdrVendor(Msf &code, const char **vendorId, 
			 const char **mediumType)
{
  CDRVendorTable *run = VENDOR_TABLE;

  *vendorId = NULL;

  char m = code.min();
  char s = code.sec();
  char f = code.frac();

  char type = f % 10;
  f -= type;

  while (run->id != NULL) {
    if ((run->m1 == m && run->s1 == s && run->f1 == f) ||
	(run->m2 == m && run->s2 == s && run->f2 == f)) {
      *vendorId = run->id;
      break;
    }
    run++;
  }

  if (*vendorId != NULL) {
    if (type < 5) {
      *mediumType = "Long Strategy Type, e.g. Cyanine";
    }
    else {
      *mediumType = "Short Strategy Type, e.g. Phthalocyanine";
    }

    return 1;
  }

  return 0;
}

// Sends SCSI command via 'scsiIf_'.
// return: see 'ScsiIf::sendCmd()'
int CdrDriver::sendCmd(const unsigned char *cmd, int cmdLen,
		       const unsigned char *dataOut, int dataOutLen,
		       unsigned char *dataIn, int dataInLen,
		       int showErrorMsg) const
{
  return scsiIf_->sendCmd(cmd, cmdLen, dataOut, dataOutLen, dataIn,
			  dataInLen, showErrorMsg);
}

// checks if unit is ready
// return: 0: OK
//         1: scsi command failed
//         2: not ready
//         3: not ready, no disk in drive
//         4: not ready, tray out

int CdrDriver::testUnitReady(int ignoreUnitAttention) const
{
  unsigned char cmd[6];
  const unsigned char *sense;
  int senseLen;

  memset(cmd, 0, 6);

  switch (scsiIf_->sendCmd(cmd, 6, NULL, 0, NULL, 0, 0)) {
  case 1:
    return 1;

  case 2:
    sense = scsiIf_->getSense(senseLen);
    
    int code = sense[2] & 0x0f;

    if (code == 0x02) {
      // not ready
      return 2;
    }
    else if (code != 0x06) {
      scsiIf_->printError();
      return 1;
    }
    else {
      return 0;
    }
  }

  return 0;
}

bool CdrDriver::rspeed(int a) {
  rspeed_ = a;
  return true;
}

int CdrDriver::speed2Mult(int speed)
{
  return speed / 176;
}

int CdrDriver::mult2Speed(int mult)
{
  return mult * 177;
}

// start unit ('startStop' == 1) or stop unit ('startStop' == 0)
// return: 0: OK
//         1: scsi command failed

int CdrDriver::startStopUnit(int startStop) const
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x1b;
  
  if (startStop != 0) {
    cmd[4] |= 0x01;
  }

  if (sendCmd(cmd, 6, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot start/stop unit.");
    return 1;
  }

  return 0;
}


// blocks or unblocks tray
// return: 0: OK
//         1: scsi command failed

int CdrDriver::preventMediumRemoval(int block) const
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x1e;
  
  if (block != 0) {
    cmd[4] |= 0x01;
  }

  if (sendCmd(cmd, 6, NULL, 0, NULL, 0) != 0) {
    log_message(-2, "Cannot prevent/allow medium removal.");
    return 1;
  }

  return 0;
}

// reset device to initial state
// return: 0: OK
//         1: scsi command failed

int CdrDriver::rezeroUnit(int showMessage) const
{
  unsigned char cmd[6];

  memset(cmd, 0, 6);

  cmd[0] = 0x01;
  
  if (sendCmd(cmd, 6, NULL, 0, NULL, 0, showMessage) != 0) {
    if (showMessage)
      log_message(-2, "Cannot rezero unit.");
    return 1;
  }

  return 0;
}

// Flushs cache of drive which inidcates end of write action. Errors resulting
// from this command are ignored because everything is already done and
// at most the last part of the lead-out track may be affected.
// return: 0: OK
int CdrDriver::flushCache() const
{
  unsigned char cmd[10];

  memset(cmd, 0, 10);

  cmd[0] = 0x35; // FLUSH CACHE
  
  // Print no message if the flush cache command fails because some drives
  // report errors even if all went OK.
  sendCmd(cmd, 10, NULL, 0, NULL, 0, 0);

  return 0;
}

// Reads the cd-rom capacity and stores the total number of available blocks
// in 'length'.
// return: 0: OK
//         1: SCSI command failed
int CdrDriver::readCapacity(long *length, int showMessage)
{
  unsigned char cmd[10];
  unsigned char data[8];

  memset(cmd, 0, 10);
  memset(data, 0, 8);

  cmd[0] = 0x25; // READ CD-ROM CAPACITY

  if (sendCmd(cmd, 10, NULL, 0, data, 8, showMessage) != 0) {
    if (showMessage)
      log_message(-2, "Cannot read capacity.");
    return 1;
  }
  
  *length = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  // *length += 1;

  return 0;
}

int CdrDriver::blankDisk(BlankingMode)
{
  log_message(-2, "Blanking is not supported by this driver.");
  return 1;
}

// Writes data to target, the block length depends on the actual writing mode
// 'mode'. 'len' is number of blocks to write.
// 'lba' specifies the next logical block address for writing and is updated
// by this function.
// return: 0: OK
//         1: scsi command failed
int CdrDriver::writeData(TrackData::Mode mode, TrackData::SubChannelMode sm,
			 long &lba, const char *buf, long len)
{
  assert(blocksPerWrite_ > 0);
  int writeLen = 0;
  unsigned char cmd[10];
  long blockLength = blockSize(mode, sm);
  
#if 0
  long sum, i;

  sum = 0;

  for (i = 0; i < len * blockLength; i++) {
    sum += buf[i];
  }

  log_message(0, "W: %ld: %ld, %ld, %ld", lba, blockLength, len, sum);

#endif


  memset(cmd, 0, 10);
  cmd[0] = 0x2a; // WRITE1
  
  while (len > 0) {
    writeLen = (len > blocksPerWrite_ ? blocksPerWrite_ : len);

    cmd[2] = lba >> 24;
    cmd[3] = lba >> 16;
    cmd[4] = lba >> 8;
    cmd[5] = lba;

    cmd[7] = writeLen >> 8;
    cmd[8] = writeLen & 0xff;

    if (sendCmd(cmd, 10, (unsigned char *)buf, writeLen * blockLength,
		NULL, 0) != 0) {
      log_message(-2, "Write data failed.");
      return 1;
    }

    buf += writeLen * blockLength;

    lba += writeLen;
    len -= writeLen;
  }
      
  return 0;
}

// Writes 'count' blocks with zero data with 'writeData'.
// m: mode for encoding zero data
// lba: logical block address for the write command, will be updated
// encLba: logical block address used by the LE-C encoder for the
//         sector headers
// count: number of zero blocks to write
// Return: 0: OK
//         1: SCSI error occured
int CdrDriver::writeZeros(TrackData::Mode m, TrackData::SubChannelMode sm,
			  long &lba, long encLba, long count)
{
  assert(blocksPerWrite_ > 0);
  assert(zeroBuffer_ != NULL);

  int n, i;
  long cnt = 0;
  long total;
  long cntMb;
  long lastMb = 0;
  long blockLen;
  unsigned char *buf;

  blockLen = blockSize(m, sm);

  total = count * blockLen;

#if 0
  static int wcount = 0;
  char fname[100];
  sprintf(fname, "zeros%d.out", wcount++);
  FILE *fp = fopen(fname, "w");
#endif

  while (count > 0) {
    n = (count > blocksPerWrite_ ? blocksPerWrite_ : count);

    buf = (unsigned char *)zeroBuffer_;

    for (i = 0; i < n; i++) {
      Track::encodeZeroData(encodingMode_, m, sm, encLba++, buf);

      if (encodingMode_ == 0 && bigEndianSamples() == 0) {
	// swap encoded data blocks
	swapSamples((Sample *)buf, SAMPLES_PER_BLOCK);
      }

      buf += blockLen;
    }


    //fwrite(zeroBuffer_, blockLen, n, fp);

    if (writeData(encodingMode_ == 0 ? TrackData::AUDIO : m, sm, lba,
		  zeroBuffer_, n) != 0) {
      return 1;
    }
    
    cnt += n * blockLen;

    cntMb = cnt >> 20;

    if (cntMb > lastMb) {
      log_message(1, "Wrote %ld of %ld MB.\r", cntMb, total >> 20);
      fflush(stdout);
      lastMb = cntMb;
    }

    count -= n;
  }

  //fclose(fp);

  return 0;
}

// Requests mode page 'pageCode' from device and places it into given
// buffer of maximum length 'bufLen'.
// modePageHeader: if != NULL filled with mode page header (8 bytes)
// blockDesc     : if != NULL filled with block descriptor (8 bytes),
//                 buffer is zeroed if no block descriptor is received
// return: 0: OK
//         1: scsi command failed
//         2: buffer too small for requested mode page
int CdrDriver::getModePage(int pageCode, unsigned char *buf, long bufLen,
			   unsigned char *modePageHeader,
			   unsigned char *blockDesc,
			   int showErrorMsg)
{
  unsigned char cmd[10];
  long dataLen = bufLen + 8/*mode parameter header*/ + 
                          100/*spare for block descriptors*/;
  unsigned char *data = new unsigned char[dataLen];

  memset(cmd, 0, 10);
  memset(data, 0, dataLen);
  memset(buf, 0, bufLen);

  cmd[0] = 0x5a; // MODE SENSE
  cmd[2] = pageCode & 0x3f;
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, NULL, 0, data, dataLen,  showErrorMsg) != 0) {
    delete[] data;
    return 1;
  }

  long modeDataLen = (data[0] << 8) | data[1];
  long blockDescLen = (data[6] << 8) | data[7];

  if (modePageHeader != NULL)
    memcpy(modePageHeader, data, 8);

  if (blockDesc != NULL) {
    if (blockDescLen >= 8)
      memcpy(blockDesc, data + 8, 8);
    else 
      memset(blockDesc, 0, 8);
  }

  if (modeDataLen > blockDescLen + 6) {
    unsigned char *modePage = data + blockDescLen + 8;
    long modePageLen = modePage[1] + 2;

    if (modePageLen > bufLen)
      modePageLen = bufLen;

    memcpy(buf, modePage, modePageLen);
    delete[] data;
    return 0;
  }
  else {
    log_message(-2, "No mode page data received.");
    delete[] data;
    return 1;
  }
}

// Sets mode page in device specified in buffer 'modePage'
// modePageHeader: if != NULL used as mode page header (8 bytes)
// blockDesc     : if != NULL used as block descriptor (8 bytes),
// Return: 0: OK
//         1: SCSI command failed
int CdrDriver::setModePage(const unsigned char *modePage,
			   const unsigned char *modePageHeader,
			   const unsigned char *blockDesc,
			   int showErrorMsg)
{
  long pageLen = modePage[1] + 2;
  unsigned char cmd[10];
  long dataLen = pageLen + 8/*mode parameter header*/;

  if (blockDesc != NULL)
    dataLen += 8;

  unsigned char *data = new unsigned char[dataLen];

  memset(cmd, 0, 10);
  memset(data, 0, dataLen);

  if (modePageHeader != NULL)
    memcpy(data, modePageHeader, 8);

  data[0] = 0;
  data[1] = 0;
  data[4] = 0;
  data[5] = 0;


  if (blockDesc != NULL) {
    memcpy(data + 8, blockDesc, 8);
    memcpy(data + 16, modePage, pageLen);
    data[6] = 0;
    data[7] = 8;
  }
  else {
    memcpy(data + 8, modePage, pageLen);
    data[6] = 0;
    data[7] = 0;
  }

  cmd[0] = 0x55; // MODE SELECT
  cmd[1] = 1 << 4;

  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;

  if (sendCmd(cmd, 10, data, dataLen, NULL, 0, showErrorMsg) != 0) {
    delete[] data;
    return 1;
  }

  delete[] data;
  return 0;
}

// As above, but implemented with six byte mode commands

// Requests mode page 'pageCode' from device and places it into given
// buffer of maximum length 'bufLen'.
// modePageHeader: if != NULL filled with mode page header (4 bytes)
// blockDesc     : if != NULL filled with block descriptor (8 bytes),
//                 buffer is zeroed if no block descriptor is received
// return: 0: OK
//         1: scsi command failed
//         2: buffer too small for requested mode page
int CdrDriver::getModePage6(int pageCode, unsigned char *buf, long bufLen,
			    unsigned char *modePageHeader,
			    unsigned char *blockDesc,
			    int showErrorMsg)
{
  unsigned char cmd[6];
  long dataLen = bufLen + 4/*mode parameter header*/ + 
                          100/*spare for block descriptors*/;
  unsigned char *data = new unsigned char[dataLen];

  memset(cmd, 0, 6);
  memset(data, 0, dataLen);
  memset(buf, 0, bufLen);

  cmd[0] = 0x1a; // MODE SENSE(6)
  cmd[2] = pageCode & 0x3f;
  cmd[4] = (dataLen > 255) ? 0 : dataLen;

  if (sendCmd(cmd, 6, NULL, 0, data, dataLen,  showErrorMsg) != 0) {
    delete[] data;
    return 1;
  }

  long modeDataLen = data[0];
  long blockDescLen = data[3];

  if (modePageHeader != NULL)
    memcpy(modePageHeader, data, 4);

  if (blockDesc != NULL) {
    if (blockDescLen >= 8)
      memcpy(blockDesc, data + 4, 8);
    else 
      memset(blockDesc, 0, 8);
  }

  if (modeDataLen > blockDescLen + 4) {
    unsigned char *modePage = data + blockDescLen + 4;
    long modePageLen = modePage[1] + 2;

    if (modePageLen > bufLen)
      modePageLen = bufLen;

    memcpy(buf, modePage, modePageLen);
    delete[] data;
    return 0;
  }
  else {
    log_message(-2, "No mode page data received.");
    delete[] data;
    return 1;
  }
}
 
// Sets mode page in device specified in buffer 'modePage'
// modePageHeader: if != NULL used as mode page header (4 bytes)
// blockDesc     : if != NULL used as block descriptor (8 bytes),
// Return: 0: OK
//         1: SCSI command failed
int CdrDriver::setModePage6(const unsigned char *modePage,
			    const unsigned char *modePageHeader,
			    const unsigned char *blockDesc,
			    int showErrorMsg)
{
  long pageLen = modePage[1] + 2;
  unsigned char cmd[6];
  long dataLen = pageLen + 4/*mode parameter header*/;

  if (blockDesc != NULL)
    dataLen += 8;

  unsigned char *data = new unsigned char[dataLen];

  memset(cmd, 0, 6);
  memset(data, 0, dataLen);

  if (modePageHeader != NULL)
    memcpy(data, modePageHeader, 4);

  data[0] = 0;

  if (blockDesc != NULL) {
    memcpy(data + 4, blockDesc, 8);
    memcpy(data + 12, modePage, pageLen);
    data[3] = 8;
  }
  else {
    memcpy(data + 4, modePage, pageLen);
    data[3] = 0;
  }

  cmd[0] = 0x15; // MODE SELECT(6)
  cmd[1] = 1 << 4;

  cmd[4] = dataLen;

  if (sendCmd(cmd, 6, data, dataLen, NULL, 0, showErrorMsg) != 0) {
    delete[] data;
    return 1;
  }

  delete[] data;
  return 0;
}


// Retrieves TOC data of inserted CD. It won't distinguish between different
// sessions.
// The track information is returned either for all sessions or for the
// first session depending on the drive. 'cdTocLen' is filled with number
// of entries including the lead-out track.
// Return: 'NULL' on error, else array of 'CdToc' structures with '*cdTocLen'
// entries 
CdToc *CdrDriver::getTocGeneric(int *cdTocLen)
{
  unsigned char cmd[10];
  unsigned short dataLen;
  unsigned char *data = NULL;;
  unsigned char reqData[4]; // buffer for requestion the actual length
  unsigned char *p = NULL;
  int i;
  CdToc *toc;
  int nTracks;

  // read disk toc length
  memset(cmd, 0, 10);
  cmd[0] = 0x43; // READ TOC
  cmd[6] = 0; // return info for all tracks
  cmd[8] = 4;
  
  if (sendCmd(cmd, 10, NULL, 0, reqData, 4) != 0) {
    log_message(-2, "Cannot read disk toc.");
    return NULL;
  }

  dataLen = (reqData[0] << 8) | reqData[1];
  dataLen += 2;

  log_message(4, "getTocGeneric: data len %d", dataLen);

  if (dataLen < 12) {
    dataLen = (100 * 8) + 4;
  }

  data = new unsigned char[dataLen];
  memset(data, 0, dataLen);

  // read disk toc
  cmd[7] = dataLen >> 8;
  cmd[8] = dataLen;
  
  if (sendCmd(cmd, 10, NULL, 0, data, dataLen) != 0) {
    log_message(-2, "Cannot read disk toc.");
    delete[] data;
    return NULL;
  }

  nTracks = data[3] - data[2] + 1;
  if (nTracks > 99) {
    log_message(-2, "Got illegal toc data.");
    delete[] data;
    return NULL;
  }

  toc = new CdToc[nTracks + 1];
  
  for (i = 0, p = data + 4; i <= nTracks; i++, p += 8) {
    toc[i].track = p[2];
    toc[i].adrCtl = p[1];
    toc[i].start = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
  }

  *cdTocLen = nTracks + 1;

  delete[] data;

  return toc;
}

// Retrieves TOC data of inserted CD. The track information is returend for
// specified session number only. The lead-out start is taken from the
// correct session so that it can be used to calculate the length of the
// last track.
// 'cdTocLen' is filled with number of entries including the lead-out track.
// Return: 'NULL' on error, else array of 'CdToc' structures with '*cdTocLen'
// entries 

#define IS_BCD(v) (((v) & 0xf0) <= 0x90 && ((v) & 0x0f) <= 0x09)
CdToc *CdrDriver::getToc(int sessionNr, int *cdTocLen)
{
  int rawTocLen;
  int completeTocLen;
  CdToc *completeToc; // toc retrieved with generic method to verify with raw
                      // toc data
  CdToc *cdToc;
  CdRawToc *rawToc;
  int i, j, tocEnt;
  int nTracks = 0;
  int trackNr;
  long trackStart;
  int isBcd = -1;
  int lastTrack;
  int min, sec, frame;

  if ((completeToc = getTocGeneric(&completeTocLen)) == NULL)
    return NULL;

  if (options_ & OPT_DRV_GET_TOC_GENERIC) {
    *cdTocLen = completeTocLen;
    return completeToc;
  }

  if ((rawToc = getRawToc(1, &rawTocLen)) == NULL) {
    *cdTocLen = completeTocLen;
    return completeToc;
  }

  // Try to determine if raw toc data contains BCD or HEX numbers.
  for (i = 0; i < rawTocLen; i++) {
    if ((rawToc[i].adrCtl & 0xf0) == 0x10) { // only process QMODE1 entries
      if (rawToc[i].point < 0xa0 && !IS_BCD(rawToc[i].point)) {
	isBcd = 0;
      }

      if (rawToc[i].point < 0xa0 || rawToc[i].point == 0xa2) {
	if (!IS_BCD(rawToc[i].pmin) || !IS_BCD(rawToc[i].psec) ||
	    !IS_BCD(rawToc[i].pframe)) {
	  isBcd = 0;
	  break;
	}
      }
    }
  }

  if (options_ & OPT_DRV_RAW_TOC_BCD) {
    if (isBcd == 0) {
      log_message(-2, "The driver option 0x%lx indicates that the raw TOC data",
	      OPT_DRV_RAW_TOC_BCD);
      log_message(-2, "contains BCD values but a non BCD value was found.");
      log_message(-2, "Please adjust the driver options.");
      log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
	
      delete[] rawToc;
      *cdTocLen = completeTocLen;
      return completeToc;
    }
    isBcd = 1;
  }
  else if (options_ & OPT_DRV_RAW_TOC_HEX) {
    isBcd = 0;
  }
  else {
    if (isBcd == -1) {
      // We still don't know if the values are BCD or HEX but we've ensured
      // so far that all values are valid BCD numbers.

      // Assume that we have BCD numbers and compare with the generic toc data.
      isBcd = 1;
      for (i = 0; i < rawTocLen && isBcd == 1; i++) {
	if ((rawToc[i].adrCtl & 0xf0) == 0x10 && // only process QMODE1 entries
	    rawToc[i].point < 0xa0) { 
	  trackNr = SubChannel::bcd2int(rawToc[i].point);

	  for (j = 0; j < completeTocLen; j++) {
	    if (completeToc[j].track == trackNr) {
	      break;
	    }
	  }

	  if (j < completeTocLen) {
	    min = SubChannel::bcd2int(rawToc[i].pmin);
	    sec = SubChannel::bcd2int(rawToc[i].psec);
	    frame = SubChannel::bcd2int(rawToc[i].pframe);

	    if (min <= 99 && sec < 60 && frame < 75) {
	      trackStart = Msf(min, sec, frame).lba() - 150;
	      if (completeToc[j].start != trackStart) {
		// start does not match -> values are not BCD
		isBcd = 0;
	      }
	    }
	    else {
	      // bogus time code -> values are not BCD
	      isBcd = 0;
	    }
	  }
	  else {
	    // track not found -> values are not BCD
	    isBcd = 0;
	  }
	}
      }

      if (isBcd == 1) {
	// verify last lead-out pointer
	trackStart = 0; // start of lead-out
	for (i = rawTocLen - 1; i >= 0; i--) {
	  if ((rawToc[i].adrCtl & 0xf0) == 0x10 && // QMODE1 entry
	      rawToc[i].point == 0xa2) {
	    min = SubChannel::bcd2int(rawToc[i].pmin);
	    sec = SubChannel::bcd2int(rawToc[i].psec);
	    frame = SubChannel::bcd2int(rawToc[i].pframe);
	    
	    if (min <= 99 && sec < 60 && frame < 75)
	      trackStart = Msf(min, sec, frame).lba() - 150;
	    break;
	  }
	}
	
	if (i < 0) {
	  log_message(-1, "Found bogus toc data (no lead-out entry in raw data).");
	  log_message(-1, "Your drive probably does not support raw toc reading.");
	  log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
	  log_message(-1, "Use driver option 0x%lx to suppress this message.",
		  OPT_DRV_GET_TOC_GENERIC);
	
	  delete[] rawToc;
	  *cdTocLen = completeTocLen;
	  return completeToc;
	}
	
	for (j = 0; j < completeTocLen; j++) {
	  if (completeToc[j].track == 0xaa) {
	    break;
	  }
	}
	
	if (j < completeTocLen) {
	  if (trackStart != completeToc[j].start) {
	    // lead-out start does not match -> values are not BCD
	    isBcd = 0;
	  }
	}
	else {
	  log_message(-2, "Found bogus toc data (no lead-out entry).");
	  
	  delete[] completeToc;
	  delete[] rawToc;
	  return NULL;
	}
      }
      
    }

    if (isBcd == 0) {
      // verify that the decision is really correct.
      for (i = 0; i < rawTocLen && isBcd == 0; i++) {
	if ((rawToc[i].adrCtl & 0xf0) == 0x10 && // only process QMODE1 entries
	    rawToc[i].point < 0xa0) {
	  
	  trackNr = rawToc[i].point;
	
	  for (j = 0; j < completeTocLen; j++) {
	    if (completeToc[j].track == trackNr) {
	      break;
	    }
	  }

	  if (j < completeTocLen) {
	    min = rawToc[i].pmin;
	    sec = rawToc[i].psec;
	    frame = rawToc[i].pframe;
	    
	    if (min <= 99 && sec < 60 && frame < 75) {
	      trackStart = Msf(min, sec, frame).lba() - 150;
	      if (completeToc[j].start != trackStart) {
		// start does not match -> values are not HEX
		isBcd = -1;
	      }
	    }
	    else {
	      // bogus time code -> values are not HEX
	      isBcd = -1;
	    }
	  }
	  else {
	    // track not found -> values are not BCD
	    isBcd = -1;
	  }
	}
      }

      // verify last lead-out pointer
      trackStart = 0; // start of lead-out
      for (i = rawTocLen - 1; i >= 0; i--) {
	if ((rawToc[i].adrCtl & 0xf0) == 0x10 && // QMODE1 entry
	    rawToc[i].point == 0xa2) {
	  min = rawToc[i].pmin;
	  sec = rawToc[i].psec;
	  frame = rawToc[i].pframe;
	  
	  if (min <= 99 && sec < 60 && frame < 75)
	    trackStart = Msf(min, sec, frame).lba() - 150;
	  break;
	}
      }

      if (i < 0) {
	log_message(-1, "Found bogus toc data (no lead-out entry in raw data).");
	log_message(-1, "Your drive probably does not support raw toc reading.");
	log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
	log_message(-1, "Use driver option 0x%lx to suppress this message.",
		OPT_DRV_GET_TOC_GENERIC);
	
	delete[] rawToc;
	*cdTocLen = completeTocLen;
	return completeToc;
      }
      
      for (j = 0; j < completeTocLen; j++) {
	if (completeToc[j].track == 0xaa) {
	  break;
	}
      }
      
      if (j < completeTocLen) {
	if (trackStart != completeToc[j].start) {
	  // lead-out start does not match -> values are not BCD
	  isBcd = -1;
	}
      }
      else {
	log_message(-1, "Found bogus toc data (no lead-out entry).");
	
	delete[] rawToc;
	delete[] completeToc;
	return NULL;
      }
    }
    
    if (isBcd == -1) {
      log_message(-1, "Could not determine if raw toc data is BCD or HEX. Please report!");
      log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
      log_message(-1,
	      "Use driver option 0x%lx or 0x%lx to assume BCD or HEX data.",
	      OPT_DRV_RAW_TOC_BCD, OPT_DRV_RAW_TOC_HEX);
      
      delete[] rawToc;
      *cdTocLen = completeTocLen;
      return completeToc;
    }
  }

  log_message(4, "Raw toc contains %s values.", isBcd == 0 ? "HEX" : "BCD");

  for (i = 0; i < rawTocLen; i++) {
    if (rawToc[i].sessionNr == sessionNr &&
	(rawToc[i].adrCtl & 0xf0) == 0x10 && /* QMODE1 entry */
	rawToc[i].point < 0xa0) {
      nTracks++;
    }
  }

  if (nTracks == 0 || nTracks > 99) {
    log_message(-1, "Found bogus toc data (0 or > 99 tracks). Please report!");
    log_message(-1, "Your drive probably does not support raw toc reading.");
    log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
    log_message(-1, "Use driver option 0x%lx to suppress this message.",
	    OPT_DRV_GET_TOC_GENERIC);
	
    delete[] rawToc;
    *cdTocLen = completeTocLen;
    return completeToc;
  }

  cdToc = new CdToc[nTracks + 1];
  tocEnt = 0;
  lastTrack = -1;

  for (i = 0; i < rawTocLen; i++) {
    if (rawToc[i].sessionNr == sessionNr &&
	(rawToc[i].adrCtl & 0xf0) == 0x10 &&  // QMODE1 entry
	rawToc[i].point < 0xa0) {

      if (isBcd) {
	trackNr = SubChannel::bcd2int(rawToc[i].point);
	trackStart = Msf(SubChannel::bcd2int(rawToc[i].pmin),
			 SubChannel::bcd2int(rawToc[i].psec),
			 SubChannel::bcd2int(rawToc[i].pframe)).lba();
      }
      else {
	trackNr = rawToc[i].point;
	trackStart =
	  Msf(rawToc[i].pmin, rawToc[i].psec, rawToc[i].pframe).lba();
      }

      if (lastTrack != -1 && trackNr != lastTrack + 1) {
	log_message(-1, "Found bogus toc data (track number sequence). Please report!");
	log_message(-1, "Your drive probably does not support raw toc reading.");
	log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
	log_message(-1, "Use driver option 0x%lx to suppress this message.",
		OPT_DRV_GET_TOC_GENERIC);
	
	delete[] cdToc;
	delete[] rawToc;
	*cdTocLen = completeTocLen;
	return completeToc;
      }

      lastTrack = trackNr;

      cdToc[tocEnt].adrCtl = rawToc[i].adrCtl;
      cdToc[tocEnt].track = trackNr;
      cdToc[tocEnt].start = trackStart - 150;
      tocEnt++;
    }
  }

  // find lead-out pointer
  for (i = 0; i < rawTocLen; i++) {
    if (rawToc[i].sessionNr == sessionNr &&
	(rawToc[i].adrCtl & 0xf0) == 0x10 &&  // QMODE1 entry
	rawToc[i].point == 0xa2 /* Lead-out pointer */) {

      if (isBcd) {
	trackStart = Msf(SubChannel::bcd2int(rawToc[i].pmin),
			 SubChannel::bcd2int(rawToc[i].psec),
			 SubChannel::bcd2int(rawToc[i].pframe)).lba();
      }
      else {
	trackStart =
	  Msf(rawToc[i].pmin, rawToc[i].psec, rawToc[i].pframe).lba();
      }
      
      cdToc[tocEnt].adrCtl = rawToc[i].adrCtl;
      cdToc[tocEnt].track = 0xaa;
      cdToc[tocEnt].start = trackStart - 150;
      tocEnt++;

      break;
    }
  }
  
  if (tocEnt != nTracks + 1) {
    log_message(-1, "Found bogus toc data (no lead-out pointer for session). Please report!");
    log_message(-1, "Your drive probably does not support raw toc reading.");
    log_message(-1, "Using TOC data retrieved with generic method (no multi session support).");
    log_message(-1, "Use driver option 0x%lx to suppress this message.",
	    OPT_DRV_GET_TOC_GENERIC);
	
    delete[] cdToc;
    delete[] rawToc;
    *cdTocLen = completeTocLen;
    return completeToc;
  }
  

  delete[] rawToc;
  delete[] completeToc;

  *cdTocLen = nTracks + 1;
  return cdToc;
}

static char *buildDataFileName(int trackNr, CdToc *toc, int nofTracks, 
			       const char *basename, const char *extension)
{
  char buf[30];
  int start, end;
  int run;
  int onlyOneAudioRange = 1;

  // don't modify the STDIN filename
  if (strcmp(basename, "-") == 0)
    return strdupCC(basename);


  if ((toc[trackNr].adrCtl & 0x04) != 0) {
    // data track
    sprintf(buf, "_%d", trackNr + 1);
    return strdup3CC(basename, buf, NULL);
  }

  // audio track, find continues range of audio tracks
  start = trackNr;
  while (start > 0 && (toc[start - 1].adrCtl & 0x04) == 0)
    start--;

  if (start > 0) {
    run = start - 1;
    while (run >= 0) {
      if ((toc[run].adrCtl & 0x04) == 0) {
	onlyOneAudioRange = 0;
	break;
      }
      run--;
    }
  }

  end = trackNr;
  while (end < nofTracks - 1 && (toc[end + 1].adrCtl & 0x04) == 0)
    end++;

  if (onlyOneAudioRange && end < nofTracks - 1) {
    run = end + 1;
    while (run < nofTracks) {
      if ((toc[run].adrCtl & 0x04) == 0) {
	onlyOneAudioRange = 0;
	break;
      }
      run++;
    }
  }

  if (onlyOneAudioRange) {
    return strdup3CC(basename, extension, NULL);
  }
  else {
    sprintf(buf, "_%d-%d", start + 1, end + 1);
    return strdup3CC(basename, buf, extension);
  }
}

/* Checks if drive's capabilites support the selected sub-channel reading
   mode for given track mode.
   mode: track mode
   caps: capabilities bits
   Return: 1: current sub-channel reading mode is supported
           0: current sub-channel reading mode is not supported
*/
int CdrDriver::checkSubChanReadCaps(TrackData::Mode mode, unsigned long caps)
{
  int ret = 0;

  switch (subChanReadMode_) {
  case TrackData::SUBCHAN_NONE:
    ret = 1;
    break;

  case TrackData::SUBCHAN_RW_RAW:
    if (mode == TrackData::AUDIO) {
      if ((caps & (CDR_READ_CAP_AUDIO_RW_RAW|CDR_READ_CAP_AUDIO_PW_RAW)) != 0)
	ret = 1;
    }
    else {
	if ((caps & (CDR_READ_CAP_DATA_RW_RAW|CDR_READ_CAP_DATA_PW_RAW)) != 0)
	ret = 1;
    }
    break;

  case TrackData::SUBCHAN_RW:
    if (mode == TrackData::AUDIO) {
      if ((caps & CDR_READ_CAP_AUDIO_RW_COOKED) != 0)
	ret = 1;
    }
    else {
      if ((caps & CDR_READ_CAP_DATA_RW_COOKED) != 0)
	ret = 1;
    }
    break;
  }
   
  return ret;
}

// Creates 'Toc' object for inserted CD.
// session: session that should be analyzed
// audioFilename: name of audio file that is placed into TOC
// Return: newly allocated 'Toc' object or 'NULL' on error
Toc *CdrDriver::readDiskToc(int session, const char *dataFilename)
{
  int nofTracks = 0;
  int i, j;
  CdToc *cdToc = getToc(session, &nofTracks);
  Msf indexIncrements[98];
  int indexIncrementCnt = 0;
  char isrcCode[13];
  unsigned char trackCtl; // control nibbles of track
  int ctlCheckOk;
  char *fname;
  char *extension = NULL;
  char *p;
  TrackInfo *trackInfos;

  if (cdToc == NULL) {
    return NULL;
  }

  if (nofTracks <= 1) {
    log_message(-1, "No tracks on disk.");
    delete[] cdToc;
    return NULL;
  }

  log_message(1, "");
  printCdToc(cdToc, nofTracks);
  log_message(1, "");
  //return NULL;

  nofTracks -= 1; // do not count lead-out

  readCapabilities_ = getReadCapabilities(cdToc, nofTracks);
  
  fname = strdupCC(dataFilename);
  if ((p = strrchr(fname, '.')) != NULL) {
    extension = strdupCC(p);
    *p = 0;
  }

  trackInfos = new TrackInfo[nofTracks + 1];
  memset(trackInfos, 0, (nofTracks + 1) * sizeof(TrackInfo));

  for (i = 0; i < nofTracks; i++) {
    TrackData::Mode trackMode;

    if ((cdToc[i].adrCtl & 0x04) != 0) {
      if ((trackMode = getTrackMode(i + 1, cdToc[i].start)) ==
	  TrackData::MODE0) {
	log_message(-1, "Cannot determine mode of data track %d - asuming MODE1.",
		i + 1);
	trackMode = TrackData::MODE1;
      }

      if (rawDataReading_) {
	if (trackMode == TrackData::MODE1) {
	  trackMode = TrackData::MODE1_RAW;
	}
	else if (trackMode == TrackData::MODE2) {
	  trackMode = TrackData::MODE2_RAW;
	}
	else if (trackMode == TrackData::MODE2_FORM1 ||
		 trackMode == TrackData::MODE2_FORM2 ||
		 trackMode == TrackData::MODE2_FORM_MIX) {
	  trackMode = TrackData::MODE2_RAW;
	}
      }
      else if (mode2Mixed_) {
	if (trackMode == TrackData::MODE2_FORM1 ||
	    trackMode == TrackData::MODE2_FORM2) {
	  trackMode = TrackData::MODE2_FORM_MIX;
	}
      }
    }
    else {
      trackMode = TrackData::AUDIO;
    }

    if (!checkSubChanReadCaps(trackMode, readCapabilities_)) {
      log_message(-2, "This drive does not support %s sub-channel reading.",
	      TrackData::subChannelMode2String(subChanReadMode_));
      delete[] cdToc;
      delete[] trackInfos;
      delete[] fname;
      return NULL;
    }

    trackInfos[i].trackNr = cdToc[i].track;
    trackInfos[i].ctl = cdToc[i].adrCtl & 0x0f;
    trackInfos[i].mode = trackMode;
    trackInfos[i].start = cdToc[i].start;
    trackInfos[i].pregap = 0;
    trackInfos[i].fill = 0;
    trackInfos[i].indexCnt = 0;
    trackInfos[i].isrcCode[0] = 0;
    trackInfos[i].filename = buildDataFileName(i, cdToc, nofTracks, fname,
					       extension);
    trackInfos[i].bytesWritten = 0;
  }

  // lead-out entry
  trackInfos[nofTracks].trackNr = 0xaa;
  trackInfos[nofTracks].ctl = 0;
  trackInfos[nofTracks].mode = trackInfos[nofTracks - 1].mode;
  trackInfos[nofTracks].start = cdToc[nofTracks].start;
  if (taoSource()) {
    trackInfos[nofTracks].start -= taoSourceAdjust_;
  }
  trackInfos[nofTracks].pregap = 0;
  trackInfos[nofTracks].fill = 0;
  trackInfos[nofTracks].indexCnt = 0;
  trackInfos[nofTracks].isrcCode[0] = 0;
  trackInfos[nofTracks].filename = NULL;
  trackInfos[nofTracks].bytesWritten = 0;
  
  long pregap = 0;
  long defaultPregap;
  long slba, elba;

  if (session == 1) {
    pregap = cdToc[0].start; // pre-gap of first track
  }

  for (i = 0; i < nofTracks; i++) {
    trackInfos[i].pregap = pregap;

    slba = trackInfos[i].start;
    elba = trackInfos[i + 1].start;

    defaultPregap = 0;

    if (taoSource()) {
      // assume always a pre-gap of 150 + # link blocks between two tracks
      // except between two audio tracks
      if ((trackInfos[i].mode != TrackData::AUDIO ||
	   trackInfos[i + 1].mode != TrackData::AUDIO) &&
	  i < nofTracks - 1) {
	defaultPregap = 150 + taoSourceAdjust_;
      }
    }
    else {
      // assume a pre-gap of 150 between tracks of different mode
      if (trackInfos[i].mode != trackInfos[i + 1].mode) {
	defaultPregap = 150;
      }
    }

    elba -= defaultPregap;
    

    Msf trackLength(elba - slba);

    log_message(1, "Analyzing track %02d (%s): start %s, ", i + 1,
	    TrackData::mode2String(trackInfos[i].mode),
	    Msf(cdToc[i].start).str());
    log_message(1, "length %s...", trackLength.str());

    if (pregap > 0) {
      log_message(2, "Found pre-gap: %s", Msf(pregap).str());
    }

    isrcCode[0] = 0;
    indexIncrementCnt = 0;
    pregap = 0;
    trackCtl = 0;

    if (!fastTocReading_) {
      // Find index increments and pre-gap of next track
      if (trackInfos[i].mode == TrackData::AUDIO) {
	analyzeTrack(TrackData::AUDIO, i + 1, slba, elba,
		     indexIncrements, &indexIncrementCnt, 
		     i < nofTracks - 1 ? &pregap : 0, isrcCode, &trackCtl);
	if (defaultPregap != 0)
	  pregap = defaultPregap;
      }
    }
    else {
      if (trackInfos[i].mode == TrackData::AUDIO) {
	if (readIsrc(i + 1, isrcCode) != 0) {
	  isrcCode[0] = 0;
	}
      }
    }

    if (pregap == 0) {
      pregap = defaultPregap;
    }

    if (isrcCode[0] != 0) {
      log_message(2, "Found ISRC code.");
      memcpy(trackInfos[i].isrcCode, isrcCode, 13);
    }

    for (j = 0; j < indexIncrementCnt; j++)
      trackInfos[i].index[j] = indexIncrements[j].lba();
    
    trackInfos[i].indexCnt = indexIncrementCnt;
    
    if ((trackCtl & 0x80) != 0) {
      // Check track against TOC control nibbles
      ctlCheckOk = 1;
      if ((trackCtl & 0x01) !=  (cdToc[i].adrCtl & 0x01)) {
	log_message(-1, "Pre-emphasis flag of track differs from TOC - toc file contains TOC setting.");
	ctlCheckOk = 0;
      }
      if ((trackCtl & 0x08) != (cdToc[i].adrCtl & 0x08)) {
	log_message(-1, "2-/4-channel-audio  flag of track differs from TOC - toc file contains TOC setting.");
	ctlCheckOk = 0;
      }

      if (ctlCheckOk) {
	log_message(2, "Control nibbles of track match CD-TOC settings.");
      }
    }
  }

  int padFirstPregap;

  if (onTheFly_) {
    if (session == 1 && (options_ & OPT_DRV_NO_PREGAP_READ) == 0)
      padFirstPregap = 0;
    else
      padFirstPregap = 1;
  }
  else {
    padFirstPregap = (session != 1) || padFirstPregap_;
  }


  Toc *toc = buildToc(trackInfos, nofTracks + 1, padFirstPregap);

  if (toc != NULL) {
    if ((options_ & OPT_DRV_NO_CDTEXT_READ) == 0) 
      readCdTextData(toc);

    if (readCatalog(toc, trackInfos[0].start, trackInfos[nofTracks].start))
      log_message(2, "Found disk catalogue number.");
  }

  // overwrite last time message
  log_message(1, "        \t");

  delete[] cdToc;
  delete[] trackInfos;

  delete[] fname;
  if (extension != NULL)
    delete[] extension;

  return toc;
}

// Implementation is based on binary search over all sectors of actual
// track. ISRC codes are not extracted here.
int CdrDriver::analyzeTrackSearch(TrackData::Mode, int trackNr, long startLba,
				  long endLba, Msf *index, int *indexCnt,
				  long *pregap, char *isrcCode,
				  unsigned char *ctl)
{
  isrcCode[0] = 0;
  *ctl = 0;


  if (pregap != NULL) {
    *pregap = findIndex(trackNr + 1, 0, startLba, endLba - 1);
    if (*pregap >= endLba) {
	*pregap = 0;
    }
    else if (*pregap > 0) {
      *pregap = endLba - *pregap;
    }
  }

  // check for index increments
  int ind = 2;
  long indexLba = startLba;
  *indexCnt = 0;

  do {
    if ((indexLba = findIndex(trackNr, ind, indexLba, endLba - 1)) > 0) {
      log_message(2, "Found index %d at %s", ind, Msf(indexLba).str());
      if (*indexCnt < 98 && indexLba > startLba) {
	index[*indexCnt] =  Msf(indexLba - startLba);
	*indexCnt += 1;
      }
      ind++;
    }
  } while (indexLba > 0 && indexLba < endLba);


  // Retrieve control nibbles of track, add 75 to track start so we 
  // surely get a block of current track.
  int dummy, track;
  if (getTrackIndex(startLba + 75, &track, &dummy, ctl) == 0 &&
      track == trackNr) {
    *ctl |= 0x80;
  }

  return 0;
}

int CdrDriver::getTrackIndex(long lba, int *trackNr, int *indexNr, 
			     unsigned char *ctl)
{
  return 1;
}

// Scan from lba 'trackStart' to 'trackEnd' for the position at which the
// index switchs to 'index' of track number 'track'.
// return: lba of index start postion or 0 if index was not found
long CdrDriver::findIndex(int track, int index, long trackStart,
			  long trackEnd)
{
  int actTrack;
  int actIndex;
  long start = trackStart;
  long end = trackEnd;
  long mid;

  //log_message(0, "findIndex: %ld - %ld", trackStart, trackEnd);

  while (start < end) {
    mid = start + ((end - start) / 2);
    
    //log_message(0, "Checking block %ld...", mid);
    if (getTrackIndex(mid, &actTrack, &actIndex, NULL) != 0) {
      return 0;
    }
    //log_message(0, "Found track %d, index %d", actTrack, actIndex);
    if ((actTrack < track || actIndex < index) && mid + 1 < trackEnd) {
      //log_message(0, "  Checking block %ld...", mid + 1);
      if (getTrackIndex(mid + 1, &actTrack, &actIndex, NULL) != 0) {
	return 0;
      }
      //log_message(0, "  Found track %d, index %d", actTrack, actIndex);
      if (actTrack == track && actIndex == index) {
	//log_message(0, "Found pregap at %ld", mid + 1);
	return mid;
      }
      else {
	start = mid + 1;
      }
      
    }
    else {
      end = mid;
    }
  }

  return 0;
}

int CdrDriver::analyzeTrackScan(TrackData::Mode, int trackNr, long startLba,
				long endLba,
				Msf *index, int *indexCnt, long *pregap,
				char *isrcCode, unsigned char *ctl)
{
  SubChannel **subChannels;
  int n, i;
  int actIndex = 1;
  long length;
  long crcErrCnt = 0;
  long timeCnt = 0;
  int ctlSet = 0;
  int isrcCodeFound = 0;
  long trackStartLba = startLba;

  *isrcCode = 0;

  if (pregap != NULL)
    *pregap = 0;

  *indexCnt = 0;
  *ctl = 0;

  //startLba -= 75;
  if (startLba < 0) {
    startLba = 0;
  }
  length = endLba - startLba;

  while (length > 0) {
    n = (length > maxScannedSubChannels_) ? maxScannedSubChannels_ : length;

    if (readSubChannels(TrackData::SUBCHAN_NONE, startLba, n, &subChannels,
			NULL) != 0 ||
	subChannels == NULL) {
      return 1;
    }

    for (i = 0; i < n; i++) {
      SubChannel *chan = subChannels[i];
      //chan->print();

      if (chan->checkCrc() && chan->checkConsistency()) {
	if (chan->type() == SubChannel::QMODE1DATA) {
	  int t = chan->trackNr();
	  Msf time(chan->min(), chan->sec(), chan->frame()); // track rel time
	  Msf atime(chan->amin(), chan->asec(), chan->aframe()); // abs time

	  if (timeCnt > 74) {
	    log_message(1, "%s\r", time.str());
	    timeCnt = 0;
	  }

	  if (t == trackNr && !ctlSet) {
	    *ctl = chan->ctl();
	    *ctl |= 0x80;
	    ctlSet = 1;
	  }
	  if (t == trackNr && chan->indexNr() == actIndex + 1) {
	    actIndex = chan->indexNr();
	    log_message(2, "Found index %d at: %s", actIndex, time.str());
	    if ((*indexCnt) < 98) {
	      index[*indexCnt] = time;
	      *indexCnt += 1;
	    }
	  }
	  else if (t == trackNr + 1) {
	    if (chan->indexNr() == 0) {
	      if (pregap != NULL) {
                // don't use time.lba() to calculate pre-gap length; it would
                // count one frame too many if the CD counts the pre-gap down
                // to 00:00:00 instead of 00:00:01
                // Instead, count number of frames until start of Index 01
                // See http://sourceforge.net/tracker/?func=detail&aid=604751&group_id=2171&atid=102171
                // atime starts at 02:00, so subtract it
		*pregap = endLba - (atime.lba() - 150);
              }
	      if (crcErrCnt != 0)
		log_message(2, "Found %ld Q sub-channels with CRC errors.",
			crcErrCnt);
	    
	      return 0;
	    }
	  }
	}
	else if (chan->type() == SubChannel::QMODE3) {
	  if (!isrcCodeFound && startLba > trackStartLba) {
	    strcpy(isrcCode, chan->isrc());
	    isrcCodeFound = 1;
	  }
	}
      }
      else {
	crcErrCnt++;
#if 0
	if (chan->type() == SubChannel::QMODE1DATA) {
	  log_message(2, "Q sub-channel data at %02d:%02d:%02d failed CRC check - ignored",
		 chan->min(), chan->sec(), chan->frame());
	}
	else {
	  log_message(2, "Q sub-channel data failed CRC check - ignored.");
	}
	chan->print();
#endif
      }

      timeCnt++;
    }

    length -= n;
    startLba += n;
  }

  if (crcErrCnt != 0)
    log_message(2, "Found %ld Q sub-channels with CRC errors.", crcErrCnt);

  return 0;
}

// Checks if toc is suitable for writing. Usually all tocs are OK so
// return just 0 here.
// Return: 0: OK
//         1: toc may not be  suitable
//         2: toc is not suitable
int CdrDriver::checkToc(const Toc *toc)
{
  int ret = 0;

  if (multiSession_ && toc->tocType() != Toc::CD_ROM_XA) {
    log_message(-1, "The toc type should be set to CD_ROM_XA if a multi session");
    log_message(-1, "CD is recorded.");
    ret = 1;
  }

  TrackIterator itr(toc);
  const Track *run;
  int tracknr;

  for (run = itr.first(), tracknr = 1; run != NULL;
       run = itr.next(), tracknr++) {
    if (run->subChannelType() != TrackData::SUBCHAN_NONE) {
      if (subChannelEncodingMode(run->subChannelType()) == -1) {
	log_message(-2, "Track %d: sub-channel writing mode is not supported by driver.", tracknr);
	ret = 2;
      }
    }
  }

  return ret;
}

// Returns block size for given mode and actual 'encodingMode_' that must
// be used to send data to the recorder.
long CdrDriver::blockSize(TrackData::Mode m,
			  TrackData::SubChannelMode sm) const
{
  long bsize = 0;

  if (encodingMode_ == 0) {
    // only audio blocks are written
    bsize = AUDIO_BLOCK_LEN;
  }
  else if (encodingMode_ == 1) {
    // encoding for SCSI-3/mmc drives in session-at-once mode
    switch (m) {
    case TrackData::AUDIO:
      bsize = AUDIO_BLOCK_LEN;
      break;
    case TrackData::MODE1:
    case TrackData::MODE1_RAW:
      bsize = MODE1_BLOCK_LEN;
      break;
    case TrackData::MODE2:
    case TrackData::MODE2_RAW:
    case TrackData::MODE2_FORM1:
    case TrackData::MODE2_FORM2:
    case TrackData::MODE2_FORM_MIX:
      bsize = MODE2_BLOCK_LEN;
      break;
    case TrackData::MODE0:
      log_message(-3, "Illegal mode in 'CdrDriver::blockSize()'.");
      break;
    }
  }
  else {
    log_message(-3, "Illegal encoding mode in 'CdrDriver::blockSize()'.");
  }

  bsize += TrackData::subChannelSize(sm);

  return bsize;
}

void CdrDriver::printCdToc(CdToc *toc, int tocLen)
{
  int t;
  long len;

  log_message(1, "Track   Mode    Flags  Start                Length");
  log_message(1, "------------------------------------------------------------");

  for (t = 0; t < tocLen; t++) {
    if (t == tocLen - 1) {
      log_message(1, "Leadout %s   %x      %s(%6ld)",
	      (toc[t].adrCtl & 0x04) != 0 ? "DATA " : "AUDIO",
	      toc[t].adrCtl & 0x0f,
	      Msf(toc[t].start).str(), toc[t].start);
    }
    else {
      len = toc[t + 1].start - toc[t].start;
      log_message(1, "%2d      %s   %x      %s(%6ld) ", toc[t].track,
	      (toc[t].adrCtl & 0x04) != 0 ? "DATA " : "AUDIO",
	      toc[t].adrCtl & 0x0f,
	      Msf(toc[t].start).str(), toc[t].start);
      log_message(1, "    %s(%6ld)", Msf(len).str(), len);
    }
  }
}


TrackData::Mode CdrDriver::getTrackMode(int, long trackStartLba)
{
  unsigned char cmd[10];
  unsigned char data[2340];
  int blockLength = 2340;
  TrackData::Mode mode;

  if (setBlockSize(blockLength) != 0) {
    return TrackData::MODE0;
  }

  memset(cmd, 0, 10);

  cmd[0] = 0x28; // READ10
  cmd[2] = trackStartLba >> 24;
  cmd[3] = trackStartLba >> 16;
  cmd[4] = trackStartLba >> 8;
  cmd[5] = trackStartLba;
  cmd[8] = 1;

  if (sendCmd(cmd, 10, NULL, 0, data, blockLength) != 0) {
    setBlockSize(MODE1_BLOCK_LEN);
    return TrackData::MODE0;
  }
  
  setBlockSize(MODE1_BLOCK_LEN);

  mode = determineSectorMode(data);

  if (mode == TrackData::MODE0) {
    log_message(-2, "Found illegal mode in sector %ld.", trackStartLba);
  }

  return mode;
}

TrackData::Mode CdrDriver::determineSectorMode(unsigned char *buf)
{
  switch (buf[3]) {
  case 1:
    return TrackData::MODE1;
    break;

  case 2:
    return analyzeSubHeader(buf + 4);
    break;
  }

  // illegal mode found
  return TrackData::MODE0;
}

// Analyzes given 8 byte sub head and tries to determine if it belongs
// to a form 1, form 2 or a plain mode 2 sector.
TrackData::Mode CdrDriver::analyzeSubHeader(unsigned char *sh)
{
  if (sh[0] == sh[4] && sh[1] == sh[5] && sh[2] == sh[6] && sh[3] == sh[7]) {
    // check first copy
    //if (sh[0] < 8 && sh[1] < 8 && sh[2] != 0) {
    if ((sh[2] & 0x20) != 0)
      return TrackData::MODE2_FORM2;
    else
      return TrackData::MODE2_FORM1;
    //}

#if 0
    // check second copy
    if (sh[4] < 8 && sh[5] < 8 && sh[6] != 0) {
      if (sh[6] & 0x20 != 0)
	return TrackData::MODE2_FORM2;
      else
	return TrackData::MODE2_FORM1;
    }
#endif
  }
  else {
    // no valid sub-header data, sector is a plain MODE2 sector
    return TrackData::MODE2;
  }
}


// Sets block size for read/write operation to given value.
// blocksize: block size in bytes
// density: (optional, default: 0) density code
// Return: 0: OK
//         1: SCSI command failed
int CdrDriver::setBlockSize(long blocksize, unsigned char density)
{
  unsigned char cmd[10];
  unsigned char ms[16];

  if (blockLength_ == blocksize)
    return 0;

  memset(ms, 0, 16);

  ms[3] = 8;
  ms[4] = density;
  ms[10] = blocksize >> 8;
  ms[11] = blocksize;

  memset(cmd, 0, 10);

  cmd[0] = 0x15; // MODE SELECT6
  cmd[4] = 12;

  if (sendCmd(cmd, 6, ms, 12, NULL, 0) != 0) {
    log_message(-2, "Cannot set block size.");
    return 1;
  }

  blockLength_ = blocksize;

  return 0;
}


// Returns control flags for given track.		  
unsigned char CdrDriver::trackCtl(const Track *track)
{
  unsigned char ctl = 0;

  if (track->copyPermitted()) {
    ctl |= 0x20;
  }

  if (track->type() == TrackData::AUDIO) {
    // audio track
    if (track->preEmphasis()) {
      ctl |= 0x10;
    }
    if (track->audioType() == 1) {
      ctl |= 0x80;
    }
  }
  else {
    // data track
    ctl |= 0x40;
  }
  
  return ctl;
}

// Returns session format for point A0 toc entry depending on Toc type.
unsigned char CdrDriver::sessionFormat()
{
  unsigned char ret = 0;
  int nofMode1Tracks;
  int nofMode2Tracks;
  
  switch (toc_->tocType()) {
  case Toc::CD_DA:
  case Toc::CD_ROM:
    ret = 0x00;
    break;

  case Toc::CD_I:
    ret = 0x10;
    break;

  case Toc::CD_ROM_XA:
    /* The toc type can only be set to CD_ROM_XA if the session contains
       at least one data track. Otherwise the toc type must be CD_DA even
       in multi session mode.
    */
    toc_->trackSummary(NULL, &nofMode1Tracks, &nofMode2Tracks);
    if (nofMode1Tracks + nofMode2Tracks > 0)
      ret = 0x20;
    else
      ret = 0x0;
    break;
  }

  log_message(3, "Session format: %x", ret);

  return ret;
}

// Generic method to read CD-TEXT packs according to the MMC-2 specification.
// nofPacks: filled with number of found CD-TEXT packs
// return: array of CD-TEXT packs or 'NULL' if no packs where retrieved
CdTextPack *CdrDriver::readCdTextPacks(long *nofPacks)
{
  unsigned char cmd[12];
  unsigned char *data;
  unsigned char reqData[4];

#if 0
  memset(cmd, 0, 12);

  cmd[0] = 0xbe;
  cmd[2] = 0xf0;
  cmd[3] = 0x00;
  cmd[4] = 0x00;
  cmd[5] = 0x00;
  cmd[8] = 15;
  cmd[9] = 0x0;
  cmd[10] = 0x1;
  
  long len1 = 15 * (AUDIO_BLOCK_LEN + 96);
  data = new unsigned char [len1];

  if (sendCmd(cmd, 12, NULL, 0, data, len1) != 0) {
    log_message(1, "Cannot read raw CD-TEXT data.");
  }

  long i, j;
  unsigned char *p = data + AUDIO_BLOCK_LEN;

  log_message(0, "Raw CD-TEXT data");

  for (i = 0; i < 15; i++) {
    unsigned char packs[72];
    PWSubChannel96 chan(p);

    chan.getRawRWdata(packs);

    for (j = 0; j < 4; j++) {
      log_message(0, "%02x %02x %02x %02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x  CRC: %02x %02x", 
	      packs[j*18+0], packs[j*18+1], packs[j*18+2], packs[j*18+3], 
	      packs[j*18+4], packs[j*18+5], packs[j*18+6], packs[j*18+7], 
	      packs[j*18+8], packs[j*18+9], packs[j*18+10], packs[j*18+11], 
	      packs[j*18+12], packs[j*18+13], packs[j*18+14], packs[j*18+15], 
	      packs[j*18+16], packs[j*18+17]);
    }

    p += AUDIO_BLOCK_LEN + 96;
  }
      
  delete[] data;


  log_message(0, "Raw CD-TEXT data - end");
#endif

  memset(cmd, 0, 10);

  cmd[0] = 0x43; // READ TOC/PMA/ATIP
  cmd[2] = 5;    // CD-TEXT
  cmd[8] = 4;

  if (sendCmd(cmd, 10, NULL, 0, reqData, 4, 0) != 0) {
    log_message(3, "Cannot read CD-TEXT data - maybe not supported by drive.");
    return NULL;
  }

  long len = ((reqData[0] << 8 ) | reqData[1]) + 2;

  log_message(4, "CD-TEXT data len: %ld", len);

  if (len <= 4)
    return NULL;

  if (len > scsiMaxDataLen_) {
    log_message(-2, "CD-TEXT data too big for maximum SCSI transfer length.");
    return NULL;
  }

  data = new unsigned char[len];

  cmd[7] = len >> 8;
  cmd[8] = len;

  if (sendCmd(cmd, 10, NULL, 0, data, len, 1) != 0) {
    log_message(-2, "Reading of CD-TEXT data failed.");
    delete[] data;
    return NULL;
  }
  
  *nofPacks = (len - 4) / sizeof(CdTextPack);

  CdTextPack *packs = new CdTextPack[*nofPacks];

  memcpy(packs, data + 4, *nofPacks * sizeof(CdTextPack));

  delete[] data;

  return packs;
}

// Analyzes CD-TEXT packs and stores read data in given 'Toc' object.
// Return: 0: OK
//         1: error occured
int CdrDriver::readCdTextData(Toc *toc)
{
  long i, j;
  long nofPacks;
  CdTextPack *packs = readCdTextPacks(&nofPacks);
  unsigned char buf[256 * 12];
  unsigned char lastType;
  int lastBlockNumber;
  int blockNumber;
  int pos;
  int actTrack;
  CdTextItem::PackType packType;
  CdTextItem *sizeInfoItem = NULL;
  CdTextItem *item;

  if (packs == NULL)
    return 1;

  log_message(1, "Found CD-TEXT data.");

  pos = 0;
  lastType = packs[0].packType;
  lastBlockNumber = (packs[0].blockCharacter >> 4) & 0x07;
  actTrack = 0;

  for (i = 0; i < nofPacks; i++) {
    CdTextPack &p = packs[i];

#if 1
    log_message(4, "%02x %02x %02x %02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x  CRC: %02x %02x", p.packType, p.trackNumber,
	    p.sequenceNumber, p.blockCharacter, p.data[0], p.data[1], 
	    p.data[2], p.data[3], p.data[4], p.data[5], p.data[6], p.data[7], 
	    p.data[8], p.data[9], p.data[10], p.data[11],
	    p.crc0, p.crc1);
#endif

    blockNumber = (p.blockCharacter >> 4) & 0x07;

    if (lastType != p.packType || lastBlockNumber != blockNumber) {
      if (lastType >= 0x80 && lastType <= 0x8f) {
	packType = CdTextItem::int2PackType(lastType);

	if (CdTextItem::isBinaryPack(packType)) {
	  // finish binary data

	  if (packType == CdTextItem::CDTEXT_GENRE) {
	    // The two genre codes may be followed by a string. Adjust 'pos'
	    // so that all extra 0 bytes at the end of the data are stripped
	    // off.
	    for (j = 2; j < pos && buf[j] != 0; j++) ;
	    if (j < pos)
	      pos = j + 1;
	  }

	  item = new CdTextItem(packType, lastBlockNumber, buf, pos);

	  if (packType == CdTextItem::CDTEXT_SIZE_INFO)
	    sizeInfoItem = item;

	  toc->addCdTextItem(0, item);
	}
      }
      else {
	log_message(-2, "CD-TEXT: Found invalid pack type: %02x", lastType);
	delete[] packs;
	return 1;
      }

      lastType = p.packType;
      lastBlockNumber = blockNumber;
      pos = 0;
      actTrack = 0;
    }

    if (p.packType >= 0x80 && p.packType <= 0x8f) {
      packType = CdTextItem::int2PackType(p.packType);

      if (CdTextItem::isBinaryPack(packType)) {
	memcpy(buf + pos, p.data, 12);
	pos += 12;
      }
      else {
	// pack contains text -> read all string from it
	j = 0;

	while (j < 12 && actTrack <= toc->nofTracks()) {
	  for (; j < 12 && p.data[j] != 0; j++)
	    buf[pos++] = p.data[j];

	  if (j < 12) {
	    // string is finished
	    buf[pos] = 0;

#if 0	
	    log_message(0, "%02x %02x: %s", p.packType, p.trackNumber, buf);
#endif

	    toc->addCdTextItem(actTrack,
			       new CdTextItem(packType, blockNumber,
					      (char*)buf));
	    actTrack++;
	    pos = 0;

	    if (CdTextItem::isTrackPack(packType)) {
	      j++; // skip zero
	    }
	    else {
	      j = 12; // don't use remaining zeros to build track packs
	    }
	  }
	}
      }
    }
    else {
      log_message(-2, "CD-TEXT: Found invalid pack type: %02x", p.packType);
      delete[] packs;
      return 1;
    }
  }

  if (pos != 0 && lastType >= 0x80 && lastType <= 0x8f) {
    packType = CdTextItem::int2PackType(lastType);

    if (CdTextItem::isBinaryPack(packType)) {
      // finish binary data

      if (packType == CdTextItem::CDTEXT_GENRE) {
	// The two genre codes may be followed by a string. Adjust 'pos'
	// so that all extra 0 bytes at the end of the data are stripped
	// off.
	for (j = 2; j < pos && buf[j] != 0; j++) ;
	if (j < pos)
	  pos = j + 1;
      }

      item = new CdTextItem(packType, lastBlockNumber, buf, pos);
      toc->addCdTextItem(0, item);

      if (packType == CdTextItem::CDTEXT_SIZE_INFO)
	sizeInfoItem = item;
    }
  }

  delete[] packs;

  // update language mapping from SIZE INFO pack data
  if (sizeInfoItem != NULL && sizeInfoItem->dataLen() >= 36) {
    const unsigned char *data = sizeInfoItem->data();
    for (i = 0; i < 8; i++) {
      if (data[28 + i] > 0)
	toc->cdTextLanguage(i, data[28 + i]);
      else
	toc->cdTextLanguage(i, -1);
    }
  }
  else {
    log_message(-1, "Cannot determine language mapping from CD-TEXT data.");
    log_message(-1, "Using default mapping.");
  }

  return 0;
}

int CdrDriver::analyzeDataTrack(TrackData::Mode mode, int trackNr,
				long startLba, long endLba, long *pregap)
{
  long maxLen = scsiMaxDataLen_ / AUDIO_BLOCK_LEN;
  long lba = startLba;
  long len = endLba - startLba;
  long actLen, n;

  *pregap = 0;

  while (len > 0) {
    n = len > maxLen ? maxLen : len;

    if ((actLen = readTrackData(mode, TrackData::SUBCHAN_NONE, lba, n,
				transferBuffer_)) < 0) {
      log_message(-2, "Analyzing of track %d failed.", trackNr);
      return 1;
    }

    log_message(1, "%s\r", Msf(lba).str());

    if (actLen != n) {
      //log_message(0, "Data track pre-gap: %ld", len - actLen);
      *pregap = len - actLen;

      if (*pregap > 300) {
	log_message(-1,
		"The pre-gap of the following track appears to have length %s.",
		Msf(*pregap).str());
	log_message(-1, "This value is probably bogus and may be caused by unexpected");
	log_message(-1, "behavior of the drive. Try to verify with other tools how");
	log_message(-1, "much data can be read from the current track and compare it");
	log_message(-1, "to the value stored in the toc-file. Usually, the pre-gap");
	log_message(-1, "should have length 00:02:00.");
      }

      return 0;
    }

    len -= n;
    lba += n;
  }

  return 0;
}

/* Reads toc and audio data from CD for specified 'session' number.
 * The data is written to file 'dataFilename' unless on-the-fly writing
 * is active in which case the data is written to the file descriptor
 * 'onTheFlyFd'.
 * Return: newly created 'Toc' object or 'NULL' if an error occured
 */

Toc *CdrDriver::readDisk(int session, const char *dataFilename)
{
  int padFirstPregap = 1;
  int nofTracks = 0;
  int i;
  CdToc *cdToc = getToc(session, &nofTracks);
  //unsigned char trackCtl; // control nibbles of track
  //int ctlCheckOk;
  TrackInfo *trackInfos;
  TrackData::Mode trackMode;
  int fp = -1;
  char *fname = strdupCC(dataFilename);
  Toc *toc = NULL;
  int trs = 0;
  int tre = 0;
  long slba, elba;
  ReadDiskInfo info;

  if (cdToc == NULL) {
    return NULL;
  }

  if (nofTracks <= 1) {
    log_message(-1, "No tracks on disk.");
    delete[] cdToc;
    return NULL;
  }

  log_message(1, "");
  printCdToc(cdToc, nofTracks);
  log_message(1, "");
  //return NULL;

  nofTracks -= 1; // do not count lead-out

  readCapabilities_ = getReadCapabilities(cdToc, nofTracks);

  trackInfos = new TrackInfo[nofTracks + 1];
  
  for (i = 0; i < nofTracks; i++) {
    if ((cdToc[i].adrCtl & 0x04) != 0) {
      if ((trackMode = getTrackMode(i + 1, cdToc[i].start)) ==
	  TrackData::MODE0) {
	log_message(-1, "Cannot determine mode of data track %d - asuming MODE1.",
		i + 1);
	trackMode = TrackData::MODE1;
      }

      if (rawDataReading_) {
	if (trackMode == TrackData::MODE1) {
	  trackMode = TrackData::MODE1_RAW;
	}
	else if (trackMode == TrackData::MODE2_FORM1 ||
		 trackMode == TrackData::MODE2_FORM2 ||
		 trackMode == TrackData::MODE2_FORM_MIX) {
	  trackMode = TrackData::MODE2_RAW;
	}
      }
      else if (mode2Mixed_) {
	if (trackMode == TrackData::MODE2_FORM1 ||
	    trackMode == TrackData::MODE2_FORM2 ||
	    trackMode == TrackData::MODE2_FORM_MIX) {
	  trackMode = TrackData::MODE2_FORM_MIX;
	}
      }
    }
    else {
      trackMode = TrackData::AUDIO;
    }

    if (!checkSubChanReadCaps(trackMode, readCapabilities_)) {
      log_message(-2, "This drive does not support %s sub-channel reading.",
	      TrackData::subChannelMode2String(subChanReadMode_));
      goto fail;
    }

    trackInfos[i].trackNr = cdToc[i].track;
    trackInfos[i].ctl = cdToc[i].adrCtl & 0x0f;
    trackInfos[i].mode = trackMode;
    trackInfos[i].start = cdToc[i].start;
    trackInfos[i].pregap = 0;
    trackInfos[i].fill = 0;
    trackInfos[i].indexCnt = 0;
    trackInfos[i].isrcCode[0] = 0;
    trackInfos[i].filename = fname;
    trackInfos[i].bytesWritten = 0;
  }

  // lead-out entry
  trackInfos[nofTracks].trackNr = 0xaa;
  trackInfos[nofTracks].ctl = 0;
  trackInfos[nofTracks].mode = trackInfos[nofTracks - 1].mode;
  trackInfos[nofTracks].start = cdToc[nofTracks].start;
  if (taoSource()) {
    trackInfos[nofTracks].start -= taoSourceAdjust_;
  }
  trackInfos[nofTracks].pregap = 0;
  trackInfos[nofTracks].indexCnt = 0;
  trackInfos[nofTracks].isrcCode[0] = 0;
  trackInfos[nofTracks].filename = NULL;
  trackInfos[nofTracks].bytesWritten = 0;


  if (onTheFly_) {
    fp = onTheFlyFd_;
  }
  else {

    giveUpRootPrivileges();

#ifdef __CYGWIN__
    if ((fp = open(dataFilename, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,
		   0666)) < 0)
#else
    if ((fp = open(dataFilename, O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0)
#endif
    {
      log_message(-2, "Cannot open \"%s\" for writing: %s", dataFilename,
	      strerror(errno));
      delete[] cdToc;
      return NULL;
    }
  }

  info.tracks = nofTracks;
  info.startLba = trackInfos[0].start;
  info.endLba = trackInfos[nofTracks].start;


  while (trs < nofTracks) {
    if (trackInfos[trs].mode != TrackData::AUDIO) {
      if (trs == 0) {
	if (session == 1)
	  trackInfos[trs].pregap = trackInfos[trs].start;
      }
      else {
	if (taoSource()) {
	  trackInfos[trs].pregap = 150 + taoSourceAdjust_;
	}
	else {
	  if (trackInfos[trs].mode != trackInfos[trs - 1].mode)
	    trackInfos[trs].pregap = 150;
	}
      }

      slba = trackInfos[trs].start;
      elba = trackInfos[trs + 1].start;

      if (taoSource()) {
	if (trs < nofTracks - 1)
	  elba -= 150 + taoSourceAdjust_;
      }
      else {
	if (trackInfos[trs].mode != trackInfos[trs + 1].mode) {
	  elba -= 150;
	}
      }


      log_message(1, "Copying data track %d (%s): start %s, ", trs + 1, 
	      TrackData::mode2String(trackInfos[trs].mode),
	      Msf(cdToc[trs].start).str());
      log_message(1, "length %s to \"%s\"...", Msf(elba - slba).str(),
	      trackInfos[trs].filename);
      
      if (readDataTrack(&info, fp, slba, elba, &trackInfos[trs]) != 0)
	goto fail;

      trs++;
    }
    else {
      // find continuous range of audio tracks
      tre = trs;
      while (tre < nofTracks && trackInfos[tre].mode == TrackData::AUDIO)
	tre++;

      if (trs == 0) {
	if (session == 1)
	  trackInfos[trs].pregap = trackInfos[trs].start;
      }
      else {
	// previous track must be of different mode so assume a standard
	// pre-gap here
	if (taoSource()) {
	  trackInfos[trs].pregap = 150 + taoSourceAdjust_;
	}
	else {
	  trackInfos[trs].pregap = 150;
	}
      }

      slba = cdToc[trs].start;
      elba = cdToc[tre].start;

      // If we have the first track of the first session we can start ripping
      // from lba 0 to extract the pre-gap data.
      // But only if the drive supports it as indicated by 'options_'.
      if (session == 1 && trs == 0 &&
	  (options_ & OPT_DRV_NO_PREGAP_READ) == 0) {
	slba = 0;
      }

      // Assume that the pre-gap length conforms to the standard if the track
      // mode changes. 
      if (taoSource()) {
	if (tre < nofTracks)
	  elba -= 150 + taoSourceAdjust_;
      }
      else {
	if (trackInfos[tre - 1].mode != trackInfos[tre].mode) {
	  elba -= 150;
	}
      }

      log_message(1, "Copying audio tracks %d-%d: start %s, ", trs + 1, tre,
	      Msf(slba).str());
      log_message(1, "length %s to \"%s\"...", Msf(elba - slba).str(),
	      trackInfos[trs].filename);

      if (readAudioRange(&info, fp, slba, elba, trs, tre - 1, trackInfos) != 0)
	goto fail;

      trs = tre;
    }
  }

  // if the drive allows to read audio data from the first track's
  // pre-gap the data will be written to the output file and 
  // 'buildToc()' must not create zero data for the pre-gap
  padFirstPregap = 1;

  if (session == 1 && (options_ & OPT_DRV_NO_PREGAP_READ) == 0)
    padFirstPregap = 0;

  toc = buildToc(trackInfos, nofTracks + 1, padFirstPregap);

  if (!onTheFly_ && toc != NULL) {
    if ((options_ & OPT_DRV_NO_CDTEXT_READ) == 0)
      readCdTextData(toc);

    if (readCatalog(toc, trackInfos[0].start, trackInfos[nofTracks].start)) {
      log_message(2, "Found disk catalogue number.");
    }
  }

  sendReadCdProgressMsg(RCD_EXTRACTING, nofTracks, nofTracks, 1000, 1000);

fail:
  delete[] cdToc;
  delete[] trackInfos;

  delete[] fname;

  if (!onTheFly_ && fp >= 0) {
    if (close(fp) != 0) {
      log_message(-2, "Writing to \"%s\" failed: %s", dataFilename,
	      strerror(errno));
      delete toc;
      return NULL;
    }
  }

  return toc;
}


Toc *CdrDriver::buildToc(TrackInfo *trackInfos, long nofTrackInfos,
			 int padFirstPregap)
{
  long i, j;
  long nofTracks = nofTrackInfos - 1;
  int foundDataTrack = 0;
  int foundAudioTrack = 0;
  int foundXATrack = 0;
  long modeStartLba = 0; // start LBA for current mode
  unsigned long dataLen;
  TrackData::Mode trackMode;
  TrackData::Mode lastMode = TrackData::MODE0; // illegal in this context
  TrackData::SubChannelMode trackSubChanMode;
  int newMode;
  long byteOffset = 0;

  if (nofTrackInfos < 2)
    return NULL;

  Toc *toc = new Toc;

  // build the Toc

  for (i = 0; i < nofTracks; i++) {
    TrackInfo &ati = trackInfos[i]; // actual track info
    TrackInfo &nti = trackInfos[i + 1]; // next track info

    newMode = 0;
    trackMode = ati.mode;
    trackSubChanMode = subChanReadMode_;

    switch (trackMode) {
    case TrackData::AUDIO:
      foundAudioTrack = 1;
      break;
    case TrackData::MODE1:
    case TrackData::MODE1_RAW:
    case TrackData::MODE2:
      foundDataTrack = 1;
      break;
    case TrackData::MODE2_RAW:
    case TrackData::MODE2_FORM1:
    case TrackData::MODE2_FORM2:
    case TrackData::MODE2_FORM_MIX:
      foundXATrack = 1;
      break;
    case TrackData::MODE0:
      // should not happen
      break;
    }

    if (trackMode != lastMode) {
      newMode = 1;
      if (i == 0 && !padFirstPregap)
	modeStartLba = 0;
      else
	modeStartLba = ati.start;
      lastMode = trackMode;
    }
    
    Track t(trackMode, trackSubChanMode);

    t.preEmphasis(ati.ctl & 0x01);
    t.copyPermitted(ati.ctl & 0x02);
    t.audioType(ati.ctl & 0x08);

    if (ati.isrcCode[0] != 0)
      t.isrc(ati.isrcCode);

    if (trackMode == TrackData::AUDIO) {
      if (trackSubChanMode == TrackData::SUBCHAN_NONE) {
	if (newMode && (i > 0 || padFirstPregap)) {
	  Msf trackLength(nti.start - ati.start - nti.pregap);
	  if (ati.pregap > 0) {
	    t.append(SubTrack(SubTrack::DATA,
			      TrackData(Msf(ati.pregap).samples())));
	  }
	  t.append(SubTrack(SubTrack::DATA,
			    TrackData(ati.filename, byteOffset,
				      0, trackLength.samples())));
	}
	else {
	  Msf trackLength(nti.start - ati.start - nti.pregap + ati.pregap);
	  t.append(SubTrack(SubTrack::DATA,
			    TrackData(ati.filename, byteOffset,
				      Msf(ati.start - modeStartLba
					  - ati.pregap).samples(), 
				      trackLength.samples())));
	}
      }
      else {
	if (newMode && (i > 0 || padFirstPregap)) {
	  long trackLength = nti.start - ati.start - nti.pregap;
	  
	  if (ati.pregap > 0) {
	    dataLen = ati.pregap * TrackData::dataBlockSize(trackMode,
							    trackSubChanMode);
	    t.append(SubTrack(SubTrack::DATA,
			      TrackData(trackMode, trackSubChanMode,
					dataLen)));
	  }
	  dataLen = trackLength * TrackData::dataBlockSize(trackMode,
							    trackSubChanMode);
	  t.append(SubTrack(SubTrack::DATA,
			    TrackData(trackMode, trackSubChanMode,
				      ati.filename, byteOffset, dataLen)));
	}
	else {
	  long trackLength = nti.start - ati.start - nti.pregap + ati.pregap;
	  dataLen = trackLength * TrackData::dataBlockSize(trackMode,
							   trackSubChanMode);
	  long offset =
	    (ati.start - modeStartLba - ati.pregap) *
	    TrackData::dataBlockSize(trackMode, trackSubChanMode);
	    

	  t.append(SubTrack(SubTrack::DATA,
			    TrackData(trackMode, trackSubChanMode,
				      ati.filename, byteOffset + offset,
				      dataLen)));
	}
      }

      t.start(Msf(ati.pregap));
    }
    else {
      long trackLength = nti.start - ati.start - nti.pregap - ati.fill;

      if (ati.pregap != 0) {
	// add zero data for pre-gap
	dataLen = ati.pregap * TrackData::dataBlockSize(trackMode,
							trackSubChanMode);
	t.append(SubTrack(SubTrack::DATA,
			  TrackData(trackMode, trackSubChanMode, dataLen)));
      }
      
      dataLen = trackLength * TrackData::dataBlockSize(trackMode,
						       trackSubChanMode);
      t.append(SubTrack(SubTrack::DATA,
			TrackData(trackMode, trackSubChanMode, ati.filename,
				  byteOffset, dataLen)));

      if (ati.fill > 0) {
	dataLen =  ati.fill * TrackData::dataBlockSize(trackMode,
						       trackSubChanMode);
	t.append(SubTrack(SubTrack::DATA,
			  TrackData(trackMode, trackSubChanMode, dataLen)));
      }

      t.start(Msf(ati.pregap));
    }

    for (j = 0; j < ati.indexCnt; j++)
      t.appendIndex(Msf(ati.index[j]));

    toc->append(&t);

    byteOffset += ati.bytesWritten;
  }

  if (foundXATrack)
    toc->tocType(Toc::CD_ROM_XA);
  else if (foundDataTrack)
    toc->tocType(Toc::CD_ROM);
  else
    toc->tocType(Toc::CD_DA);

  return toc;
}

// Reads a complete data track.
// start: start of data track from TOC
// end: start of next track from TOC
// trackInfo: info about current track, updated by this function
// Return: 0: OK
//         1: error occured
int CdrDriver::readDataTrack(ReadDiskInfo *info, int fd, long start, long end,
			     TrackInfo *trackInfo)
{
  long len = end - start;
  long totalLen = len;
  long lba;
  long lastLba;
  long blockLen = 0;
  long blocking;
  long burst;
  long iterationsWithoutError = 0;
  long n, ret;
  long act;
  int foundLECError;
  unsigned char *buf;
  TrackData::Mode mode = TrackData::AUDIO;

  switch (trackInfo->mode) {
  case TrackData::MODE1:
    mode = TrackData::MODE1;
    blockLen = MODE1_BLOCK_LEN;
    break;
  case TrackData::MODE1_RAW:
    mode = TrackData::MODE1_RAW;
    blockLen = AUDIO_BLOCK_LEN;
    break;
  case TrackData::MODE2:
    mode = TrackData::MODE2;
    blockLen = MODE2_BLOCK_LEN;
    break;
  case TrackData::MODE2_RAW:
    mode = TrackData::MODE2_RAW;
    blockLen = AUDIO_BLOCK_LEN;
    break;
  case TrackData::MODE2_FORM1:
    mode = TrackData::MODE2_FORM1;
    blockLen = MODE2_FORM1_DATA_LEN;
    break;
  case TrackData::MODE2_FORM2:
    mode = TrackData::MODE2_FORM2;
    blockLen = MODE2_FORM2_DATA_LEN;
    break;
  case TrackData::MODE2_FORM_MIX:
    mode = TrackData::MODE2_FORM_MIX;
    blockLen = MODE2_BLOCK_LEN;
    break;
  case TrackData::MODE0:
  case TrackData::AUDIO:
    log_message(-3, "CdrDriver::readDataTrack: Illegal mode.");
    return 1;
    break;
  }

  blockLen += TrackData::subChannelSize(subChanReadMode_);

  // adjust mode in 'trackInfo'
  trackInfo->mode = mode;

  trackInfo->bytesWritten = 0;

  blocking = scsiMaxDataLen_ / (AUDIO_BLOCK_LEN + PW_SUBCHANNEL_LEN);
  assert(blocking > 0);

  buf = new unsigned char[blocking * blockLen];

  lba = lastLba = start;
  burst = blocking;

  while (len > 0) {
    if (burst != blocking && iterationsWithoutError > 2 * blocking)
      burst = blocking;

    n = (len > burst) ? burst : len;

    foundLECError = 0;

    if ((act = readTrackData(mode, subChanReadMode_, lba, n, buf)) == -1) {
      log_message(-2, "Read error while copying data from track.");
      delete[] buf;
      return 1;
    }

    if (act == -2) {
      // L-EC error encountered
      if (trackInfo->mode == TrackData::MODE1_RAW ||
	  trackInfo->mode == TrackData::MODE2_RAW) {

	if (n > 1) {
	  // switch to single step mode
	  iterationsWithoutError = 0;
	  burst = 1;
	  continue;
	}
	else {
	  foundLECError = 1;
	  act = n = 0;
	}
      }
      else {
	log_message(-2, "L-EC error around sector %ld while copying data from track.", lba);
	log_message(-2, "Use option '--read-raw' to ignore L-EC errors.");
	delete[] buf;
	return 1;
      }
    }

    if (foundLECError) {
      iterationsWithoutError = 0;

      log_message(2, "Found L-EC error at sector %ld - ignored.", lba);

      // create a dummy sector for the sector with L-EC errors
      Msf m(lba + 150);

      memcpy(buf, syncPattern, 12);
      buf[12] = SubChannel::bcd(m.min());
      buf[13] = SubChannel::bcd(m.sec());
      buf[14] = SubChannel::bcd(m.frac());
      if (trackInfo->mode == TrackData::MODE1_RAW)
	buf[15] = 1;
      else
	buf[15] = 2;

      memcpy(buf + 16, SECTOR_ERROR_DATA, blockLen - 16);

      if ((ret = fullWrite(fd, buf, blockLen)) != blockLen) {
	if (ret < 0)
	  log_message(-2, "Writing of data failed: %s", strerror(errno));
	else
	  log_message(-2, "Writing of data failed: Disk full");
	  
	delete[] buf;
	return 1;
      }

      trackInfo->bytesWritten += blockLen;

      lba += 1;
      len -= 1;
    }
    else {
      iterationsWithoutError++;

      if (act > 0) {
	if ((ret = fullWrite(fd, buf, blockLen * act)) != blockLen * act) {
	  if (ret < 0)
	    log_message(-2, "Writing of data failed: %s", strerror(errno));
	  else
	    log_message(-2, "Writing of data failed: Disk full");
	  
	  delete[] buf;
	  return 1;
	}
      }

      trackInfo->bytesWritten += blockLen * act;

      if (lba > lastLba + 75) {
	Msf lbatime(lba);
	log_message(1, "%02d:%02d:00\r", lbatime.min(), lbatime.sec());
	lastLba = lba;

	if (remote_) {
	  long totalProgress;
	  long progress;

	  progress = (totalLen - len) * 1000;
	  progress /= totalLen;

	  totalProgress = lba - info->startLba;

	  if (totalProgress > 0) {
	    totalProgress *= 1000;
	    totalProgress /= (info->endLba - info->startLba);
	  }
	  else {
	    totalProgress = 0;
	  }

	  sendReadCdProgressMsg(RCD_EXTRACTING, info->tracks, 
				trackInfo->trackNr, progress, totalProgress);
	}
      }

      lba += act;
      len -= act;

      if (act != n)
	break;
    }
  }

  // pad remaining blocks with zero data, e.g. for disks written in TAO mode

  if (len > 0) {
    log_message(-1, "Padding with %ld zero sectors.", len);

    if (mode == TrackData::MODE1_RAW || mode == TrackData::MODE2_RAW) {
      memcpy(buf, syncPattern, 12);

      if (mode == TrackData::MODE1_RAW)
	buf[15] = 1;
      else
	buf[15] = 2;

      memset(buf + 16, 0, blockLen - 16);
    }
    else {
      memset(buf, 0, blockLen);
    }
    
    while (len > 0) {
      if (mode == TrackData::MODE1_RAW || mode == TrackData::MODE2_RAW) {
	Msf m(lba + 150);

	buf[12] = SubChannel::bcd(m.min());
	buf[13] = SubChannel::bcd(m.sec());
	buf[14] = SubChannel::bcd(m.frac());
      }

      if ((ret = fullWrite(fd, buf, blockLen)) != blockLen) {
	if (ret < 0)
	  log_message(-2, "Writing of data failed: %s", strerror(errno));
	else
	  log_message(-2, "Writing of data failed: Disk full");
	  
	delete[] buf;
	return 1;
      }

      trackInfo->bytesWritten += blockLen;
      
      len--;
      lba++;
    }
  }

  delete[] buf;
  
  return 0;
}

// Tries to read the catalog number from the sub-channels starting at LBA 0.
// If a catalog number is found it will be placed into the provided 14 byte
// buffer 'mcnCode'. Otherwise 'mcnCode[0]' is set to 0.
// Return: 0: OK
//         1: SCSI error occured
#define N_ELEM 11
#define MCN_LEN 13
#define MAX_MCN_SCAN_LENGTH 5000

static int cmpMcn(const void *p1, const void *p2)
{
  const char *s1 = (const char *)p1;
  const char *s2 = (const char *)p2;
  
  return strcmp(s1, s2);
}

int CdrDriver::readCatalogScan(char *mcnCode, long startLba, long endLba)
{

  SubChannel **subChannels;
  int n, i;
  long length;
  int mcnCodeFound = 0;
  char mcn[N_ELEM][MCN_LEN+1];

  *mcnCode = 0;

  length = endLba - startLba;

  if (length > MAX_MCN_SCAN_LENGTH)
    length = MAX_MCN_SCAN_LENGTH;


  while ((length > 0) && (mcnCodeFound < N_ELEM)) {
    n = (length > maxScannedSubChannels_ ? maxScannedSubChannels_ : length);

    if (readSubChannels(TrackData::SUBCHAN_NONE, startLba, n, &subChannels,
			NULL) != 0 ||
	subChannels == NULL) {
      return 1;
    }

    for (i = 0; i < n; i++) {
      SubChannel *chan = subChannels[i];
      //chan->print();

      if (chan->checkCrc() && chan->checkConsistency()) {
        if (chan->type() == SubChannel::QMODE2) {
          if (mcnCodeFound < N_ELEM) {
            strcpy(mcn[mcnCodeFound++], chan->catalog());
          }
        }
      }
    }

    length -= n;
    startLba += n;
  }

  if(mcnCodeFound > 0) {
    qsort(mcn, mcnCodeFound, MCN_LEN + 1, cmpMcn);
    strcpy(mcnCode, mcn[(mcnCodeFound >> 1)]);
  }

  return 0;
}

#undef N_ELEM
#undef MCN_LEN
#undef MAX_MCN_SCAN_LENGTH


// Sends a read cd progress message without blocking the actual process.
void CdrDriver::sendReadCdProgressMsg(ReadCdProgressType type, int totalTracks,
				      int track, int trackProgress,
				      int totalProgress)
{
  if (remote_) {
    int fd = remoteFd_;
    ProgressMsg p;

    p.status = type;
    p.totalTracks = totalTracks;
    p.track = track;
    p.trackProgress = trackProgress;
    p.totalProgress = totalProgress;
    p.bufferFillRate = 0;
    p.writerFillRate = 0;

    if (write(fd, REMOTE_MSG_SYNC_, sizeof(REMOTE_MSG_SYNC_)) != sizeof(REMOTE_MSG_SYNC_) ||
	write(fd, (const char*)&p, sizeof(p)) != sizeof(p)) {
      log_message(-1, "Failed to send read CD remote progress message.");
    }
  }
}

// Sends a write cd progress message without blocking the actual process.
int CdrDriver::sendWriteCdProgressMsg(WriteCdProgressType type, 
				      int totalTracks, int track,
				      int trackProgress, int totalProgress,
				      int bufferFillRate, int writeFill)
{
  if (remote_) {
    int fd = remoteFd_;
    ProgressMsg p;

    p.status = type;
    p.totalTracks = totalTracks;
    p.track = track;
    p.trackProgress = trackProgress;
    p.totalProgress = totalProgress;
    p.bufferFillRate = bufferFillRate;
    p.writerFillRate = writeFill;

    if (write(fd, REMOTE_MSG_SYNC_, sizeof(REMOTE_MSG_SYNC_)) != sizeof(REMOTE_MSG_SYNC_) ||
	write(fd, (const char*)&p, sizeof(p)) != sizeof(p)) {
      log_message(-1, "Failed to send write CD remote progress message.");
      return 1;
    }
  }

  return 0;
}

// Sends a blank cd progress message without blocking the actual process.
int CdrDriver::sendBlankCdProgressMsg(int totalProgress)
{
  if (remote_) {
    int fd = remoteFd_;
    ProgressMsg p;

    p.status = PGSMSG_BLK;
    p.totalTracks = 0;
    p.track = 0;
    p.trackProgress = 0;
    p.totalProgress = totalProgress;
    p.bufferFillRate = 0;
    p.writerFillRate = 0;

    if (write(fd, REMOTE_MSG_SYNC_, sizeof(REMOTE_MSG_SYNC_)) != sizeof(REMOTE_MSG_SYNC_) ||
	write(fd, (const char*)&p, sizeof(p)) != sizeof(p)) {
      log_message(-1, "Failed to send write CD remote progress message.");
      return 1;
    }
  }

  return 0;
}

long CdrDriver::audioRead(TrackData::SubChannelMode sm, int byteOrder,
			  Sample *buffer, long startLba, long len)
{
  SubChannel **chans;
  int i;
  int swap;
  long blockLen = AUDIO_BLOCK_LEN + TrackData::subChannelSize(sm);

  if (readSubChannels(sm, startLba, len, &chans, buffer) != 0) {
    memset(buffer, 0, len * blockLen);
    audioReadError_ = 1;
    return len;
  }

  swap = (audioDataByteOrder_ == byteOrder) ? 0 : 1;

  if (options_ & OPT_DRV_SWAP_READ_SAMPLES)
    swap = !swap;

  if (swap) {
    unsigned char *b = (unsigned char*)buffer;
    
    for (i = 0; i < len; i++) {
      swapSamples((Sample *)b, SAMPLES_PER_BLOCK);
      b += blockLen;
    }
  }


  if (remote_ && startLba > audioReadLastLba_) {
    long totalTrackLen = audioReadTrackInfo_[audioReadActTrack_ + 1].start -
                         audioReadTrackInfo_[audioReadActTrack_ ].start;
    long progress = startLba - audioReadTrackInfo_[audioReadActTrack_ ].start;
    long totalProgress;

    if (progress > 0) {
      progress *= 1000;
      progress /= totalTrackLen;
    }
    else {
      progress = 0;
    }

    totalProgress = startLba + len - audioReadInfo_->startLba;

    if (totalProgress > 0) {
      totalProgress *= 1000;
      totalProgress /= audioReadInfo_->endLba - audioReadInfo_->startLba;
    }
    else {
      totalProgress = 0;
    }

    sendReadCdProgressMsg(RCD_EXTRACTING, audioReadInfo_->tracks,
			  audioReadActTrack_ + 1, progress, totalProgress);

    audioReadLastLba_ = startLba;
  }


  if (chans == NULL) {
    // drive does not provide sub channel data so that's all we could do here:

    if (startLba > audioReadTrackInfo_[audioReadActTrack_ + 1].start) {
      audioReadActTrack_++;
      log_message(1, "Track %d...", audioReadActTrack_ + 1);
    }

    if (startLba - audioReadProgress_ > 75) {
      audioReadProgress_ = startLba;
      Msf m(audioReadProgress_);
      log_message(1, "%02d:%02d:00\r", m.min(), m.sec());
    }
    
    return len;      
  }

  // analyze sub-channels to find pre-gaps, index marks and ISRC codes
  for (i = 0; i < len; i++) {
    SubChannel *chan = chans[i];
    //chan->print();
    
    if (chan->checkCrc() && chan->checkConsistency()) {
      if (chan->type() == SubChannel::QMODE1DATA) {
	int t = chan->trackNr() - 1;
	Msf atime = Msf(chan->amin(), chan->asec(), chan->aframe());

	//log_message(0, "LastLba: %ld, ActLba: %ld", audioReadActLba_, atime.lba());

	if (t >= audioReadStartTrack_ && t <= audioReadEndTrack_ &&
	    atime.lba() > audioReadActLba_ && 
	    atime.lba() - 150 < audioReadTrackInfo_[t + 1].start) {
	  Msf time(chan->min(), chan->sec(), chan->frame()); // track rel time

	  audioReadActLba_ = atime.lba();

	  if (audioReadActLba_ - audioReadProgress_ > 75) {
	    audioReadProgress_ = audioReadActLba_;
	    Msf m(audioReadProgress_ - 150);
	    log_message(1, "%02d:%02d:00\r", m.min(), m.sec());
	  }

	  if (t == audioReadActTrack_ &&
	      chan->indexNr() == audioReadActIndex_ + 1) {
	  
	    if (chan->indexNr() > 1) {
	      log_message(2, "Found index %d at: %s", chan->indexNr(),
		      time.str());
	  
	      if (audioReadTrackInfo_[t].indexCnt < 98) {
		audioReadTrackInfo_[t].index[audioReadTrackInfo_[t].indexCnt] = time.lba();
		audioReadTrackInfo_[t].indexCnt += 1;
	      }
	    }
	  }
	  else if (t == audioReadActTrack_ + 1) {
	    log_message(1, "Track %d...", t + 1);
	    //chan->print();
	    if (chan->indexNr() == 0) {
              // don't use time.lba() to calculate pre-gap length; it would
              // count one frame too many if the CD counts the pre-gap down
              // to 00:00:00 instead of 00:00:01
              // Instead, count number of frames until start of Index 01
              // See http://sourceforge.net/tracker/?func=detail&aid=604751&group_id=2171&atid=102171
              // atime starts at 02:00, so subtract it
	      audioReadTrackInfo_[t].pregap = (startLba + len + 1) - \
                (atime.lba() - 150);
	      log_message(2, "Found pre-gap: %s",
                Msf(audioReadTrackInfo_[t].pregap).str());
	    }
	  }

	  audioReadActIndex_ = chan->indexNr();
	  audioReadActTrack_ = t;
	}
      }
      else if (chan->type() == SubChannel::QMODE3) {
	if (audioReadTrackInfo_[audioReadActTrack_].isrcCode[0] == 0) {
	  log_message(2, "Found ISRC code.");
	  strcpy(audioReadTrackInfo_[audioReadActTrack_].isrcCode,
		 chan->isrc());
	}
      }
    }
    else {
      audioReadCrcCount_++;
    }
  }

  return len;
}

int CdrDriver::readAudioRangeStream(ReadDiskInfo *info, int fd, long start,
				    long end, int startTrack, int endTrack, 
				    TrackInfo *trackInfo)
{
  long startLba = start;
  long endLba = end - 1;
  long len, ret;
  long blocking, blockLen;
  long lba = startLba;
  unsigned char *buf;

  blockLen = AUDIO_BLOCK_LEN + TrackData::subChannelSize(subChanReadMode_);
  blocking = scsiMaxDataLen_ / blockLen;
  assert(blocking > 0);
  
  buf = new unsigned char[blocking * blockLen];

  audioReadInfo_ = info;
  audioReadTrackInfo_ = trackInfo;
  audioReadStartTrack_ = startTrack;
  audioReadEndTrack_ = endTrack;
  audioReadLastLba_ = audioReadActLba_ = startLba + 149;
  audioReadActTrack_ = startTrack;
  audioReadActIndex_ = 1;
  audioReadCrcCount_ = 0;
  audioReadError_ = 0;
  audioReadProgress_ = 0;

  len = endLba - startLba + 1;

  log_message(1, "Track %d...", startTrack + 1);

  trackInfo[endTrack].bytesWritten = 0;

  while (len > 0) {
    long n = len > blocking ? blocking : len;
    long bytesToWrite = n * blockLen;

    CdrDriver::audioRead(subChanReadMode_, 1/*big endian byte order*/,
			 (Sample *)buf, lba, n);

    lba += n;

    if ((ret = fullWrite(fd, buf, bytesToWrite)) != bytesToWrite) {
      if (ret < 0)
	log_message(-2, "Writing of data failed: %s", strerror(errno));
      else
	log_message(-2, "Writing of data failed: Disk full");

      delete[] buf;
      return 1;
    }

    trackInfo[endTrack].bytesWritten += bytesToWrite;

    len -= n;
  }


  if (audioReadCrcCount_ != 0)
    log_message(2, "Found %ld Q sub-channels with CRC errors.", audioReadCrcCount_);

  delete[] buf;
  return 0;
}


// read cdda paranoia related:

void CdrDriver::paranoiaMode(int mode)
{
  paranoiaMode_ = PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP;

  switch (mode) {
  case 0:
    paranoiaMode_ = PARANOIA_MODE_DISABLE;
    break;

  case 1:
    paranoiaMode_ |= PARANOIA_MODE_OVERLAP;
    paranoiaMode_ &= ~PARANOIA_MODE_VERIFY;
    break;

  case 2:
    paranoiaMode_ &= ~(PARANOIA_MODE_SCRATCH|PARANOIA_MODE_REPAIR);
    break;
  }
}


int CdrDriver::readAudioRangeParanoia(ReadDiskInfo *info, int fd, long start,
				      long end, int startTrack, int endTrack, 
				      TrackInfo *trackInfo)
{
  long startLba = start;
  long endLba = end - 1;
  long len, ret;
  size16 *buf;

  if (paranoia_ == NULL) {
    // first time -> allocate paranoia structure 
    paranoiaDrive_ = new cdrom_drive;
    paranoiaDrive_->cdr = this;
    paranoiaDrive_->nsectors = maxScannedSubChannels_;
    paranoia_ = paranoia_init(paranoiaDrive_);
  }

  paranoia_set_range(paranoia_, startLba, endLba);
  paranoia_modeset(paranoia_, paranoiaMode_);

  audioReadInfo_ = info;
  audioReadTrackInfo_ = trackInfo;
  audioReadStartTrack_ = startTrack;
  audioReadEndTrack_ = endTrack;
  audioReadLastLba_ = audioReadActLba_ = startLba + 149;
  audioReadActTrack_ = startTrack;
  audioReadActIndex_ = 1;
  audioReadCrcCount_ = 0;
  audioReadError_ = 0;
  audioReadProgress_ = 0;

  len = endLba - startLba + 1;

  log_message(1, "Track %d...", startTrack + 1);

  trackInfo[endTrack].bytesWritten = 0;

  while (len > 0) {
    buf = paranoia_read(paranoia_, &CdrDriver::paranoiaCallback);

    // The returned samples are always in host byte order. We want to
    // output in big endian byte order so swap if we are a little
    // endian host.
    if (hostByteOrder_ == 0)
      swapSamples((Sample*)buf, SAMPLES_PER_BLOCK);

    if ((ret = fullWrite(fd, buf, AUDIO_BLOCK_LEN)) != AUDIO_BLOCK_LEN) {
      if (ret < 0)
	log_message(-2, "Writing of data failed: %s", strerror(errno));
      else
	log_message(-2, "Writing of data failed: Disk full");

      return 1;
    }

    trackInfo[endTrack].bytesWritten += AUDIO_BLOCK_LEN;

    len--;
  }


  if (audioReadCrcCount_ != 0)
    log_message(2, "Found %ld Q sub-channels with CRC errors.", audioReadCrcCount_);

  return 0;
}


long cdda_read(cdrom_drive *d, void *buffer, long beginsector, long sectors)
{
  CdrDriver *cdr = (CdrDriver*)d->cdr;

  return cdr->audioRead(TrackData::SUBCHAN_NONE, cdr->hostByteOrder(),
			(Sample*)buffer, beginsector, sectors);
}

void CdrDriver::paranoiaCallback(long, int)
{
}
