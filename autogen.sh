#! /bin/sh
# Run this to generate the configure script and unpack needed packages

# This deletes the (old) pccts dir and unpacks the latest version
if test -e pccts.tar.gz ; then
  rm -rf pccts
  echo "Unpacking pccts.tar.gz"
  tar xzf pccts.tar.gz
fi

# This deletes the (old) scsilib dir and unpacks the latest version
if test -e scsilib.tar.gz ; then
  rm -rf scsilib
  echo "Unpacking scsilib.tar.gz"
  tar xzf scsilib.tar.gz
fi

# Calls aclocal, automake, autoconf and al. for you
echo "Running autoreconf"
autoreconf
