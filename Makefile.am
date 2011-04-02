SUBDIRS = src po data

EXTRA_DIST =			\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	README			\
	COPYING			\
	AUTHORS			\
	NEWS			\
	MAINTAINERS		\
	ChangeLog.pre-git	\
	almanah.doap

DISTCLEANFILES = \
	intltool-extract	\
	intltool-merge		\
	intltool-update

MAINTAINERCLEANFILES = \
	INSTALL		\
	aclocal.m4	\
	compile		\
	config.guess	\
	config.h.in	\
	config.sub	\
	depcomp		\
	install-sh	\
	ltmain.sh	\
	missing		\
	mkinstalldirs	\
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	ABOUT-NLS \
	config.rpath \
	po/Makevars.template \
	po/Rules-quot \
	po/boldquot.sed \
	po/en@boldquot.header \
	po/en@quot.header \
	po/insert-header.sin \
	po/quot.sed \
	po/remove-potcdate.sin \
	$(NULL)

# Copy all the spec files. Of course, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

# ChangeLog
ChangeLog: $(srcdir)/ChangeLog
$(srcdir)/ChangeLog:
	$(AM_V_GEN)(if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run \
	   git log d8d3d82934d1627da5e49c8784b5297e27550e69.. --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi)
.PHONY: $(srcdir)/ChangeLog

MAINTAINERCLEANFILES += ChangeLog

-include $(top_srcdir)/git.mk
