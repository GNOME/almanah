AC_PREREQ(2.63)
AC_INIT([almanah],[0.9.0],[https://bugzilla.gnome.org/enter_bug.cgi?product=almanah],[almanah],[http://live.gnome.org/Almanah_Diary])

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.9 dist-bzip2 no-dist-gzip check-news subdir-objects])

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

GETTEXT_PACKAGE=almanah
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define to the Gettext package name])
AC_SUBST(GETTEXT_PACKAGE)
IT_PROG_INTLTOOL([0.35.0])

AC_PROG_CXX
AM_PROG_CC_C_O
LT_INIT([])
PKG_PROG_PKG_CONFIG
GLIB_GSETTINGS

AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

dnl ***************************************************************************
dnl Options
dnl ***************************************************************************

dnl Debugging
AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug],[turn on debugging]),
	[case "${enableval}" in
		yes) debug=true ;;
		no)  debug=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
	esac],[debug=false])

if test $debug = "true"; then
	AC_DEFINE([ENABLE_DEBUG],[1],[Define if you want debugging enabled])
fi

AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

dnl Encryption support
AC_ARG_ENABLE([encryption],
	AS_HELP_STRING([--enable-encryption],[enable database encryption support]),
	[case "${enableval}" in
		yes) encryption=true ;;
		no)  encryption=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-encryption]) ;;
	esac],[encryption=true])

if test $encryption = "true"; then
	AC_DEFINE([ENABLE_ENCRYPTION],[1],[Define if you want database encryption support])
fi

AM_CONDITIONAL([ENCRYPTION], [test x$encryption = xtrue])

dnl Spell checking
AC_ARG_ENABLE([spell-checking],
	AS_HELP_STRING([--enable-spell-checking],[enable spell checking support]),
	[case "${enableval}" in
		yes) spell_checking=true ;;
		no)  spell_checking=false ;;
		*) AC_MSG_ERROR([bad value ${enableval} for --enable-spell-checking]) ;;
	esac],[spell_checking=false])

if test $spell_checking = "true"; then
	AC_ERROR([Building with spell checking enabled is not possible, as gtkspell-2.0 does not support GTK+ 3. Use --disable-spell-checking])
	AC_DEFINE([ENABLE_SPELL_CHECKING],[1],[Define if you want spell checking support])
fi

AM_CONDITIONAL([SPELL_CHECKING], [test x$spell_checking = xtrue])

dnl ***************************************************************************
dnl Dependencies
dnl ***************************************************************************

dnl Required dependencies
PKG_CHECK_MODULES(STANDARD, glib-2.0 gtk+-3.0 >= 3.0.0 gmodule-2.0 gthread-2.0 gio-2.0 >= 2.28.0 sqlite3 cairo atk)
AC_SUBST(STANDARD_CFLAGS)
AC_SUBST(STANDARD_LIBS)

dnl Evolution
PKG_CHECK_MODULES(EVO, libecal-1.2 libedataserver-1.2 libedataserverui-3.0 gconf-2.0, have_evo=yes, have_evo=no)
if test "x$have_evo" = "xyes"; then
	AM_GCONF_SOURCE_2
	AC_DEFINE(HAVE_EVO, 1, [Defined if libecal-1.2 is installed])
fi
AM_CONDITIONAL([HAVE_EVO], [test x$have_evo = xyes])

AC_SUBST(EVO_CFLAGS)
AC_SUBST(EVO_LIBS)

dnl Spell checking
if test $spell_checking = "true"; then
	PKG_CHECK_MODULES(SPELL_CHECKING, gtkspell-2.0)
	AC_SUBST(SPELL_CHECKING_CFLAGS)
	AC_SUBST(SPELL_CHECKING_LIBS)
fi

dnl Encryption support
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

	AC_DEFINE_UNQUOTED([MIN_GPGME_VERSION],"${min_gpgme_version}",[The minimum required version of GPGME])

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

# Various required functions and headers
AC_CHECK_HEADERS([libintl.h])
AC_C_INLINE
AC_FUNC_MKTIME
AC_TYPE_SIZE_T

AC_CONFIG_FILES([
Makefile
src/Makefile
po/Makefile.in
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/22x22/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
data/org.gnome.almanah.gschema.xml.in
])
AC_OUTPUT
