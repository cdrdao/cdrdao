#! /bin/sh
# Run this to generate the configure script and unpack needed packages

# This generates the configure script from configure.in
autoconf

# This deletes the (old) pccts dir and unpacks the latest version
if test -e pccts.tar.gz ; then
  rm -rf pccts
  tar xvzf pccts.tar.gz
fi

# This deletes the (old) scsilib dir and unpacks the latest version
if test -e scsilib.tar.gz ; then
  rm -rf scsilib
  tar xvzf scsilib.tar.gz
fi
