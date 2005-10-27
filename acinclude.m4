# Configure paths for GTK--
# Erik Andersen	30 May 1998
# Modified by Tero Pulkkinen (added the compiler checks... I hope they work..)

dnl Check and configure include and link paths for lame library
dnl AC_PATH_LAME(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl
AC_DEFUN([AC_PATH_LAME],
[dnl
  AC_ARG_ENABLE(lame-test,[  --disable-lame-test     skip checking for right mp3lame library],lame_test=$enableval,lame_test=yes)
  AC_ARG_WITH(lame,[  --with-lame             enable building of toc2mp3 (default is YES)],lame=$withval,lame="yes")
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

AC_DEFUN([AC_LIBSCG],
[dnl
dnl Get libscg version
dnl

dnl  AC_MSG_CHECKING(for scg/schily library >= $1)

  AC_LANG_SAVE
  AC_LANG_C
  ac_save_CFLAGS="$CFLAGS"
  ac_save_LIBS="$LIBS"

  AC_MSG_CHECKING(for libscg/schily version >= $1)
  CFLAGS="$scsilib_incl"
  LIBS="$scsilib_libs"

  AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <standard.h>
#include <scg/scgcmd.h>
#include <scg/scsitransp.h>
]],[[
  int maj1, maj2, min1, min2;
  const char* v1 = "$1";
  const char* v2 = scg_version(0, SCG_VERSION);
  maj1 = atoi(v1);
  maj2 = atoi(v2);
  if (maj2 < maj1) {
    return -1;
  }
  if (!strchr(v1, '.') || !strchr(v2, '.'))
    return -1;

  min1 = atoi(strchr(v1, '.') + 1);
  min2 = atoi(strchr(v2, '.') + 1);
  if (min2 < min1) {
    return -1;
  }
   
]])],[AC_MSG_RESULT(yes); $2],[AC_MSG_RESULT(no); $3],[AC_MSG_RESULT(skipped); $2])

  CFLAGS="$ac_save_CFLAGS"
  LIBS="$ac_save_LIBS"
  AC_LANG_RESTORE
]
)

