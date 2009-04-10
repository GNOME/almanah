AC_INIT(almanah, 0.6.0)

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
	AC_HELP_STRING([--enable-debug], [turn on debugging]),
	[case "${enableval}" in
		yes) debug=true ;;
		no)  debug=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
	esac],[debug=false])

if test $debug = "true"; then
	AC_DEFINE([ENABLE_DEBUG],[1],[Define if you want debugging enabled])
fi

AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AC_ARG_ENABLE([encryption],
	AC_HELP_STRING([--enable-encryption], [enable database encryption support]),
	[case "${enableval}" in
		yes) encryption=true ;;
		no)  encryption=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-encryption]) ;;
	esac],[encryption=true])

if test $encryption = "true"; then
	AC_DEFINE([ENABLE_ENCRYPTION],[1],[Define if you want database encryption support])
fi

AM_CONDITIONAL([ENCRYPTION], [test x$encryption = xtrue])

AC_ARG_ENABLE([spell-checking],
	AC_HELP_STRING([--enable-spell-checking], [enable spell checking support]),
	[case "${enableval}" in
		yes) spell_checking=true ;;
		no)  spell_checking=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-spell-checking]) ;;
	esac],[spell_checking=true])

if test $spell_checking = "true"; then
	AC_DEFINE([ENABLE_SPELL_CHECKING],[1],[Define if you want spell checking support])
fi

AM_CONDITIONAL([SPELL_CHECKING], [test x$spell_checking = xtrue])

dnl ***************************************************************************
dnl Dependencies
dnl ***************************************************************************

GETTEXT_PACKAGE=almanah
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AM_PROG_LIBTOOL
AM_GCONF_SOURCE_2
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

PKG_CHECK_MODULES(STANDARD, glib-2.0 gtk+-2.0 >= 2.14 gmodule-2.0 gio-2.0 sqlite3 cairo gconf-2.0 atk libecal-1.2 libedataserver-1.2 libedataserverui-1.2 dbus-glib-1)
AC_SUBST(STANDARD_CFLAGS)
AC_SUBST(STANDARD_LIBS)

if test $spell_checking = "true"; then
	PKG_CHECK_MODULES(SPELL_CHECKING, gtkspell-2.0)
	AC_SUBST(SPELL_CHECKING_CFLAGS)
	AC_SUBST(SPELL_CHECKING_LIBS)
fi

if test $encryption = "true"; then
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
		ENCRYPTION_CFLAGS="$ENCRYPTION_CFLAGS `$GPGME_CONFIG --cflags`"
		ENCRYPTION_LIBS="$ENCRYPTION_LIBS `$GPGME_CONFIG --libs`"
		AC_MSG_RESULT(yes)
	else
		AC_MSG_ERROR(GPGME $min_gpgme_version or later needed)
	fi
else
	ENCRYPTION_CFLAGS=""
	ENCRYPTION_LIBS=""
fi

AC_SUBST(ENCRYPTION_CFLAGS)
AC_SUBST(ENCRYPTION_LIBS)

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/22x22/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
])
