include project-config.mk
include $(CCDIR)/make/ccmake-setup.mk

application_name    = z64sample
application_version = 0.1.5
application_author  = z64tools

CFLAGS    += -Ofast -msse2 -mfpmath=sse
LDFLAGS   +=

output     = $(application_name)-$(targetnm)$(extension)
cleanies   = $(application_name)-*
libraries  = $(cclib) $(ccmath)

src       += $(shell find src -type f -name '*.c' $(stderr_null))
embed     += $(shell find src/emb -type f $(stderr_null))
pyi       += $(shell find src/pydef -type f -name '*.pyi' $(stderr_null))

include $(CCDIR)/make/ccmake-recipe.mk
