TESTS = almanah_gtests


CFLAGS = \
	$(STANDARD_CFLAGS) \
	-DALMANAH_TESTING

LDADD = \
	$(STANDARD_LIBS)

noinst_PROGRAMS = $(TESTS)

almanah_gtests_SOURCES = \
	../src/vfs.h \
	../src/vfs.c \
	gtests_vfs.c
