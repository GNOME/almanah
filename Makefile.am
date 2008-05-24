SUBDIRS = src po data

EXTRA_DIST =			\
	autogen.sh		\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in	\
	README			\
	COPYING			\
	AUTHORS			\
	INSTALL			\
	NEWS

DISTCLEANFILES = \
	intltool-extract	\
	intltool-merge		\
	intltool-update

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done

