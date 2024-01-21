ifeq ($(CCDIR),)
	tool_embed     = python3 ./tools/ccembed
	tool_genembed  = python3 ./tools/ccgenembed
	tool_targetnm  = python3 ./tools/cctargetnm
	tool_pikac     = python3 ./tools/ccpikac
else
	CFLAGS        += -I"$(CCDIR)"
	tool_embed     = $(CCDIR)/tools/ccembed
	tool_genembed  = $(CCDIR)/tools/ccgenembed
	tool_targetnm  = $(CCDIR)/tools/cctargetnm
	tool_pikac     = $(CCDIR)/tools/ccpikac
endif

stderr_null = 2> /dev/null
target      = native
extension   =
CC          = gcc
AR          = ar
PKGCONFIG   = pkgconfig
STRIP       = strip

ifeq ($(target),)
else ifeq ($(target),native)
else
	ifeq ($(findstring mingw,$(target)),mingw)
		extension   = .exe
	endif
	CC          = $(target)-gcc
	AR          = $(target)-ar
	PKGCONFIG   = $(target)-pkgconfig
	STRIP       = $(target)-strip
endif

targetnm = $(shell $(tool_targetnm) $(target))

ifeq ($(debug),true)
	targetnm  := $(targetnm)-debug
	CFLAGS    += -g -D__debug__=1
else
	ifeq ($(release),true)
		targetnm  := $(targetnm)-release
		main_strip = $(STRIP) --strip-unneeded $@
	endif
endif

obj_dir = /$(targetnm)

define generate_deps
	@echo -n $(dir $@) > $(@:.o=.mk)
	@$(CC) -MM $(CFLAGS) $< >> $(@:.o=.mk)
endef

define main_cc
	@printf "const char* g_tool_name = \"%s\";\nconst char* g_tool_version = \"%s\";\n" "$(application_name)" "$(application_version)" | $(CC) -c -o bin/version.o -x c -
	@printf "CC: %s\n" "$(notdir $@)"
	@$(CC) -o $@ $^ bin/version.o $(CFLAGS) $(LDFLAGS)
	$(main_strip)
endef
