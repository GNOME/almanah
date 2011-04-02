bin_PROGRAMS = almanah

almanah_SOURCES = \
	$(ALMANAH_MARSHAL_FILES)	\
	$(ALMANAH_ENUM_FILES)		\
	main.c				\
	main.h				\
	interface.c			\
	interface.h			\
	main-window.c			\
	main-window.h			\
	definition-manager-window.c	\
	definition-manager-window.h	\
	storage-manager.c		\
	storage-manager.h		\
	add-definition-dialog.c		\
	add-definition-dialog.h		\
	search-dialog.c			\
	search-dialog.h			\
	printing.c			\
	printing.h			\
	entry.c				\
	entry.h				\
	event.c				\
	event.h				\
	event-factory.c			\
	event-factory.h			\
	event-manager.c			\
	event-manager.h			\
	definition.c			\
	definition.h			\
	date-entry-dialog.c		\
	date-entry-dialog.h		\
	import-export-dialog.c		\
	import-export-dialog.h		\
	import-operation.c		\
	import-operation.h		\
	export-operation.c		\
	export-operation.h		\
	definitions/file.c		\
	definitions/file.h		\
	definitions/note.c		\
	definitions/note.h		\
	definitions/uri.c		\
	definitions/uri.h		\
	widgets/calendar.c		\
	widgets/calendar.h

if HAVE_EVO
almanah_SOURCES += \
	event-factories/calendar.c		\
	event-factories/calendar.h		\
	event-factories/calendar-client.c	\
	event-factories/calendar-client.h	\
	event-factories/calendar-debug.h	\
	event-factories/calendar-sources.c	\
	event-factories/calendar-sources.h	\
	events/calendar-appointment.c		\
	events/calendar-appointment.h		\
	events/calendar-task.c			\
	events/calendar-task.h			\
	definitions/contact.c			\
	definitions/contact.h
endif

if ENCRYPTION
almanah_SOURCES += \
	preferences-dialog.c	\
	preferences-dialog.h
endif

almanah_CPPFLAGS = \
	-DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\"	\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"		\
	-I$(srcdir)/definitions				\
	-I$(srcdir)/events				\
	-I$(srcdir)/event-factories			\
	$(DISABLE_DEPRECATED)				\
	$(AM_CPPFLAGS)

almanah_CFLAGS = \
	$(STANDARD_CFLAGS)		\
	$(EVO_CFLAGS)			\
	$(ENCRYPTION_CFLAGS)		\
	$(SPELL_CHECKING_CFLAGS)	\
	$(WARN_CFLAGS)			\
	$(AM_CFLAGS)

almanah_LDADD = \
	$(STANDARD_LIBS)	\
	$(EVO_LIBS)		\
	$(ENCRYPTION_LIBS)	\
	$(SPELL_CHECKING_LIBS)	\
	$(AM_LDADD)

# Marshalling
ALMANAH_MARSHAL_FILES = \
	almanah-marshal.c	\
	almanah-marshal.h

almanah-marshal.h: almanah-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=almanah_marshal --header $< > $@
almanah-marshal.c: almanah-marshal.list almanah-marshal.h
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=almanah_marshal --header --body $< > $@

# Enums
ALMANAH_ENUM_FILES = \
	definition-builtins.h		\
	definition-builtins.c		\
	event-factory-builtins.h	\
	event-factory-builtins.c

definition-builtins.h: definition.h Makefile
	$(AM_V_GEN)(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __ALMANAH_DEFINITION_BUILTINS_H__\n#define __ALMANAH_DEFINITION_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define ALMANAH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __ALMANAH_DEFINITION_BUILTINS_H__ */" \
		definition.h) > $(@F)

definition-builtins.c: definition.h Makefile definition-builtins.h
	$(AM_V_GEN)(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"definition.h\"\n#include \"definition-builtins.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		definition.h) > $(@F)

event-factory-builtins.h: event-factory.h Makefile
	$(AM_V_GEN)(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __ALMANAH_EVENT_FACTORY_BUILTINS_H__\n#define __ALMANAH_EVENT_FACTORY_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define ALMANAH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __ALMANAH_EVENT_FACTORY_BUILTINS_H__ */" \
		event-factory.h) > $(@F)

event-factory-builtins.c: event-factory.h Makefile event-factory-builtins.h
	$(AM_V_GEN)(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"event-factory.h\"\n#include \"event-factory-builtins.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		event-factory.h) > $(@F)

CLEANFILES = \
	$(ALMANAH_MARSHAL_FILES)	\
	$(ALMANAH_ENUM_FILES)

EXTRA_DIST = almanah-marshal.list

-include $(top_srcdir)/git.mk
