CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c99 -g -O2
PROGRAMS := vtxdis

.PHONY: all clean distclean

all: $(PROGRAMS)
	$(MAKE) -C ZAPD
	$(MAKE) -C fado
	$(MAKE) -C buildtools
# Some gcc versions give a warning about the -flto flag with no parameters. So we override the CFLAGS as a workaround
	$(MAKE) -C z64compress CFLAGS="-Os -flto=auto"
	$(MAKE) -C audio

clean:
	$(RM) $(PROGRAMS)
	$(MAKE) -C ZAPD clean
	$(MAKE) -C fado clean
	$(MAKE) -C buildtools clean
	$(MAKE) -C z64compress clean
	$(MAKE) -C audio clean

distclean: clean
	$(MAKE) -C audio distclean

vtxdis_SOURCES	   := vtxdis.c

define COMPILE =
$(1): $($1_SOURCES)
	$(CC) $(CFLAGS) $$^ -o $$@
endef

$(foreach p,$(PROGRAMS),$(eval $(call COMPILE,$(p))))
