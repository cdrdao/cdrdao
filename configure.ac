dnl Process this file with autoconf to produce a configure script.
AC_REVISION($Revision: 1.25 $)dnl

AC_INIT(cdrdao, 1.2.3)

AC_CONFIG_SRCDIR(dao/main.cc)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_CANONICAL_HOST
AM_GCONF_SOURCE_2

AC_ARG_WITH(pcctsbin,[  --with-pcctsbin=dir     set directory of PCCTS parser generator executables],[pcctsbin=$withval],[pcctsbin=default])

AC_ARG_WITH(pcctsinc,[  --with-pcctsinc=dir     set directory of PCCTS parser generator includes],[pcctsinc=$withval],[pcctsinc=default])

AC_ARG_WITH(linux-qnx-sched,[  --with-linux-qnx-sched  enable QNX real time scheduling for Linux],[AC_DEFINE(LINUX_QNX_SCHEDULING,1,"Use real time scheduling for Linux")])

AC_ARG_WITH(posix-threads,[  --with-posix-threads    use Posix threads for ring buffer (default is YES)],[use_pthreads=$withval],[use_pthreads=default])

AC_ARG_WITH(scglib,[  --with-scglib           use bundled libscg SCSI library (see INSTALL) (default is NO)],[scglib=$withval],[scglib=default])

AC_ARG_WITH(xdao,[  --with-xdao             enable build of gcdmaster Gnome GUI front-end (default is YES)],[xdao=$withval],[xdao=default])

AC_ARG_WITH(ogg-support,[  --with-ogg-support      enable OGG format support (default is YES)],[],[with_ogg_support=yes])

AC_ARG_WITH(mp3-support,[  --with-mp3-support      enable MP3 format support (default is YES)],[],[with_mp3_support=yes])

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_PROG(AR,ar,ar)
AC_PROG_MAKE_SET
AC_PATH_TOOL([PKG_CONFIG], [pkg-config])

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h getopt.h malloc.h unistd.h sys/mman.h sched.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_TYPE_SIGNAL
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

case 2 in
	$ac_cv_sizeof_short) 
                AC_DEFINE(size16,short,"")
		SIZE16="short";;
	$ac_cv_sizeof_int)
                AC_DEFINE(size16,int,"")
		SIZE16="int";;
esac

case 4 in
	$ac_cv_sizeof_short) SIZE32="short";;
	$ac_cv_sizeof_int) SIZE32="int";;
	$ac_cv_sizeof_long) SIZE32="long";;
esac

AC_CHECK_TYPE(u_int8_t, unsigned char)
AC_CHECK_TYPE(int16_t,,AC_DEFINE_UNQUOTED(int16_t,$SIZE16,""))
AC_CHECK_TYPE(int32_t,,AC_DEFINE_UNQUOTED(int32_t,$SIZE32,""))
AC_CHECK_TYPE(u_int16_t,,AC_DEFINE_UNQUOTED(u_int16_t,$SIZE16,""))
AC_CHECK_TYPE(u_int32_t,,AC_DEFINE_UNQUOTED(u_int32_t,$SIZE32,""))

if test -z "$SIZE16"; then
	AC_MSG_ERROR(No 16 bit type found on this platform!)
fi

dnl Check for additionally required libraries

AC_CHECK_FUNC(sched_getparam,AC_DEFINE(HAVE_SCHED_GETPARAM,1,""),
  AC_CHECK_LIB(posix4,sched_getparam,[use_libposix4=yes;AC_DEFINE(HAVE_SCHED_GETPARAM)]))

AC_CHECK_FUNC(sched_get_priority_max,AC_DEFINE(HAVE_SCHED_GET_PRIORITY_MAX,1,""),
  AC_CHECK_LIB(posix4,sched_get_priority_max,[use_libposix4=yes;AC_DEFINE(HAVE_SCHED_GET_PRIORITY_MAX,1,"")]))

AC_CHECK_FUNC(sched_setscheduler,AC_DEFINE(HAVE_SCHED_SETSCHEDULER,1,""),
  AC_CHECK_LIB(posix4,sched_setscheduler,[use_libposix4=yes;AC_DEFINE(HAVE_SCHED_SETSCHEDULER,1,"")]))

AC_CHECK_LIB(socket,socket,[use_libsocket=yes])
AC_CHECK_LIB(socket,connect,[use_libsocket=yes])

dnl Add 'libsocket' if required
if test "$use_libsocket" = yes; then
  LIBS="$LIBS -lsocket"
fi

dnl Check if libposix4 must be linked
if test "$use_libposix4" = yes; then
  LIBS="$LIBS -lposix4"
fi

dnl Checks for library functions.
AC_CHECK_FUNCS(strerror)
AC_CHECK_FUNCS(mlockall munlockall)
AC_CHECK_FUNCS(getpagesize)
AC_CHECK_FUNCS(usleep)
AC_CHECK_FUNCS(setreuid setregid seteuid setegid setuid setgid)

AC_CHECK_FUNCS(inet_aton)

dnl check if Posix threads should be used
if test "$use_pthreads" = default; then
  use_pthreads=yes
fi

if test "$use_pthreads" = yes; then
  AC_CHECK_FUNC(pthread_create,have_pthread_std=yes,have_pthread_std=no)

  if test "$have_pthread_std" = yes; then
    AC_DEFINE(USE_POSIX_THREADS,1,"")
    thread_libs=""
    thread_test_lib=""
  else
    AC_CHECK_LIB(pthread,pthread_create,have_pthread=yes,have_pthread=no)

    if test "$have_pthread" = yes; then
      AC_DEFINE(USE_POSIX_THREADS,1,"")
      thread_libs="-lpthread"
      thread_test_lib="pthread"
    else
      AC_CHECK_LIB(pthread32,pthread_create,have_pthread32=yes,have_pthread32=no)
      if test "$have_pthread32" = yes; then
        AC_DEFINE(USE_POSIX_THREADS)
        thread_libs="-lpthread32"
        thread_test_lib="pthread32"
      else
        AC_CHECK_LIB(c_r,pthread_create,have_c_r=yes,have_c_r=no)
      
        if test "$have_c_r" = yes; then
          AC_DEFINE(USE_POSIX_THREADS)
          thread_libs="-lc_r"
          thread_test_lib="c_r"
        else
          use_pthreads=no
	fi
      fi
    fi
  fi

  if test "$use_pthreads" = yes; then
    if test "$have_pthread_std" = yes; then
      AC_CHECK_FUNCS(pthread_sigmask pthread_attr_setschedpolicy)
      AC_CHECK_FUNCS(pthread_attr_setschedparam pthread_getschedparam)
      AC_CHECK_FUNCS(pthread_setschedparam)
    else
      AC_CHECK_LIB($thread_test_lib,pthread_sigmask,AC_DEFINE(HAVE_PTHREAD_SIGMASK))
      AC_CHECK_LIB($thread_test_lib,pthread_attr_setschedpolicy,AC_DEFINE(HAVE_PTHREAD_ATTR_SETSCHEDPOLICY))
      AC_CHECK_LIB($thread_test_lib,pthread_attr_setschedparam,AC_DEFINE(HAVE_PTHREAD_ATTR_SETSCHEDPARAM))
      AC_CHECK_LIB($thread_test_lib,pthread_getschedparam,AC_DEFINE(HAVE_PTHREAD_GETSCHEDPARAM))
      AC_CHECK_LIB($thread_test_lib,pthread_setschedparam,AC_DEFINE(HAVE_PTHREAD_SETSCHEDPARAM))
    fi
  fi
fi  

if test "$use_pthreads" = yes; then
  mydefs="-D_THREAD_SAFE -D_REENTRANT $mydefs"
fi


dnl Checks for pccts parser generator
en_pccts="no"
if test "$pcctsbin" = default; then
  antlr_path='$(srcdir)/../pccts/antlr/antlr'
  dlg_path='$(srcdir)/../pccts/dlg/dlg'


  en_pccts="yes"
else
  AC_PATH_PROG(antlr_path,antlr,no,$pcctsbin)
  AC_PATH_PROG(dlg_path,dlg,no,$pcctsbin)

  if test $antlr_path = "no" || test $dlg_path = "no"; then
    AC_MSG_WARN([PCCTS: antlr or dlg not found in path, using own.])
    antlr_path='$(srcdir)/../pccts/antlr/antlr'
    dlg_path='$(srcdir)/../pccts/dlg/dlg'
    en_pccts="yes"
  fi
fi


if test "$pcctsinc" = default; then
  pcctsinc='$(srcdir)/../pccts/h'
  antlr_path='$(srcdir)/../pccts/antlr/antlr'
  dlg_path='$(srcdir)/../pccts/dlg/dlg'
  en_pccts="yes"
else
  ac_save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -I$pcctsinc"
  AC_CHECK_HEADER(pcctscfg.h,,[
    AC_MSG_WARN([PCCTS: headers not found in path, using own.])
    pcctsinc='$(srcdir)/../pccts/h'
    antlr_path='$(srcdir)/../pccts/antlr/antlr'
    en_pccts="yes"])
  CPPFLAGS="$ac_save_CPPFLAGS"
fi

utiltools="toc2cue cue2toc"
en_cdrdao=yes
en_toc2cue=yes
en_cue2toc=yes
en_toc2mp3=no

AC_PATH_LAME(3.92, [utiltools="$utiltools toc2mp3" en_toc2mp3=yes],
        [echo "Building of toc2mp3 disabled"])

dnl Determine whether we're building the gcdaster GUI front-end
en_xdao=no
if test "$xdao" = default; then
  xdao=yes
fi
if test "$xdao" = yes; then
  en_xdao=yes
  PKG_CHECK_MODULES(SIGCPP2, sigc++-2.0 >= 2.0.0,[],
        [echo "Building of gcdmaster disabled"; en_xdao=no])
  PKG_CHECK_MODULES(GTKMM2, gtkmm-2.4 >= 2.4.0,[],
        [echo "Building of gcdmaster disabled"; en_xdao=no])
  PKG_CHECK_MODULES(LIBGUIMM2, libgnomeuimm-2.6 >= 2.6.0,[],
        [echo "Building of gcdmaster disabled"; en_xdao=no])
fi

dnl Determine source for scsi interface class

if test "$scglib" = yes; then
  scsilib_dir=scsilib
  scsilib_incl='-I$(srcdir)/../scsilib/export -I$(srcdir)/../scsilib/export/libschily'
  scsilib_libs="-L../scsilib/export"
  en_scsilib="yes"

  scsilib_libs="$scsilib_libs -lscg -lschily"
  scsilib_objs="ScsiIf-lib.o"
  
  case "$host" in
    *-freebsd*)
			       scsilib_libs="$scsilib_libs -lcam"
			       ;;
  esac

else

  en_scsilib="no (using native interface)"
  scsilib_dir=""
  scsilib_incl=""
  scsilib_libs=""

  case "$host" in
    *-linux*)
	       scsilib_libs=""
	       scsilib_objs="ScsiIf-linux.o sg_err.o"
	       ;;
    
    *-freebsd*)
	       scsilib_libs="-lcam"
	       scsilib_objs="ScsiIf-freebsd-cam.o"
	       ;;

    *-netbsd*)
	       scsilib_libs="-lutil"
	       scsilib_objs="ScsiIf-netbsd.o"
	       ;;

    *-irix*)
	       scsilib_libs=""
	       scsilib_objs="ScsiIf-irix.o"
	       ;;

    *-darwin*)
	       scsilib_libs="-framework IOKit -framework CoreFoundation"
	       scsilib_objs="ScsiIf-osx.o"
	       ;;

    *-pc-cygwin*)
                scsilib_libs=""
                scsilib_objs="ScsiIf-win.o"
                ;;

    *)
       AC_MSG_ERROR([No native interface for operating system $host_os, use --with-libscg]) 
       ;;
    esac
fi

dnl Selection of buffer code
case "$host" in
  *)
     dao_src=dao.cc
     ;;
esac

dnl Additional operating system dependent selections
case "$host" in
  *-freebsd*)
	      if test "$use_pthreads" = yes; then
		LDFLAGS="-pthread $LDFLAGS"
dnl -lc_r is automatically done by the linker
		thread_libs=""
	      fi
	      ;;
  *darwin*)
            AC_DEFINE(_P1003_1B_VISIBLE,1,"")
            LIBS="$LIBS -framework CoreServices -framework IOKit"
            ;;

esac

dnl Sound device selection for xcdrdao
case "$host" in
  *-linux* | *-freebsd* | *-solaris*)
	     sound_if_obj=SoundIF-ao.o
	     ;;

  *)
     sound_if_obj=SoundIF-none.o
     ;;
esac

dnl Check for OGG support
if test "$with_ogg_support" = yes; then
  PKG_CHECK_MODULES(VORBISFILE, vorbisfile >= 1.0, [],
        [echo "OGG support disabled"; with_ogg_support=no])
fi

dnl Check for MP3 support
if test "$with_mp3_support" = yes; then
        PKG_CHECK_MODULES(MAD, mad >= 0.10, [],
	[echo "libMAD detection via pkg-config failed"; with_mp3_support=no])
	if test "$with_mp3_support" = no; then
		AC_CHECK_LIB(mad, mad_stream_init, [AC_CHECK_HEADER([mad.h])])
		if test "$ac_cv_lib_mad_mad_stream_init" = yes; then
        	        AC_ARG_VAR(MAD_LIBS,"")
	        	AC_ARG_VAR(MAD_CFLAGS,"")
		        MAD_LIBS="-lmad"
        		MAD_CFLAGS=""
        		with_mp3_support=yes
        	else
	                echo "MP3 support disabled"; with_mp3_support=no
		fi
	fi
fi

dnl Check for libao support
if test "$with_mp3_support" = yes || test "$with_ogg_support" = yes || test "$en_xdao" = yes; then
PKG_CHECK_MODULES(AO, ao >= 0.8, [AC_DEFINE(HAVE_AO,1,"")],
        [echo "Building of gcdmaster disabled"; with_ogg_support=no;
        with_mp3_support=no; en_xdao=no])
fi

if test "$with_ogg_support" = yes; then
  AC_DEFINE(HAVE_OGG_SUPPORT,1,"")
fi
if test "$with_mp3_support" = yes; then
  AC_DEFINE(HAVE_MP3_SUPPORT,1,"")
fi

dnl General platform specific setup

case "$host" in
  *-sysv4*)
		mydefs="-DUNIXWARE $mydefs"
		LIBS="$LIBS -L/usr/ucblib -lucb"
		;;
esac


AC_SUBST(en_xdao)
AC_SUBST(en_toc2mp3)
AC_SUBST(en_toc2cue)
AC_SUBST(en_cue2toc)
AC_SUBST(en_cdrdao)
AC_SUBST(scsilib_dir)
AC_SUBST(scsilib_incl)
AC_SUBST(scsilib_libs)
AC_SUBST(scsilib_objs)
AC_SUBST(pcctsinc)
AC_SUBST(dao_src)
AC_SUBST(sound_if_obj)
AC_SUBST(thread_libs)
AC_SUBST(mydefs)
AC_SUBST(antlr_path)
AC_SUBST(dlg_path)

AM_CONDITIONAL([COND_XDAO], [test "$en_xdao" = yes])
AM_CONDITIONAL([COND_TOC2MP3], [test "$en_toc2mp3" = yes])
AM_CONDITIONAL([COND_SCSILIB], [test "$en_scsilib" = yes])
AM_CONDITIONAL([COND_PCCTS], [test "$en_pccts" = yes])
AM_CONDITIONAL([COND_MP3], [test "$with_mp3_support" = yes])
AM_CONDITIONAL([COND_OGG], [test "$with_ogg_support" = yes])

AC_CONFIG_FILES([
        trackdb/Makefile
        dao/Makefile
        utils/Makefile
        xdao/Makefile
	xdao/gcdmaster.schemas
        xdao/stock/Makefile
        paranoia/Makefile
        pccts/Makefile
        pccts/antlr/Makefile
        pccts/dlg/Makefile
        Makefile
        specs/cdrdao.fedora.spec
        ])

AC_OUTPUT

echo
echo "------------------------------------------------------"
echo "  Building scsilib   : $en_scsilib"
echo "  Building pccts     : $en_pccts"
echo "  Building cdrdao    : $en_cdrdao"
echo "     OGG support     : $with_ogg_support"
echo "     MP3 support     : $with_mp3_support"
echo "  Building toc2cue   : $en_toc2cue"
echo "  Building cue2toc   : $en_cue2toc"
echo "  Building toc2mp3   : $en_toc2mp3"
echo "  Building gcdmaster : $en_xdao"
echo "------------------------------------------------------"
