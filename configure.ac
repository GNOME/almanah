AC_INIT(diary, 0.3)

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

if test $debug = "true"; then
	AC_DEFINE([ENABLE_DEBUG],[1],[Define if you want debugging enabled])
fi

AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AC_ARG_ENABLE([encryption],
	[  --enable-encryption	Enable database encryption support],
	[case "${enableval}" in
		yes) encryption=true ;;
		no)  encryption=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-encryption]) ;;
	esac],[encryption=true])

if test $encryption = "true"; then
	AC_DEFINE([ENABLE_ENCRYPTION],[1],[Define if you want database encryption support])
fi

AM_CONDITIONAL([ENCRYPTION], [test x$encryption = xtrue])

dnl ***************************************************************************
dnl Internationalisation
dnl ***************************************************************************

GETTEXT_PACKAGE=diary
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [GETTEXT package name])
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.35.0])

AM_PROG_LIBTOOL

PKG_CHECK_MODULES(STANDARD, glib-2.0 gtk+-2.0 >= 2.12 gmodule-2.0 gio-2.0 sqlite3 gtkspell-2.0 cairo)
AC_SUBST(STANDARD_CFLAGS)
AC_SUBST(STANDARD_LIBS)

if test $encryption = "true"; then
	PKG_CHECK_MODULES(ENCRYPTION, gconf-2.0)

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
data/diary.desktop
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/22x22/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
])
