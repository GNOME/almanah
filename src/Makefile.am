INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\"				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"				\
	-I$(srcdir)/definitions						\
	-I$(srcdir)/events						\
	-I$(srcdir)/event-factories					\
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
	$(ALMANAH_MARSHAL_FILES)	\
	definition-builtins.c	\
	definition-builtins.h	\
	event-factory-builtins.c	\
	event-factory-builtins.h	\
	main.c			\
	main.h			\
	interface.c		\
	interface.h		\
	main-window.c		\
	main-window.h		\
	definition-manager-window.c	\
	definition-manager-window.h	\
	storage-manager.c	\
	storage-manager.h	\
	add-definition-dialog.c	\
	add-definition-dialog.h	\
	search-dialog.c		\
	search-dialog.h		\
	printing.c		\
	printing.h		\
	entry.c			\
	entry.h			\
	event.c			\
	event.h			\
	event-factory.c		\
	event-factory.h		\
	event-manager.c		\
	event-manager.h		\
	definition.c		\
	definition.h		\
	date-entry-dialog.c	\
	date-entry-dialog.h	\
	import-dialog.c		\
	import-dialog.h		\
	definitions/contact.c		\
	definitions/contact.h		\
	definitions/file.c		\
	definitions/file.h		\
	definitions/note.c		\
	definitions/note.h		\
	definitions/uri.c		\
	definitions/uri.h		\
	events/calendar-appointment.c	\
	events/calendar-appointment.h	\
	events/calendar-task.c		\
	events/calendar-task.h		\
	events/f-spot-photo.c		\
	events/f-spot-photo.h		\
	event-factories/calendar.c	\
	event-factories/calendar.h	\
	event-factories/calendar-client.c	\
	event-factories/calendar-client.h	\
	event-factories/calendar-debug.h	\
	event-factories/calendar-sources.c	\
	event-factories/calendar-sources.h	\
	event-factories/f-spot.c		\
	event-factories/f-spot.h

if ENCRYPTION
almanah_SOURCES += \
	preferences-dialog.c	\
	preferences-dialog.h
endif

almanah_LDFLAGS = 
almanah_LDADD = \
	$(STANDARD_LIBS)	\
	$(ENCRYPTION_LIBS)	\
	$(SPELL_CHECKING_LIBS)

# Marshalling
ALMANAH_MARSHAL_FILES = \
	almanah-marshal.c	\
	almanah-marshal.h

GLIB_GENMARSHAL = `pkg-config --variable=glib_genmarshal glib-2.0`

almanah-marshal.h: almanah-marshal.list Makefile
	( $(GLIB_GENMARSHAL) --prefix=almanah_marshal $(srcdir)/almanah-marshal.list --header > almanah-marshal.h )
almanah-marshal.c: almanah-marshal.h Makefile
	( $(GLIB_GENMARSHAL) --prefix=almanah_marshal $(srcdir)/almanah-marshal.list --header --body > almanah-marshal.c )

# Enums
definition-builtins.h: stamp-definition-builtins.h
	@true

stamp-definition-builtins.h: definition.h Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __ALMANAH_DEFINITION_BUILTINS_H__\n#define __ALMANAH_DEFINITION_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define ALMANAH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __ALMANAH_DEFINITION_BUILTINS_H__ */" definition.h) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh definition-builtins.h || cp xgen-gtbh definition-builtins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)

definition-builtins.c: definition.h Makefile definition-builtins.h
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"definition.h\"\n#include \"definition-builtins.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		definition.h ) > xgen-gtbc \
	&& cp xgen-gtbc definition-builtins.c  \
	&& rm -f xgen-gtbc

event-factory-builtins.h: stamp-event-factory-builtins.h
	@true

stamp-event-factory-builtins.h: event-factory.h Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __ALMANAH_EVENT_FACTORY_BUILTINS_H__\n#define __ALMANAH_EVENT_FACTORY_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define ALMANAH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __ALMANAH_EVENT_FACTORY_BUILTINS_H__ */" event-factory.h) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh event-factory-builtins.h || cp xgen-gtbh event-factory-builtins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)

event-factory-builtins.c: event-factory.h Makefile event-factory-builtins.h
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"event-factory.h\"\n#include \"event-factory-builtins.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		event-factory.h ) > xgen-gtbc \
	&& cp xgen-gtbc event-factory-builtins.c  \
	&& rm -f xgen-gtbc

CLEANFILES = \
	$(ALMANAH_MARSHAL_FILES)	\
	definition-builtins.h		\
	definition-builtins.c		\
	stamp-definition-builtins.h	\
	event-factory-builtins.h		\
	event-factory-builtins.c		\
	stamp-event-factory-builtins.h

EXTRA_DIST = almanah-marshal.list

-include $(top_srcdir)/git.mk
