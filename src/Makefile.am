INCLUDES = \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"	\
	-DPACKAGE_SRC_DIR=\""$(srcdir)"\"				\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"				\
	-I$(srcdir)/definitions						\
	-I$(srcdir)/links						\
	-I$(srcdir)/link-factories					\
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
	definition-builtins.c	\
	definition-builtins.h	\
	link-factory-builtins.c	\
	link-factory-builtins.h	\
	main.c			\
	main.h			\
	interface.c		\
	interface.h		\
	main-window.c		\
	main-window.h		\
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
	link.c			\
	link.h			\
	link-factory.c		\
	link-factory.h		\
	link-manager.c		\
	link-manager.h		\
	definition.c		\
	definition.h		\
	definitions/file.c		\
	definitions/file.h		\
	definitions/note.c		\
	definitions/note.h		\
	definitions/uri.c		\
	definitions/uri.h		\
	links/calendar-appointment.c	\
	links/calendar-appointment.h	\
	links/calendar-task.c		\
	links/calendar-task.h		\
	link-factories/calendar.c	\
	link-factories/calendar.h	\
	link-factories/calendar-client.c	\
	link-factories/calendar-client.h	\
	link-factories/calendar-debug.h		\
	link-factories/calendar-sources.c	\
	link-factories/calendar-sources.h

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

link-factory-builtins.h: stamp-link-factory-builtins.h
	@true

stamp-link-factory-builtins.h: link-factory.h Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __ALMANAH_LINK_FACTORY_BUILTINS_H__\n#define __ALMANAH_LINK_FACTORY_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define ALMANAH_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __ALMANAH_LINK_FACTORY_BUILTINS_H__ */" link-factory.h) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh link-factory-builtins.h || cp xgen-gtbh link-factory-builtins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)

link-factory-builtins.c: link-factory.h Makefile link-factory-builtins.h
	(cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include \"link-factory.h\"\n#include \"link-factory-builtins.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		link-factory.h ) > xgen-gtbc \
	&& cp xgen-gtbc link-factory-builtins.c  \
	&& rm -f xgen-gtbc

CLEANFILES = \
	definition-builtins.h		\
	definition-builtins.c		\
	stamp-definition-builtins.h	\
	link-factory-builtins.h		\
	link-factory-builtins.c		\
	stamp-link-factory-builtins.h
