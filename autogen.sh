#! /bin/sh
# Run this to generate the configure script and unpack needed packages

cleanup() {
    echo Cleaning up.
    make clean > /dev/null >& /dev/null
    make distclean > /dev/null >& /dev/null
    find . -name Makefile.in -exec rm {} \;
    rm -f Makefile dao/Makefile gcdmaster/Makefile pccts/antlr/Makefile pccts/dlg/Makefile trackdb/Makefile utils/Makefile
    find . -name .deps -exec rm -fr {} \; >& /dev/null
    rm -f aclocal.m4 configure config.h config.log stamp-h1 specs/cdrdao.fedora.spec config.status gcdmaster/gcdmaster.schemas
    rm -fr autom4te.cache
}

if (( $# > 0 )) && [[ "$1" == "clean" ]]; then
    cleanup
    exit 0
fi

# Minimum version
MAJOR_VERSION=1
MINOR_VERSION=7

AMVERSION=`automake --version  | awk '/automake/ {print $4}'`

if test `echo $AMVERSION | awk -F. '{print $1}'` -lt $MAJOR_VERSION; 
then
  echo "Your version of automake is too old, you need at least automake $MAJOR_VERSION.$MINOR_VERSION"
  exit -1
fi

if test `echo $AMVERSION | awk -F. '{print $2}' | awk -F- '{print $1}'` -lt $MINOR_VERSION; then
  echo "Your version of automake is too old, you need at least automake $MAJOR_VERSION.$MINOR_VERSION"
  exit -1
fi

# Calls aclocal, automake, autoconf and al. for you
echo "Running autoreconf"
rm -fr autom4te.cache
autoreconf
