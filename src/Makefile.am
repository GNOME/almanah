INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\"				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"				\
	$(STANDARD_CFLAGS)						\
	$(ENCRYPTION_CFLAGS)						\
	$(SPELL_CHECKING_CFLAGS)

AM_CFLAGS = \
	-Wall		\
	-g

if DEBUG
AM_CFLAGS += \
	-fprofile-arcs	\
	-ftest-coverage
endif

bin_PROGRAMS = almanah

almanah_SOURCES = \
	main.c			\
	main.h			\
	interface.c		\
	interface.h		\
	main-window.c		\
	main-window.h		\
	storage-manager.c	\
	storage-manager.h	\
	add-link-dialog.c	\
	add-link-dialog.h	\
	search-dialog.c		\
	search-dialog.h		\
	printing.c		\
	printing.h		\
	entry.c			\
	entry.h			\
	link.c			\
	link.h			\
	links/file.c		\
	links/note.c		\
	links/uri.c

almanah_LDFLAGS = 
almanah_LDADD = \
	$(STANDARD_LIBS)	\
	$(ENCRYPTION_LIBS)	\
	$(SPELL_CHECKING_LIBS)
