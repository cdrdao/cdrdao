#! /bin/sh
# Run this to generate the configure script and unpack needed packages

# This generates the configure script from configure.in
autoconf

# This deletes the (old) pccts dir and unpacks the latest version
rm -rf pccts
tar xvzf pccts.tar.gz

# This deletes the (old) scsilib dir and unpacks the latest version
rm -rf scsilib
tar xvzf scsilib.tar.gz
