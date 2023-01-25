dnl AM_GCONF_SOURCE_2
dnl Defines GCONF_SCHEMA_CONFIG_SOURCE which is where you should install schemas
dnl  (i.e. pass to gconftool-2
dnl Defines GCONF_SCHEMA_FILE_DIR which is a filesystem directory where
dnl  you should install foo.schemas files
dnl

AC_DEFUN([AM_GCONF_SOURCE_2],
[
  if test "x$GCONF_SCHEMA_INSTALL_SOURCE" = "x"; then
    GCONF_SCHEMA_CONFIG_SOURCE=`gconftool-2 --get-default-source`
  else
    GCONF_SCHEMA_CONFIG_SOURCE=$GCONF_SCHEMA_INSTALL_SOURCE
  fi

  AC_ARG_WITH([gconf-source],
	      AS_HELP_STRING([--with-gconf-source=sourceaddress],[Config database for installing schema files.]),
	      [GCONF_SCHEMA_CONFIG_SOURCE="$withval"],)

  AC_SUBST(GCONF_SCHEMA_CONFIG_SOURCE)
  AC_MSG_RESULT([Using config source $GCONF_SCHEMA_CONFIG_SOURCE for schema installation])

  if test "x$GCONF_SCHEMA_FILE_DIR" = "x"; then
    GCONF_SCHEMA_FILE_DIR='$(sysconfdir)/gconf/schemas'
  fi

  AC_ARG_WITH([gconf-schema-file-dir],
	      AS_HELP_STRING([--with-gconf-schema-file-dir=dir],[Directory for installing schema files.]),
	      [GCONF_SCHEMA_FILE_DIR="$withval"],)

  AC_SUBST(GCONF_SCHEMA_FILE_DIR)
  AC_MSG_RESULT([Using $GCONF_SCHEMA_FILE_DIR as install directory for schema files])

  AC_ARG_ENABLE(schemas-install,
  	AS_HELP_STRING([--disable-schemas-install],[Disable the schemas installation]),
     [case ${enableval} in
       yes|no) ;;
       *) AC_MSG_ERROR([bad value ${enableval} for --enable-schemas-install]) ;;
      esac])
  AM_CONDITIONAL([GCONF_SCHEMAS_INSTALL], [test "$enable_schemas_install" != no])
])
