include make/ccmake-setup.mk

library   ?= none
CFLAGS    += -Ofast
LDFLAGS   +=
output     = $(targetnm)-$(library).a
src        = $(shell find $(library)/* -type f -name '*.c')
cleanies   = *.a

define main_cc
	@printf "AR: %s\n" "$(notdir $@)"
	@$(AR) -rcs $@ $^
endef

include make/ccmake-recipe.mk
