# Configure paths for GTK--
# Erik Andersen	30 May 1998
# Modified by Tero Pulkkinen (added the compiler checks... I hope they work..)

dnl Test for GTKMM, and define GTKMM_CFLAGS and GTKMM_LIBS
dnl   to be used as follows:
dnl AM_PATH_GTKMM([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl
AC_DEFUN(AC_PATH_GTKMM,
[dnl 
dnl Get the cflags and libraries from the gtkmm-config script
dnl
AC_ARG_WITH(gtkmm-prefix,[  --with-gtkmm-prefix=PREFIX
                          Prefix where GTK-- is installed (optional)],
            gtkmm_config_prefix="$withval", gtkmm_config_prefix="")
AC_ARG_WITH(gtkmm-exec-prefix,[  --with-gtkmm-exec-prefix=PREFIX
                          Exec prefix where GTK-- is installed (optional)],
            gtkmm_config_exec_prefix="$withval", gtkmm_config_exec_prefix="")
AC_ARG_ENABLE(gtkmmtest, [  --disable-gtkmmtest     Do not try to compile and run a test GTK-- program],
		    , enable_gtkmmtest=yes)

  if test x$gtkmm_config_exec_prefix != x ; then
     gtkmm_config_args="$gtkmm_config_args --exec-prefix=$gtkmm_config_exec_prefix"
     if test x${GTKMM_CONFIG+set} != xset ; then
        GTKMM_CONFIG=$gtkmm_config_exec_prefix/bin/gtkmm-config
     fi
  fi
  if test x$gtkmm_config_prefix != x ; then
     gtkmm_config_args="$gtkmm_config_args --prefix=$gtkmm_config_prefix"
     if test x${GTKMM_CONFIG+set} != xset ; then
        GTKMM_CONFIG=$gtkmm_config_prefix/bin/gtkmm-config
     fi
  fi

  AC_PATH_PROG(GTKMM_CONFIG, gtkmm-config, no)
  min_gtkmm_version=ifelse([$1], ,0.10.0,$1)

  AC_MSG_CHECKING(for GTK-- - version >= $min_gtkmm_version)
  no_gtkmm=""
  if test "$GTKMM_CONFIG" = "no" ; then
    no_gtkmm=yes
  else
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS

    GTKMM_CFLAGS=`$GTKMM_CONFIG --cflags`
    GTKMM_LIBS=`$GTKMM_CONFIG --libs`
    gtkmm_config_major_version=`$GTKMM_CONFIG --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gtkmm_config_minor_version=`$GTKMM_CONFIG --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gtkmm_config_micro_version=`$GTKMM_CONFIG --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gtkmmtest" = "xyes" ; then
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CXXFLAGS="$CXXFLAGS $GTKMM_CFLAGS"
      LIBS="$LIBS $GTKMM_LIBS"
dnl
dnl Now check if the installed GTK-- is sufficiently new. (Also sanity
dnl checks the results of gtkmm-config to some extent
dnl
      rm -f conf.gtkmmtest
      AC_TRY_RUN([
#include <gtk--.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gtkmmtest");

  /* HP/UX 0 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gtkmm_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gtkmm_version");
     exit(1);
   }

  if ((gtkmm_major_version != $gtkmm_config_major_version) ||
      (gtkmm_minor_version != $gtkmm_config_minor_version) ||
      (gtkmm_micro_version != $gtkmm_config_micro_version))
    {
      printf("\n*** 'gtkmm-config --version' returned %d.%d.%d, but GTK-- (%d.%d.%d)\n", 
             $gtkmm_config_major_version, $gtkmm_config_minor_version, $gtkmm_config_micro_version,
             gtkmm_major_version, gtkmm_minor_version, gtkmm_micro_version);
      printf ("*** was found! If gtkmm-config was correct, then it is best\n");
      printf ("*** to remove the old version of GTK--. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gtkmm-config was wrong, set the environment variable GTKMM_CONFIG\n");
      printf("*** to point to the correct copy of gtkmm-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
/* GTK-- does not have the GTKMM_*_VERSION constants */
/* 
  else if ((gtkmm_major_version != GTKMM_MAJOR_VERSION) ||
	   (gtkmm_minor_version != GTKMM_MINOR_VERSION) ||
           (gtkmm_micro_version != GTKMM_MICRO_VERSION))
    {
      printf("*** GTK-- header files (version %d.%d.%d) do not match\n",
	     GTKMM_MAJOR_VERSION, GTKMM_MINOR_VERSION, GTKMM_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     gtkmm_major_version, gtkmm_minor_version, gtkmm_micro_version);
    }
*/
  else
    {
      if ((gtkmm_major_version > major) ||
        ((gtkmm_major_version == major) && (gtkmm_minor_version > minor)) ||
        ((gtkmm_major_version == major) && (gtkmm_minor_version == minor) && (gtkmm_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GTK-- (%d.%d.%d) was found.\n",
               gtkmm_major_version, gtkmm_minor_version, gtkmm_micro_version);
        printf("*** You need a version of GTK-- newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GTK-- is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gtkmm-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GTK--, but you can also set the GTKMM_CONFIG environment to point to the\n");
        printf("*** correct copy of gtkmm-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gtkmm=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CXXFLAGS="$ac_save_CXXFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gtkmm" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GTKMM_CONFIG" = "no" ; then
       echo "*** The gtkmm-config script installed by GTK-- could not be found"
       echo "*** If GTK-- was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GTKMM_CONFIG environment variable to the"
       echo "*** full path to gtkmm-config."
       echo "*** The gtkmm-config script was not available in GTK-- versions"
       echo "*** prior to 0.9.12. Perhaps you need to update your installed"
       echo "*** version to 0.9.12 or later"
     else
       if test -f conf.gtkmmtest ; then
        :
       else
          echo "*** Could not run GTK-- test program, checking why..."
          CXXFLAGS="$CFLAGS $GTKMM_CXXFLAGS"
          LIBS="$LIBS $GTKMM_LIBS"
          AC_TRY_LINK([
#include <gtk--.h>
#include <stdio.h>
],      [ return ((gtkmm_major_version) || (gtkmm_minor_version) || (gtkmm_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GTK-- or finding the wrong"
          echo "*** version of GTK--. If it is not finding GTK--, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GTK-- was incorrectly installed"
          echo "*** or that you have moved GTK-- since it was installed. In the latter case, you"
          echo "*** may want to edit the gtkmm-config script: $GTKMM_CONFIG" ])
          CXXFLAGS="$ac_save_CXXFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GTKMM_CFLAGS=""
     GTKMM_LIBS=""
     ifelse([$3], , :, [$3])
     AC_LANG_RESTORE
  fi
  AC_SUBST(GTKMM_CFLAGS)
  AC_SUBST(GTKMM_LIBS)
  rm -f conf.gtkmmtest
])



# Configure paths for Gnome

dnl Test for Gnome, define GNOME_CFLAGS and GNOME_LIBS
dnl   to be used as follows:
dnl AC_PATH_GNOME([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl
AC_DEFUN(AC_PATH_GNOME,
[dnl 
dnl Get the cflags and libraries from the gnome-config script
dnl
AC_ARG_WITH(gnome-prefix,[  --with-gnome-prefix=PREFIX
                          Prefix where Gnome and Gnomemm is installed (optional)],
            gnome_config_prefix="$withval", gnome_config_prefix="")

  if test x$gnome_config_prefix != x ; then
     gnome_config_args="$gnome_config_args --prefix=$gnome_config_prefix"
     if test x${GNOME_CONFIG+set} != xset ; then
        GNOME_CONFIG=$gnome_config_prefix/bin/gnome-config
     fi
  fi

  AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
  min_gnome_version=$1;

  AC_MSG_CHECKING(for Gnome - version >= $min_gnome_version)
  no_gnome=""
  if test "$GNOME_CONFIG" = "no" ; then
    no_gnome=yes
  else
    gnome_config_major_version=`$GNOME_CONFIG --version | \
           sed 's/.* \([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gnome_config_minor_version=`$GNOME_CONFIG --version | \
           sed 's/.* \([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gnome_config_micro_version=`$GNOME_CONFIG --version | \
           sed 's/.* \([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`

    GNOME_CFLAGS=`$GNOME_CONFIG --cflags gnomeui`
    GNOME_LIBS=`$GNOME_CONFIG --libs gnomeui`
  fi
  if test "x$no_gnome" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GNOME_CONFIG" = "no" ; then
       echo "*** The gnome-config script installed by Gnome could not be found"
       echo "*** If Gnome was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GNOME_CONFIG environment variable to the"
       echo "*** full path to gnome-config."
     fi
     GNOME_CFLAGS=""
     GNOME_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GNOME_CFLAGS)
  AC_SUBST(GNOME_LIBS)
])


dnl Check and configure include and link paths for a Gnome module 
dnl AC_PATH_GNOME_MODULE(MODUE-NAME, MINIMUM-VERSION, CFLAGS-VAR, LIBS-VAR, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
AC_DEFUN(AC_PATH_GNOME_MODULE,
[dnl 
dnl Get the cflags and libraries for the module from the gnome-config script
dnl
AC_ARG_WITH(gnome-prefix,[  --with-gnome-prefix=PREFIX
                          Prefix where Gnome is installed (optional)],
            gnome_config_prefix="$withval", gnome_config_prefix="")

  min_gnome_module_major_version=`echo $2 | sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`;
  min_gnome_module_minor_version=`echo $2 | sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`;
  min_gnome_module_micro_version=`echo $2 | sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`;

  if test x$gnome_config_prefix != x ; then
     gnome_config_args="$gnome_config_args --prefix=$gnome_config_prefix"
     if test x${GNOME_CONFIG+set} != xset ; then
        GNOME_CONFIG=$gnome_config_prefix/bin/gnome-config
     fi
  fi

  AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)

  AC_MSG_CHECKING(for Gnome module $1 - version >= $2)

  no_gnome=""
  no_gnome_module=""

  if test "$GNOME_CONFIG" = "no" ; then
    no_gnome=yes
  elif $GNOME_CONFIG --libs $1 > /dev/null 2>&1 ; then
    gnome_module_major_version=`$GNOME_CONFIG --modversion $1 | \
           sed "s/$1-\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/"`
    gnome_module_minor_version=`$GNOME_CONFIG --modversion $1 | \
           sed "s/$1-\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/"`
    gnome_module_micro_version=`$GNOME_CONFIG --modversion $1 | \
           sed "s/$1-\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/"`
  else
     no_gnome_module=yes   
  fi

  $3=""
  $4=""

  if test "x$no_gnome_module" = xyes ; then
    AC_MSG_RESULT("no - module $1 not found")
    ifelse([$6], , :, [$6])
  elif test "x$no_gnome" = x ; then
     gnome_module_version_ok=no

     if test $gnome_module_major_version -gt $min_gnome_module_major_version ; then
       gnome_module_version_ok=yes
     elif test $gnome_module_major_version -eq $min_gnome_module_major_version ; then
       if test $gnome_module_minor_version -gt $min_gnome_module_minor_version ; then
         gnome_module_version_ok=yes
       elif test $gnome_module_minor_version -eq $min_gnome_module_minor_version ; then
         if test $gnome_module_micro_version -ge $min_gnome_module_micro_version ; then
           gnome_module_version_ok=yes
         fi
       fi
     fi

     if test $gnome_module_version_ok = yes ; then
       AC_MSG_RESULT(yes)

       $3=`$GNOME_CONFIG --cflags $1`
       $4=`$GNOME_CONFIG --libs $1`

       ifelse([$5], , :, [$5])
     else
       AC_MSG_RESULT("no - found version $gnome_module_major_version.$gnome_module_minor_version.$gnome_module_micro_version")
       ifelse([$6], , :, [$6])
     fi
  else
     AC_MSG_RESULT(no)
     if test "$GNOME_CONFIG" = "no" ; then
       echo "*** The gnome-config script installed by Gnome could not be found"
       echo "*** If Gnome was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GNOME_CONFIG environment variable to the"
       echo "*** full path to gnome-config."
     fi
     ifelse([$6], , :, [$6])
  fi

  AC_SUBST($3)
  AC_SUBST($4)
])


dnl Check and configure include and link paths for lame library
dnl AC_PATH_LAME(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
AC_DEFUN(AC_PATH_LAME,
[dnl
  AC_ARG_ENABLE(lame-test,[  --disable-lame-test     skip checking for right mp3lame library],lame_test=$enableval,lame_test=yes)
  AC_ARG_WITH(lame,[  --without-lame          disable building of toc2mp3],lame=$withval,lame="yes")
  AC_ARG_WITH(lame-lib,[  --with-lame-lib=dir     set directory containing libmp3lame],lamelib=$withval,lamelib="")
  AC_ARG_WITH(lame-include,[  --with-lame-include=dir set directory containing lame include files],lameinc=$withval,lameinc="")

  if test x$lame = xyes ; then
    AC_MSG_CHECKING(for Lame library version >= $1)
    lame_ok=yes
  else
    lame_ok=no
    lame_test=no
  fi

  if test x$lame_test = xyes ; then
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS

    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"

    if test "x$lameinc" != x ; then
      CXXFLAGS="$CXXFLAGS -I$lameinc"
    fi

    if test "x$lamelib" != x ; then
      LIBS="$LIBS -L$lamelib"
    fi

    LIBS="$LIBS -lmp3lame"

    AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lame/lame.h>
int 
main ()
{
  int major, minor;
  char *tmp_version;
  lame_version_t lame_version;

  /* HP/UX 0 (%@#!) writes to sscanf strings */
  tmp_version = strdup("$1");
  if (sscanf(tmp_version, "%d.%d", &major, &minor) != 2) {
     printf("%s, bad version string\n", "$1");
     exit(1);
  }

  get_lame_version_numerical(&lame_version);

  if (lame_version.major > major ||
      ((lame_version.major == major) && (lame_version.minor >= minor))) {
    return 0;
  }
  else {
    printf("\n*** An old version of LAME (%d.%d) was found.\n",
           lame_version.major, lame_version.minor);
    printf("*** You need a version of LAME newer than %d.%d. The latest version of\n",
	       major, minor);
    printf("*** LAME is available from http://www.mp3dev.org/.\n");
    printf("*** However, it is very likely that slightly older versions of LAME\n");
    printf("*** will also work. If you want to try it, run configure with option\n");
    printf("*** --disable-lame-test. This will skip this check and assume that\n");
    printf("*** LAME is available for compiling.\n");
  }

  return 1;
}
],,lame_ok=no,[echo $ac_n "cross compiling; assumed OK... $ac_c"])

    CXXFLAGS="$ac_save_CXXFLAGS"
    LIBS="$ac_save_LIBS"

    AC_LANG_RESTORE
  fi

  LAME_CFLAGS=""
  LAME_LIBS=""

  if test $lame_ok = yes ; then
    AC_MSG_RESULT(yes)

    if test "x$lameinc" != x ; then
      LAME_CFLAGS="-I$lameinc"
    fi

    if test "x$lamelib" != x ; then
      LAME_LIBS="-L$lamelib"
    fi

    LAME_LIBS="$LAME_LIBS -lmp3lame"

    ifelse([$2], , :, [$2])
  else
    if test x$lame = xyes ; then
      AC_MSG_RESULT(no)
    fi

    ifelse([$3], , :, [$3])
  fi

  AC_SUBST(LAME_CFLAGS)
  AC_SUBST(LAME_LIBS)
]
)
