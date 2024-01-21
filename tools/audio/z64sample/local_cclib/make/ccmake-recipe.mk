CFLAGS     += -I. \
	-I$(CCDIR)/clap/include \
	-Wall -lm -lpthread \
	-Wno-multichar \
	-Wno-format-extra-args \
	-Werror=format \
	-Werror=return-type \
	-Werror=unused-result \
	-Wno-scalar-storage-order \
	-fms-extensions \
	-include embed-$(application_name)$(library).h \
	-D__ccname__=\"$(application_name)\" \
	-D__ccversion__=\"$(application_version)\" \
	-D__ccauthor__=\"$(application_author)\"
LDFLAGS    +=

pyi_c      += $(pyi:%.pyi=%.c)
pyi_h      += $(pyi:%.pyi=%.h)
src        += $(pyi_c)

obj_embed  += $(embed:%=bin$(obj_dir)/embed/%.o)
obj        += $(src:%.c=bin$(obj_dir)/%.o) $(obj_embed)
dep        += $(obj:%.o=%.mk)
cleanies   += embed-*.h $(pyi_c) $(pyi_h)

build: $(pyi_c) $(output)

clean:
	rm -rf bin
	rm -f $(cleanies)

clean-all: clean
	@$(MAKE) --no-print-directory -C $(CCDIR) clean library=cclib

# packages
cclib   := $(CCDIR)/$(targetnm)-cclib.a
cctab   := $(CCDIR)/$(targetnm)-cctab.a
ccmath  := $(CCDIR)/$(targetnm)-ccmath.a
pikapy  := $(CCDIR)/$(targetnm)-pikapy.a
clap    :=

$(cclib):
	@$(MAKE) --no-print-directory -C $(CCDIR) target=$(target) debug=$(debug) library=cclib
$(cctab):
	@$(MAKE) --no-print-directory -C $(CCDIR) target=$(target) debug=$(debug) library=cctab
$(ccmath):
	@$(MAKE) --no-print-directory -C $(CCDIR) target=$(target) debug=$(debug) library=ccmath
$(pikapy):
	@$(MAKE) --no-print-directory -C $(CCDIR) target=$(target) debug=$(debug) library=pikapy

.PHONY: $(cclib) $(cctab) $(pikapy) $(ccmath) $(clap) clean clean-all build

bin$(obj_dir)/pikapy/%.o: CFLAGS += \
	-I pikapy/pikascript-api \
	-I pikapy/pikascript-core \
	-I pikapy/pikascript-lib \
	-Wno-unknown-pragmas \
	-Wno-unused-function \
	-Wno-unused-but-set-variable

-include $(dep)

%.c: %.pyi
	@$(tool_pikac) $(dir $<) $(basename $(notdir $<))
	@touch -a $(@:%.c=%.impl.c)

embed-$(application_name)$(library).h: $(obj_embed)
	@printf "EM: %s\n" "$(notdir $@)"
	@$(tool_genembed) "$(embed)" > embed-$(application_name)$(library).h

bin$(obj_dir)/embed/%.o: % Makefile
	@mkdir -p $(dir $@)
	@printf "EM: %s\n" "$(notdir $@)"
	@$(tool_embed) -cc $(CC) -i $< -o $@

bin$(obj_dir)/%.o: %.c Makefile embed-$(application_name)$(library).h
	@mkdir -p $(dir $@)
	@printf "CC: %s\n" "$(notdir $@)"
	@$(CC) -c -o $@ $< $(CFLAGS)
	$(generate_deps)

$(output): $(pikascript) $(obj) $(libraries)
	$(main_cc)
