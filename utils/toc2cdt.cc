/*  toc2cdt - writes the CD-TEXT data of a cdrdao toc-file as a raw
 *            CD-TEXT pack file (.cdt)
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

// The output file is a plain array of 18 byte CD-TEXT packs (pack type,
// track number, sequence number, block/character position, 12 data bytes,
// 2 CRC bytes), exactly as written into the lead-in. This is the format
// expected by the CUE sheet CDTEXTFILE statement and used for the CD-TEXT
// file inside a DDP image. With -H the 4 byte header returned by the MMC
// READ TOC/PMA/ATIP command (format 5) is prepended, as some tools expect.

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <string>
#include <vector>

#include "CdTextEncoder.h"
#include "Toc.h"
#include "log.h"
#include "util.h"

static const char *PRGNAME = "toc2cdt";

// Maximum number of packs per language block: the pack sequence number is
// 8 bits wide.
static const int MAX_PACKS_PER_BLOCK = 256;

static void printVersion()
{
    log_message(0, "%s version %s", PRGNAME, VERSION);
}

static void printUsage()
{
    log_message(0, "Usage: %s [-v #] [-H] input-toc-file [output-cdt-file]", PRGNAME);
    log_message(0, "       %s { -V | -h }", PRGNAME);
    log_message(0, "");
    log_message(0, "Writes the CD-TEXT data of a toc-file as raw CD-TEXT packs.");
    log_message(0, "If no output file is given, the toc-file name with a .cdt");
    log_message(0, "extension is used.");
    log_message(0, "");
    log_message(0, "  -v #  verbosity level (0..9, default 1)");
    log_message(0, "  -H    prepend the 4 byte MMC READ TOC header to the packs");
    log_message(0, "  -V    print version and exit");
    log_message(0, "  -h    print this help and exit");
}

static std::string defaultOutputName(const std::string &tocFile)
{
    std::string base = tocFile;
    size_t slash = base.find_last_of('/');
    size_t dot = base.find_last_of('.');

    if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        base.erase(dot);

    return base + ".cdt";
}

int main(int argc, char **argv)
{
    int verbose = 1;
    bool withHeader = false;
    int c;

    log_init();

    while ((c = getopt(argc, argv, "v:HVh")) != EOF) {
        switch (c) {
        case 'v':
            verbose = atoi(optarg);
            break;
        case 'H':
            withHeader = true;
            break;
        case 'V':
            printVersion();
            return 0;
        case 'h':
            printUsage();
            return 0;
        default:
            printUsage();
            return 1;
        }
    }

    log_set_verbose(verbose);

    if (optind >= argc || argc - optind > 2) {
        printUsage();
        return 1;
    }

    std::string tocFile = argv[optind];
    std::string cdtFile = (argc - optind == 2) ? argv[optind + 1] : defaultOutputName(tocFile);

    if (tocFile == cdtFile) {
        log_message(-2, "Output file must differ from the input toc-file.");
        return 1;
    }

    Toc *toc = Toc::read(tocFile);

    if (toc == NULL) {
        log_message(-2, "Failed to read toc-file '%s'.", tocFile.c_str());
        return 1;
    }

    CdTextEncoder encoder(toc);

    if (encoder.encode() != 0) {
        log_message(-2, "CD-TEXT encoding failed.");
        delete toc;
        return 1;
    }

    std::vector<CdTextPack> packs = encoder.getPacks();

    if (packs.empty()) {
        log_message(-2, "Toc-file '%s' does not contain any CD-TEXT data.", tocFile.c_str());
        delete toc;
        return 1;
    }

    // Check that the sequence numbers did not wrap around in any block.
    int blockPacks[8] = {0};

    for (const CdTextPack &p : packs)
        blockPacks[(p.blockCharacter >> 4) & 0x07]++;

    for (int b = 0; b < 8; b++) {
        if (blockPacks[b] > MAX_PACKS_PER_BLOCK) {
            log_message(-2,
                        "CD-TEXT block %d needs %d packs, but at most %d are possible. "
                        "Shorten the CD-TEXT data.",
                        b, blockPacks[b], MAX_PACKS_PER_BLOCK);
            delete toc;
            return 1;
        }
    }

    FILE *fp = fopen(cdtFile.c_str(), "wb");

    if (fp == NULL) {
        log_message(-2, "Cannot open '%s' for writing: %s", cdtFile.c_str(), strerror(errno));
        delete toc;
        return 1;
    }

    bool ok = true;
    size_t dataLen = packs.size() * sizeof(CdTextPack);

    if (withHeader) {
        // Data length field counts the 2 reserved bytes plus the packs.
        unsigned long len = dataLen + 2;
        unsigned char header[4] = {(unsigned char)(len >> 8), (unsigned char)len, 0, 0};

        if (len > 0xffff) {
            log_message(-2, "Too much CD-TEXT data for the MMC header length field.");
            ok = false;
        } else if (fwrite(header, 1, sizeof(header), fp) != sizeof(header)) {
            ok = false;
        }
    }

    if (ok && fwrite(packs.data(), 1, dataLen, fp) != dataLen)
        ok = false;

    if (fclose(fp) != 0)
        ok = false;

    if (!ok) {
        log_message(-2, "Writing '%s' failed.", cdtFile.c_str());
        unlink(cdtFile.c_str());
        delete toc;
        return 1;
    }

    log_message(1, "Wrote %zu CD-TEXT packs to '%s'.", packs.size(), cdtFile.c_str());

    delete toc;
    return 0;
}
