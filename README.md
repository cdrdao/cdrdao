# Cdrdao - Write audio/data CD-Rs in disk-at-once mode


This file contains some additional information. See the manual page for usage of the tool.

Please use the write simulation mode (command `simulate`) when trying this program the first time on your system. This will detect problems or incompatibilities without wasting a CD-recordable. Note that you may have to eject and reinsert the CD-R after a simulation run before a real write can start.

## Sources of Information

Cdrdao Homepage: http://cdrdao.sourceforge.net/

Download:        https://github.com/cdrdao/cdrdao/

Mailing Lists:

  - cdrdao-info@lists.sourceforge.net
    Moderated list for informing about new releases or serious bugs. For subscribing information send an email with 'help' in the body to <cdrdao-info-request@lists.sourceforge.net> or visit the info page http://lists.sourceforge.net/mailman/listinfo/cdrdao-info.
  - cdrdao-devel@lists.sourceforge.net
    Open lists for discussion about problems, implementation of new features etc.  For subscribing information send an email with 'help' in the body to <cdrdao-devel-request@lists.sourceforge.net> or visit the info page http://lists.sourceforge.net/mailman/listinfo/cdrdao-devel.

Please report bugs and suggestions to https://github.com/cdrdao/cdrdao/

## Supported Devices
###  SCSI Devices
Cdrdao should support all MMC-compliant devices using the default `generic-mmc` driver. 

Cdrdao no longer uses Joerg Schilling's SCSI library and now relies exclusively on native interfaces. The scanbus command returns a list of available devices (available for Mac, Windows and Linux). If only a single device is available, it is not necessary to use the `--device` option as cdrdao will selects it automatically.

On Unix-based systems, the device normally is the path to the device file, such as `/dev/cdrom` or `/dev/sr0`. On Windows systems, the device should be the drive letter (`--device D`).

### ATAPI Support
ATAPI drives use the same command set as SCSI devices so that cdrdao also supports all ATAPI CD-ROM, CD-R and CD-RW drives. Either the `generic-mmc` or the `generic-mmc-raw` driver should work if the recorder supports DAO writing at all.

Cdrdao access ATAPI devices through the same interface as used for SCSI devices. Therefore the operating system must emulate the ATAPI devices as SCSI devices. Some operating systems do this by default (e.g. Win32), other operating system must be told to do that (e.g. Linux, enable the IDE host adapter emulation) and some operating system may not support that.

The same applies for USB and parallel port devices: cdrdao supports them but the operating system must present them as SCSI devices.

### Older non-standard Devices
Cdrdao supports a number of older non-MMC devices. See the file `README.old` for more information.

## Features
### Digital Audio Extraction
The digital audio extraction is done with the help of Monty's paranoia library (except for Plextor devices, see below).

The output file will always contain raw signed 16 bit audio samples with MSB-LSB (big endian) byte order. If the byte order of the output file is not MSB-LSB you will have to use the driver option '0x20000', e.g. `--driver 0x20000`. Please do not try to use `--swap` for writing in this case because the byte order of the audio samples that are fed to the paranoia library is wrong, too, which will cause malfunction of the paranoia library routines.

Even if you specify a file name with a `.wav` extension the resulting file will be a raw audio file without any header.

`read-cd` and `copy` will read the first track's pre-gap, too. If your drive cannot access these audio sectors which is usually indicated by read errors you will have to use driver option 0x40000.

### CD-TEXT Writing
CD-TEXT data is read with `read-toc` or `read-cd` if your drive supports it (all recent devices) and will be stored in the TOC file. It is also possible to create the CD-TEXT data manually by editing the TOC file or by using the gcdmaster GUI. If you want to create your own CD-TEXT data be sure to add the fields `TITLE` and `PERFORMER` to all tracks.

### Text Encoding

The Audio CD specifications (the "Red Book") only provides support for three possible text encoding: ASCII, ISO-8859-1 (Latin) and MSJIS (Japanese). As most software and editors nowadays only support the ubiquitous UTF-8 encoding, if the CD-TEXT data contains non-ASCII characters (either ISO-8851 or MSJIS), cdrdao will translate them to UTF-8 and output UTF-8-encoded text in the TOC file for convenience. Cdrdao will convert the UTF-8 text back to the original encoding when writing an Audio CD. Use the `no-utf8` option to disable this feature. This option forces the TOC file to be pure ASCII (any non ASCII characters are displayed as their octal value preceded by a backslash).

Note that cdrdao only supports TOC files that are either pure ASCII (with non-ASCII characters shown as an escaped octal code) or UTF-8. Cdrdao will reject TOC files that contain non-ASCII characters that are not UTF-8 compliant.

## Examples
### CD Copying
The following command will copy the CD in the source drive specified with option `--source-device` to the CD-R/CD-RW inserted in the destination drive specified with option `--device`. Only a single session will be copied which can be selected with option `--session` (default: 1st session). If you want to keep the session open you will have to use option `--multi`.

```
cdrdao copy --source-device /dev/cdrom0 --device /dev/cdrom1 --buffers 64
```

The option `--buffers` is used to adjust the ring buffer size. Each buffer holds 1 second audio data. Dividing the specified number of buffers by the writing speed gives the approx. time for which data input my be stalled, e.g. 64 buffers and writing at 4x speed results in 16 seconds.

On the fly copying is selected with option `--on-the-fly`. No intermediate data will be stored on the disk in this case.

If the source CD contains audio tracks and the source drive is slow you should consider to reduce the audio extraction quality with option `--paranoia-mode 0` or reduce the writing speed to 2x. For `--paranoia-mode 0` you will need a perfect CD-ROM drive that can provide an accurate audio stream.

We strongly recommend to perform some simulation runs before trying real writing.

### Composing an audio CD
Assume three existing audio files `audio1.wav`, `audio2.wav` and `audio3.wav` that go in three tracks. We do not want a pause between track 1 and 2 (no pre-gap). The first 10 seconds of `audio1.wav` should be used as pre-gap for track 3. Here is the toc-file:
```
// Track 1
TRACK AUDIO
FILE "audio1.wav" 0  // take all audio data from 'audio1.wav',
                     // length is taken from file

// Track 2
TRACK AUDIO
FILE "audio2.wav" 0  // take all audio data from 'audio2.wav',
                     //length is taken from file, no pre-gap

// Track 3
TRACK AUDIO
FILE "audio1.wav" 0 0:10:0 // take first 10 seconds from 'audio1.wav'
START                      // everything before this line goes into the pre-gap
FILE "audio3.wav" 0  // take all audio data from 'audio3.wav', length is taken
                     // from file
```

Type `cdrdao show-toc example.toc` to check for the correct syntax of the toc-file. Note that even for the command `show-toc` the audio files must exist if the length of the audio files is not specified like in the example.

Type `cdrdao read-test example.toc` to check if all audio files can be read without error (optional).

Type `cdrdao simulate example.toc` to perform a write simulation (optional).

Type `cdrdao write example.toc` to create the audio CD.

## Drivers
By default, cdrdao will try to detect if your device is MMC compliant and use the `generic-mmc` driver is possible.

The following driver IDs may be used with option `--driver`. It is possible to specify option bits which modify the behavior of the driver. Global and driver specific options are available. Options flags can be combined by logical 'or' and appended to the `--driver` option, e.g.  `--driver plextor-scan:0x03`.

#### Global Option Bits:
 - `0x00010000`: Use the generic method to read the TOC of a CD for commands `read-toc` and `read-cd`. If this flag is selected multi session disks will not be handled properly but some drives do not work with the currently implemented method for reading the TOC of a specific session.
 - `0x00020000`: If the byte order of the audio data retrieved with `read-cd` is wrong (i.e. not big endian samples) use this option.
 - `0x00040000`: Use this option if your drive cannot read audio data from the first track's pre-gap. You will get read error messages in this case which should vanish if this option is used.
 - `0x00080000`: Suppresses the automatic data type detection for the raw TOC data and assumes that the drive sends BCD data. Use this or the following option if cdrdao reports that it cannot "determine if raw toc data is BCD or HEX".
 - `0x00100000`: Suppresses the automatic data type detection for the raw TOC data and assumes that the drive sends HEX data. Use this or the previous option if cdrdao reports that it cannot "determine if raw toc data is BCD or HEX".
 - `0x00200000`: Do not try to read CD-TEXT data with command `read-toc`, `read-cd` or 'copy'. Some drives lock up or send junk data when asked for the CD-TEXT data. 

### Available Drivers:
#### generic-mmc
This is a driver for SCSI-3/mmc compatible CD-recorders that support session-at-once (cue sheet based) writing. All recent drives (ATAPI, SCSI, USB, Parallel Port) should be compatible with this or with the `generic-mmc-raw` driver described below. Data track writing support is also available.  `read-toc` scans linearly the Q sub-channel of each track to retrieve the pre-gap length and index marks. This method is very accurate but takes the same time like digital audio extraction. It should also work with recent CD-ROM readers.

Option Bits (read-toc/read-cd related):
 - `0x00000001`: Read 16 bytes PQ sub-channel instead of 96 byte raw P-W sub-channel data. If `read-toc`/`read-cd` fails on your drive try to select this option.
 - `0x00000002`: Only used if option '0x00000001' is selected. If set the read PQ sub-channel data is expected to contain BCD instead of HEX values. If the time count that is displayed while running `read-toc` jumps you will have to toggle this option.
 - `0x00000004`: Do not take ISRC code from the sub-channel data but use the appropriate SCSI command for reading the ISRC code. This option is automatically selected if 16 byte PQ sub-channel data with HEX values is used.
 - `0x00000008`: Try to retrieve the media catalog number by scanning the sub-channels  instead of using the appropriate SCSI command. This might be an  option if the CD-R/CD-ROM drive does not extract the catalog number otherwise. Note: A media catalog number need not be present on on a CD.
 - `0x00000020`: Use this option if the drive cannot read sub-channel data along with audio data. A binary search method for pre-gap and index mark extraction will be selected in this case which has to play a sector before the sub-channel data can be retrieved.  If `read-toc`/`read-cd` works only with this option you should consider to use `--fast-toc` since the data retrieved with the binary search method is usually not very reliable and not worth the additional time.
 - `0x00000100`: Force using the raw R-W sub-channel reading mode for audio and data tracks.
Option Bits (writing related):
 - `0x00000010`: Enable CD-TEXT writing. This is no longer necessary and is now auto-detected. This option can be forced off by using `--driver generic-mmc:0x00`.
 - `0x00000040`: Suppresses the activation of the BURN Proof feature.
 - `0x00000080`: If the drive does not support packed R-W sub-channel writing (the drive does not support the L-EC data creation and interleaving for R-W sub-channel data) you will have to specify this option. Cdrdao will perform all the encoding and write in raw R-W mode. If the drive does not support the raw R-W writing mode, too, it is not possible to write sub-channel data.

#### generic-mmc-raw
This is an alternate driver for SCSI-3/mmc compatible CD-recorders. It uses the raw writing interface where the driver has to provide the PQ sub-channel data in addition to the audio data. This writing mode allows using part of the lead-out area for audio data since the drive's firmware has no chance to prevent this. Of course, you will get some error message at the end of writing when the CD-R capacity is exhausted.  Multi session recording is currently not supported.  CD structure analysis is done like in the `generic-mmc` driver.

Option Bits:
 - All of `generic-mmc` except:
   - `0x00000010`: CD-TEXT writing capability is automatically determined.
   - `0x00000080`: R-W sub-channel writing capability is automatically determined.

#### plextor
Supports CD structure analysis (command `read-toc`) for Plextor CD-ROM readers. Pre-gaps and index marks are detected by
performing a binary search over the Q sub-channel for each track. This method is fast and gives very exact results on Plextor drives. The driver uses generic SCSI commands (PLAY AUDIO and READ SUB-CHANNEL) so it may work on other models, too, but result and speed is directly correlated to the digital audio extraction capabilities.

Option Bits:
 - `0x00000001`: Force usage of the paranoia method for audio extraction instead of the special Plextor method that is used with Plextor drives.
 - `0x00000002`: Use the READ10 command to read audio data instead of the vendor specific READ CDDA command. Only used if the paranoia DAE method is selected.
 - `0x00000010`: Don't slow down after a read error. Available only on Plextor devices. Manufacturer default is to slow down.
 - `0x00000020`: Start data transfer before maximum speed is reached. Available only on Plextor PX-20 and later. Manufacturer default is to wait for maximum speed.
 - `0x00000040`: Don't slow down when drive pauses to avoid vibrations. Available only on Plextor PX-20 and later. Manufacturer default is to slow down.

#### plextor-scan

This is an alternate driver for CD structure analysis with Plextor drives. It scans the Q sub-channels of a track linearly like the`generic-mmc` driver but is faster on Plextor drives.

Option Bits:
 - `0x00000001`: See generic-mmc
 - `0x00000002`: See generic-mmc
 - `0x00000004`: See generic-mmc

#### cdd2600
This is a native driver for the Philips CDD2000/CDD2600 drive family. `read-toc` is implemented like in the 'plextor' driver but it is slow and not very exact due to poor digital audio extraction capabilities of these drives. Anyway, I don't recommend doing CD structure analysis with the CDD2x00 because itstresses the mechanism too much.

#### ricoh-mp6200
Supports writing with the Ricoh MP6200S CD recorder.CD structure analysis is done like in the `generic-mmc` driver.

#### sony-cdu920

Driver for the Sony CDU920 recorder. Audio and data tracks are supported.  The Sony CDU948 recorder will work with this driver, too. Use option `--speed 0` to get the full writing speed.  `read-toc` uses the Q sub-channel scanning method.

#### sony-cdu948
Driver for the Sony CDU948 recorder. It extends the 'sony-cdu920' driver by CD-TEXT writing and multi session mode.

#### taiyo-yuden
This is an adapted 'cdd2600' driver for Taiyo-Yuden recorders. `read-toc` is done with 'plextor' method.

#### teac-cdr55
Driver for the Teac CD-R55 recorder. Audio and data tracks are supported. `read-toc` uses the Q sub-channel scanning method.

#### toshiba
Read only driver for Toshiba SCSI CD-ROM drives. The Q sub-channel scanning method is used to detect pre-gaps and index marks.

#### yamaha-cdr10x
Driver for the Yamaha CDR10X recorder series that supports audio and data tracks. `read-toc` uses the Q sub-channel scanning method.
