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
**Optional deps for gcdmaster GUI:** gtkmm-4.0, libadwaitamm-1.0 (≥ 1.4.0), sigc++-3.0  
**Optional audio deps:** libao, libvorbis, libmad (MP3), flac++, libsamplerate, lame

## Tests

Tests live in `testtocs/`. They compare `show-toc -v 9` output against gold files in `testtocs/gold/`. Run the test suite via the Perl harness (there is no `make check` target wired up):

```bash
cd testtocs && ./test
```

The harness iterates every `*.toc` in `testtocs/` that has a matching `gold/<name>.showtoc` and prints PASS/FAIL per case. To test a single TOC file manually:

```bash
./dao/cdrdao show-toc -v 9 testtocs/t1.toc | diff - testtocs/gold/t1.showtoc
```

When intentionally changing `show-toc` output, regenerate the gold file with the same `-v 9` invocation rather than editing it by hand.

### Commonly Used Dev Commands

Non-destructive entry points — no CD-R required, useful when iterating on drivers or the TOC parser:

```bash
./dao/cdrdao scanbus                    # enumerate drives
./dao/cdrdao show-toc file.toc          # parse/validate TOC
./dao/cdrdao read-test file.toc         # verify referenced audio files are readable
./dao/cdrdao simulate file.toc          # dry-run write (laser off)
```

Stray files at the repo root (`cd16082.toc`, `cddata16082.bin`, `denizart.toc`, `km.toc`) are ad-hoc developer fixtures, not part of the canonical test suite in `testtocs/`. Don't rely on them for regression coverage.

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
Abstract base for all SCSI I/O. Static factory `ScsiIf::create(devicePath)` and `ScsiIf::scan()` return platform-specific instances. Implementations: `ScsiIf-linux.cc` (uses `/dev/sg*`/`/dev/sr*`), `ScsiIf-osx.cc` (IOKit), `ScsiIf-freebsd-cam.cc` (CAM), `ScsiIf-netbsd.cc`, `ScsiIf-nt.cc` (Windows/Cygwin). `ScsiIf` ownership is `std::shared_ptr` — a recent rewrite (commits `f22a7d2`, `db6a78b`, `de0431e`, `1d76f49`) replaced raw-pointer ownership and reworked the Linux backend. Don't revert to raw pointers or reintroduce manual `delete` on these instances.

**CD Device Drivers (`dao/CdrDriver.h`)**  
Abstract base class for all drive operations (`readToc()`, `write()`, `simulate()`, `blank()`, `readCd()`). `GenericMMC` / `GenericMMCraw` cover most modern drives. Vendor-specific subclasses exist for older hardware (Sony, Plextor, Yamaha, Teac, CDD2600, etc.). Driver selection is by string name (e.g. `generic-mmc`) resolved at runtime.

Driver options use a bitfield passed as `--driver name:0xHHHH`. Convention: high word (`0xFFFF0000`) holds **global** option bits shared across all drivers (byte-order, CD-TEXT suppression, pre-gap workarounds — see `README.md` for the list); low word (`0x0000FFFF`) holds **driver-specific** bits. When adding a new option to a driver, pick a bit in the low word and document it in `README.md`. Don't overload existing bits — the numbers look arbitrary but are stable ABI from the user's perspective.

**Track/TOC Database (`trackdb/`)**  
- `Toc` — in-memory CD table of contents; reads/writes `.toc` files; types: `CD_DA`, `CD_ROM`, `CD_ROM_XA`, `CD_I`  
- `Track` / `TrackData` — individual track and audio/data content, supporting file types `RAW`, `WAVE`, `MP3`, `OGG`, `FLAC` and data modes `AUDIO`, `MODE1`, `MODE2`, `MODE2_FORM1/2`  
- TOC file parser is PCCTS/ANTLR 1.33 generated (grammar: `trackdb/TocParser.g`); regenerate using the vendored tools under `pccts/`. **Never edit the generated files directly** — `trackdb/TocParser.cpp`, `TocParserGram.cpp`/`.h`, `TocParserTokens.h`, `TocLexerBase.cpp`/`.h`. Edit `TocParser.g` and regenerate, or your changes will be silently overwritten on the next build.  
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
