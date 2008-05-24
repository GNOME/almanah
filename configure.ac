AC_INIT(diary, 0.1)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

dnl ***************************************************************************
dnl Options
dnl ***************************************************************************

AC_ARG_ENABLE([debug],
	[  --enable-debug	Turn on debugging],
	[case "${enableval}" in
		yes) debug=true ;;
		no)  debug=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
	esac],[debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

dnl ***************************************************************************
dnl Internationalisation
dnl ***************************************************************************

GETTEXT_PACKAGE=diary
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AM_PROG_LIBTOOL

PKG_CHECK_MODULES(STANDARD, glib-2.0 gtk+-2.0 >= 2.12 gmodule-2.0 gio-2.0 sqlite3 gtkspell-2.0 gconf-2.0)
AC_SUBST(STANDARD_CFLAGS)
AC_SUBST(STANDARD_LIBS)

PKG_CHECK_MODULES(ENCRYPTION, cryptui-0.0)

dnl Check for GPGME, which doesn't provide a pkgconfig file
dnl This code courtesy of seahorse
min_gpgme_version=1.0.0
AC_PATH_PROG(GPGME_CONFIG, gpgme-config, "failed")
if test $GPGME_CONFIG != "failed" ; then
      AC_MSG_CHECKING(for GPGME)
      req_major=`echo $min_gpgme_version | \
		 sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\1/'`
      req_minor=`echo $min_gpgme_version | \
                 sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\2/'`
      req_micro=`echo $min_gpgme_version | \
                 sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\)/\3/'`
      gpgme_config_version=`$GPGME_CONFIG --version`
      major=`echo $gpgme_config_version | \
                 sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\1/'`
      minor=`echo $gpgme_config_version | \
                 sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\2/'`
      micro=`echo $gpgme_config_version | \
		 sed 's/\([[0-9]]*\)\.\([[0-9]]*\)\.\([[0-9]]*\).*/\3/'`

	if test "$major" -eq "$req_major"; then
	   if test "$minor" -ge "$req_minor"; then
	     if test "$micro" -ge "$req_micro"; then
	       ok="yes"
	     fi
	   fi
	fi
fi

if test $ok = "yes"; then
  GPGME_CFLAGS=`$GPGME_CONFIG --cflags`
  GPGME_LIBS=`$GPGME_CONFIG --libs`
  AC_MSG_RESULT(yes)
else
  AC_MSG_ERROR(GPGME $min_gpgme_version or later needed)
fi

ENCRYPTION_CFLAGS="$ENCRYPTION_CFLAGS $GPGME_CFLAGS"
ENCRYPTION_LIBS="$ENCRYPTION_LIBS $GPGME_LIBS"

AC_SUBST(ENCRYPTION_CFLAGS)
AC_SUBST(ENCRYPTION_LIBS)

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
data/Makefile
])
