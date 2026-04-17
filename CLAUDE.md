# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

cdrdao uses GNU Autotools (Autoconf/Automake).

```bash
# First time / after configure.ac changes:
./autogen.sh
./configure
make

# Subsequent builds:
make

# Install:
make install
```

Optional configure flags of note:
- `--without-gcdmaster` — skip GUI build even if GTK deps are present
- `--without-posix-threads` — disable pthreads

**Mandatory deps:** C++17 compiler, iconv  
**Optional deps for gcdmaster GUI:** gtkmm-4.0, sigc++-3.0  
**Optional audio deps:** libao, libvorbis, libmad (MP3), flac++, libsamplerate, lame

## Tests

Tests live in `testtocs/`. They compare `show-toc` output against gold files in `testtocs/gold/`. Run the test suite via:

```bash
make check
```

To test a single TOC file manually:
```bash
./dao/cdrdao show-toc testtocs/t1.toc
diff - testtocs/gold/t1.showtoc
```

## Architecture

The codebase layers as follows:

```
CLI (dao/cdrdao.cc) / GUI (gcdmaster/)
          ↓
   CdrDriver hierarchy (dao/)
          ↓
   ScsiIf abstraction (dao/ScsiIf.h)
          ↓
   OS-specific backends (dao/ScsiIf-linux.cc, -osx.cc, -freebsd-cam.cc, etc.)
```

### Key Subsystems

**SCSI Interface (`dao/ScsiIf.h`)**  
Abstract base for all SCSI I/O. Static factory `ScsiIf::create(devicePath)` and `ScsiIf::scan()` return platform-specific instances. Implementations: `ScsiIf-linux.cc` (uses `/dev/sg*`/`/dev/sr*`), `ScsiIf-osx.cc` (IOKit), `ScsiIf-freebsd-cam.cc` (CAM), `ScsiIf-netbsd.cc`, `ScsiIf-nt.cc` (Windows/Cygwin). The `shared_ptr` ownership model was recently adopted.

**CD Device Drivers (`dao/CdrDriver.h`)**  
Abstract base class for all drive operations (`readToc()`, `write()`, `simulate()`, `blank()`, `readCd()`). `GenericMMC` / `GenericMMCraw` cover most modern drives. Vendor-specific subclasses exist for older hardware (Sony, Plextor, Yamaha, Teac, CDD2600, etc.). Driver selection is by string name (e.g. `generic-mmc`) resolved at runtime.

**Track/TOC Database (`trackdb/`)**  
- `Toc` — in-memory CD table of contents; reads/writes `.toc` files; types: `CD_DA`, `CD_ROM`, `CD_ROM_XA`, `CD_I`  
- `Track` / `TrackData` — individual track and audio/data content, supporting file types `RAW`, `WAVE`, `MP3`, `OGG`, `FLAC` and data modes `AUDIO`, `MODE1`, `MODE2`, `MODE2_FORM1/2`  
- TOC file parser is ANTLR 1.33 generated (grammar: `trackdb/TocParser.g`); regenerate with `pccts/` tools  
- `Cue2Toc` / `Toc2Cue` handle CUE sheet interoperability

**Audio Extraction (`paranoia/`)**  
Embedded cdparanoia library for robust DAE (digital audio extraction) with error correction on damaged discs.

**GUI (`gcdmaster/`)**  
Optional GTK/Gtkmm 4.0 front-end. Key classes: `AudioCDProject` (project management), `TocEdit` (track editing), `CdDevice` (device wrapper with state machine: `DEV_READY`, `DEV_RECORDING`, `DEV_READING`, etc.).

**Utilities (`utils/`)**  
Standalone tools: `toc2cue`, `cue2toc`, `toc2mp3`, `toc2cddb`.

### Command Routing (CLI)

`dao/cdrdao.cc` is the main CLI entry point. Commands (`WRITE`, `READ_TOC`, `COPY_CD`, `SIMULATE`, `BLANK`, `SCAN_BUS`, `READ_CD`, etc.) are dispatched through a `cmdStruct` table that declares device and TOC requirements per command. Config files: `/etc/cdrdao.conf`, `~/.cdrdao`.

## Platform Notes

- **Linux:** SCSI generic module (`sg`) must be loaded; devices are `/dev/sr0`, `/dev/sg0`, etc.
- **macOS:** Requires IOKit framework linkage
- **FreeBSD/DragonFly:** Links `-lcam`
- **Windows:** Cygwin/MSYS2; device specified as drive letter (`--device D`)
