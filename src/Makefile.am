INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\"				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"				\
	$(GTK_CFLAGS)

AM_CFLAGS = \
	-Wall		\
	-g

if DEBUG
	AM_CFLAGS += \
		-fprofile-arcs	\
		-ftest-coverage
endif

bin_PROGRAMS = diary

diary_SOURCES = \
	main.c			\
	main.h			\
	interface.c		\
	interface.h		\
	main-window.c		\
	main-window.h		\
	storage-manager.c	\
	storage-manager-h	\
	add-link-dialog.c	\
	add-link-dialog.h	\
	link.c			\
	link.h			\
	links/email.c		\
	links/uri.c		\
	links/file.c		\
	links/picasa.c

diary_LDFLAGS = 
diary_LDADD = $(GTK_LIBS)
