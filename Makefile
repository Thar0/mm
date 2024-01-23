# Build options can be changed by modifying the makefile or by building with 'make SETTING=value'.
# It is also possible to override the settings in Defaults in a file called .make_options as 'SETTING=value'.

-include .make_options

MAKEFLAGS += --no-builtin-rules

#### Defaults ####

# If COMPARE is 1, check the output md5sum after building
COMPARE ?= 1
# If NON_MATCHING is 1, define the NON_MATCHING C flag when building
NON_MATCHING ?= 0
# If ORIG_COMPILER is 1, compile with QEMU_IRIX and the original compiler
ORIG_COMPILER ?= 0
# if WERROR is 1, pass -Werror to CC_CHECK, so warnings would be treated as errors
WERROR ?= 0
# Keep .mdebug section in build
KEEP_MDEBUG ?= 0
# Disassembles all asm from the ROM instead of skipping files which are entirely in C
FULL_DISASM ?= 0
# Check code syntax with host compiler
RUN_CC_CHECK ?= 1
# Dump build object files
OBJDUMP_BUILD ?= 0
# Force asm processor to run on every file
ASM_PROC_FORCE ?= 0
# Number of threads to disassmble, extract, and compress with
N_THREADS ?= $(shell nproc)
# MIPS toolchain
MIPS_BINUTILS_PREFIX ?= mips-linux-gnu-
# Music Macro Language Version
MML_VERSION := 1
#### Setup ####

# Ensure the map file being created using English localization
export LANG := C

ifeq ($(NON_MATCHING),1)
  CFLAGS := -DNON_MATCHING
  CPPFLAGS := -DNON_MATCHING
  COMPARE := 0
endif

DISASM_FLAGS := --reg-names=o32
ifneq ($(FULL_DISASM), 0)
  DISASM_FLAGS += --all
endif

PROJECT_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

MAKE = make
CPPFLAGS += -P

ifeq ($(OS),Windows_NT)
  DETECTED_OS=windows
else
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Linux)
    DETECTED_OS=linux
  endif
  ifeq ($(UNAME_S),Darwin)
    DETECTED_OS=macos
    MAKE=gmake
    CPPFLAGS += -xc++
  endif
endif

#### Tools ####
ifneq ($(shell type $(MIPS_BINUTILS_PREFIX)ld >/dev/null 2>/dev/null; echo $$?), 0)
  $(error Unable to find $(MIPS_BINUTILS_PREFIX)ld. Please install or build MIPS binutils, commonly mips-linux-gnu. (or set MIPS_BINUTILS_PREFIX if your MIPS binutils install uses another prefix))
endif

CC       := tools/ido_recomp/$(DETECTED_OS)/7.1/cc
CC_OLD   := tools/ido_recomp/$(DETECTED_OS)/5.3/cc

# if ORIG_COMPILER is 1, check that either QEMU_IRIX is set or qemu-irix package installed
ifeq ($(ORIG_COMPILER),1)
  ifndef QEMU_IRIX
    QEMU_IRIX := $(shell which qemu-irix)
    ifeq (, $(QEMU_IRIX))
      $(error Please install qemu-irix package or set QEMU_IRIX env var to the full qemu-irix binary path)
    endif
  endif
  CC        = $(QEMU_IRIX) -L tools/ido7.1_compiler tools/ido7.1_compiler/usr/bin/cc
  CC_OLD    = $(QEMU_IRIX) -L tools/ido5.3_compiler tools/ido5.3_compiler/usr/bin/cc
endif

AS         := $(MIPS_BINUTILS_PREFIX)as
LD         := $(MIPS_BINUTILS_PREFIX)ld
OBJCOPY    := $(MIPS_BINUTILS_PREFIX)objcopy
OBJDUMP    := $(MIPS_BINUTILS_PREFIX)objdump
ASM_PROC   := python3 tools/asm-processor/build.py

ASM_PROC_FLAGS := --input-enc=utf-8 --output-enc=euc-jp --convert-statics=global-with-filename

ifneq ($(ASM_PROC_FORCE), 0)
	ASM_PROC_FLAGS += --force
endif

IINC       := -Iinclude -Isrc -Iassets -Ibuild -I.

ifeq ($(KEEP_MDEBUG),0)
  RM_MDEBUG = $(OBJCOPY) --remove-section .mdebug $@
else
  RM_MDEBUG = @:
endif

# Check code syntax with host compiler
ifneq ($(RUN_CC_CHECK),0)
  CHECK_WARNINGS := -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-variable -Wno-missing-braces -Wno-unused-but-set-variable -Wno-unused-label -Wno-sign-compare -Wno-tautological-compare
  CC_CHECK   := gcc -fno-builtin -fsyntax-only -funsigned-char -fdiagnostics-color -std=gnu89 -D _LANGUAGE_C -D NON_MATCHING $(IINC) -nostdinc $(CHECK_WARNINGS)
  ifneq ($(WERROR), 0)
    CC_CHECK += -Werror
  endif
else
  CC_CHECK := @:
endif

CPP        := cpp
ELF2ROM    := tools/buildtools/elf2rom
MKLDSCRIPT := tools/buildtools/mkldscript
MKDMADATA  := tools/buildtools/mkdmadata
YAZ0       := tools/buildtools/yaz0
ZAPD       := tools/ZAPD/ZAPD.out
FADO       := tools/fado/fado.elf
MAKEYAR    := tools/buildtools/makeyar

AUDIO_EXTRACT := python3 tools/audio/extraction/audio_extract.py
Z64SAMPLE     := tools/audio/z64sample/z64sample-native --silent
SBC           := tools/audio/sbc --matching
SFC           := tools/audio/sfc --matching
ELFPATCH      := tools/audio/elfpatch
ATBLGEN       := tools/audio/atblgen
AFILE_SIZES   := tools/audio/afile_sizes

# We want linemarkers in sequence assembly files for better assembler error messages
SEQ_CPP := cpp -fno-dollars-in-identifiers
SEQ_CPPFLAGS := -D_LANGUAGE_ASEQ -DMML_VERSION=$(MML_VERSION) -I include/audio -I include/tables/sfx -I build/assets/audio/soundfonts

OPTFLAGS := -O2 -g3
ASFLAGS := -march=vr4300 -32 -Iinclude
MIPS_VERSION := -mips2

# we support Microsoft extensions such as anonymous structs, which the compiler does support but warns for their usage. Suppress the warnings with -woff.
CFLAGS += -G 0 -non_shared -fullwarn -verbose -Xcpluscomm $(IINC) -nostdinc -Wab,-r4300_mul -woff 649,838,712,516,807

# Use relocations and abi fpr names in the dump
OBJDUMP_FLAGS := --disassemble --reloc --disassemble-zeroes -Mreg-names=32

ifneq ($(OBJDUMP_BUILD), 0)
  OBJDUMP_CMD = $(OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(@:.o=.s)
  OBJCOPY_BIN = $(OBJCOPY) -O binary $@ $@.bin
else
  OBJDUMP_CMD = @:
  OBJCOPY_BIN = @:
endif

ifeq ($(shell getconf LONG_BIT), 32)
  # Work around memory allocation bug in QEMU
  export QEMU_GUEST_BASE := 1
else ifneq ($(RUN_CC_CHECK),0)
  # Ensure that gcc treats the code as 32-bit
  CC_CHECK += -m32
endif

# rom compression flags
COMPFLAGS := --threads $(N_THREADS)
ifneq ($(NON_MATCHING),1)
  COMPFLAGS += --matching
endif

#### Files ####

# ROM image
ROMC := mm.us.rev1.rom.z64
ROM := $(ROMC:.rom.z64=.rom_uncompressed.z64)
ELF := $(ROM:.z64=.elf)
# description of ROM segments
SPEC := spec

# create asm directories
$(shell mkdir -p asm data)

SRC_DIRS := $(shell find src -type d)
ASM_DIRS := $(shell find asm -type d -not -path "asm/non_matchings*") $(shell find data -type d)

AIFF_DIRS := $(shell find assets/audio/samples -type d)
AIFF_FILES := $(foreach dir,$(AIFF_DIRS),$(wildcard $(dir)/*.wav))
AIFC_FILES := $(foreach f,$(AIFF_FILES),build/$(f:.wav=.aifc))

SAMPLEBANK_DIRS := $(shell find assets/audio/samplebanks -type d)
SAMPLEBANK_XMLS := $(foreach dir,$(SAMPLEBANK_DIRS),$(wildcard $(dir)/*.xml))
SAMPLEBANK_O_FILES := $(foreach f,$(SAMPLEBANK_XMLS),build/$(f:.xml=.o))

SOUNDFONT_DIRS := $(shell find assets/audio/soundfonts -type d)
SOUNDFONT_XMLS := $(foreach dir,$(SOUNDFONT_DIRS),$(wildcard $(dir)/*.xml))
SOUNDFONT_O_FILES := $(foreach f,$(SOUNDFONT_XMLS),build/$(f:.xml=.o))
SOUNDFONT_HEADERS := $(foreach f,$(SOUNDFONT_XMLS),build/$(f:.xml=.h))

SEQUENCE_DIRS := $(shell find assets/audio/sequences -type d)
SEQUENCE_FILES := $(foreach dir,$(SEQUENCE_DIRS),$(wildcard $(dir)/*.seq))
SEQUENCE_O_FILES := $(foreach f,$(SEQUENCE_FILES:.seq=.o),build/$f)

SEQUENCE_TABLE := include/tables/sequence_table.h

## Assets binaries (PNGs, JPGs, etc)
ASSET_BIN_DIRS := $(shell find assets/* -type d -not -path "assets/xml*" -not -path "assets/audio*" -not -path "assets/c/*" -not -name "c" -not -path "assets/text")
# Prevents building C files that will be #include'd
ASSET_BIN_DIRS_C_FILES := $(shell find assets/* -type d -not -path "assets/xml*" -not -path "assets/code*" -not -path "assets/overlays*")

ASSET_FILES_BIN := $(foreach dir,$(ASSET_BIN_DIRS),$(wildcard $(dir)/*.bin))
ASSET_FILES_OUT := $(foreach f,$(ASSET_FILES_BIN:.bin=.bin.inc.c),build/$f)

TEXTURE_FILES_PNG := $(foreach dir,$(ASSET_BIN_DIRS),$(wildcard $(dir)/*.png))
TEXTURE_FILES_JPG := $(foreach dir,$(ASSET_BIN_DIRS),$(wildcard $(dir)/*.jpg))
TEXTURE_FILES_OUT := $(foreach f,$(TEXTURE_FILES_PNG:.png=.inc.c),build/$f) \
					 $(foreach f,$(TEXTURE_FILES_JPG:.jpg=.jpg.inc.c),build/$f) \

C_FILES       := $(foreach dir,$(SRC_DIRS) $(ASSET_BIN_DIRS_C_FILES),$(wildcard $(dir)/*.c))
S_FILES       := $(shell grep -F "build/asm" spec | sed 's/.*build\/// ; s/\.o\".*/.s/') \
                 $(shell grep -F "build/data" spec | sed 's/.*build\/// ; s/\.o\".*/.s/')
BASEROM_FILES := $(shell grep -F "build/baserom" spec | sed 's/.*build\/// ; s/\.o\".*//')
ARCHIVES_O    := $(shell grep -F ".yar.o" spec | sed 's/.*include "// ; s/\.o\".*/.o/')
O_FILES       := $(foreach f,$(S_FILES:.s=.o),build/$f) \
                 $(foreach f,$(C_FILES:.c=.o),build/$f) \
                 $(foreach f,$(BASEROM_FILES),build/$f.o) \
                 $(ARCHIVES_O)

OVL_RELOC_FILES := $(shell $(CPP) $(CPPFLAGS) $(SPEC) | grep -o '[^"]*_reloc.o' )

# Automatic dependency files
# (Only asm_processor dependencies and reloc dependencies are handled for now)
DEP_FILES := $(O_FILES:.o=.asmproc.d) $(OVL_RELOC_FILES:.o=.d)

# create build directories
$(shell mkdir -p build/baserom build build/include/tables $(foreach dir,$(SRC_DIRS) $(ASM_DIRS) $(ASSET_BIN_DIRS) $(ASSET_BIN_DIRS_C_FILES) $(AIFF_DIRS) $(SAMPLEBANK_DIRS) $(SOUNDFONT_DIRS) $(SEQUENCE_DIRS),build/$(dir)))

# directory flags
build/src/boot/O2/%.o: OPTFLAGS := -O2

build/src/boot/libc/%.o: OPTFLAGS := -O2
build/src/boot/libm/%.o: OPTFLAGS := -O2
build/src/boot/libc64/%.o: OPTFLAGS := -O2

build/src/libultra/os/%.o: OPTFLAGS := -O1
build/src/libultra/voice/%.o: OPTFLAGS := -O2
build/src/libultra/io/%.o: OPTFLAGS := -O2
build/src/libultra/libc/%.o: OPTFLAGS := -O2
build/src/libultra/gu/%.o: OPTFLAGS := -O2
build/src/libultra/rmon/%.o: OPTFLAGS := -O2

build/src/audio/%.o: OPTFLAGS := -O2

build/assets/%.o: OPTFLAGS := -O1
build/assets/%.o: ASM_PROC_FLAGS := 

# file flags
build/src/boot/fault.o: CFLAGS += -trapuv
build/src/boot/fault_drawer.o: CFLAGS += -trapuv

build/src/code/jpegutils.o: OPTFLAGS := -O2
build/src/code/jpegdecoder.o: OPTFLAGS := -O2
build/src/code/jpegutils.o: CC := $(CC_OLD)
build/src/code/jpegdecoder.o: CC := $(CC_OLD)

build/src/code/osFlash.o: OPTFLAGS := -g
build/src/code/osFlash.o: MIPS_VERSION := -mips1
build/src/code/osFlash.o: CC := $(CC_OLD)

build/src/libultra/libc/ll.o: OPTFLAGS := -O1
build/src/libultra/libc/ll.o: MIPS_VERSION := -mips3 -32
build/src/libultra/libc/llcvt.o: OPTFLAGS := -O1
build/src/libultra/libc/llcvt.o: MIPS_VERSION := -mips3 -32

# cc & asm-processor
build/src/boot/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC) -- $(AS) $(ASFLAGS) --
build/src/boot/O2/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC) -- $(AS) $(ASFLAGS) --

build/src/libultra/%.o: CC := $(CC_OLD)
# Needed at least until voice is decompiled
build/src/libultra/voice/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC_OLD) -- $(AS) $(ASFLAGS) --

build/src/code/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC) -- $(AS) $(ASFLAGS) --
build/src/audio/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC) -- $(AS) $(ASFLAGS) --

build/src/overlays/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC) -- $(AS) $(ASFLAGS) --

build/assets/%.o: CC := $(ASM_PROC) $(ASM_PROC_FLAGS) $(CC) -- $(AS) $(ASFLAGS) --

#### Main Targets ###

uncompressed: $(ROM)
ifeq ($(COMPARE),1)
	@md5sum $(ROM)
	@md5sum -c checksum_uncompressed.md5
endif

compressed: $(ROMC)
ifeq ($(COMPARE),1)
	@md5sum $(ROMC)
	@md5sum -c checksum.md5
endif

.PHONY: all uncompressed compressed clean assetclean distclean assets disasm init setup
.DEFAULT_GOAL := uncompressed
all: uncompressed compressed

$(ROM): $(ELF)
	$(ELF2ROM) -cic 6105 $< $@

$(ROMC): $(ROM)
	python3 tools/z64compress_wrapper.py $(COMPFLAGS) $(ROM) $@ $(ELF) build/$(SPEC)

$(ELF): $(TEXTURE_FILES_OUT) $(ASSET_FILES_OUT) $(O_FILES) $(OVL_RELOC_FILES) build/ldscript.txt build/undefined_syms.txt \
        $(AIFC_FILES) \
        $(SAMPLEBANK_O_FILES) \
        $(SOUNDFONT_O_FILES) \
        $(SEQUENCE_O_FILES) \
        build/src/audio/tables/samplebank_table.o \
        build/src/audio/tables/soundfont_table.o \
        build/src/audio/tables/sequence_table.o \
        build/assets/audio/sequence_font_table.o \
        build/assets/audio/sequence_sizes.h \
        build/assets/audio/soundfont_sizes.h
	$(LD) -T build/undefined_syms.txt -T build/ldscript.txt --no-check-sections --accept-unknown-input-arch --emit-relocs -Map build/mm.map -o $@

## Order-only prerequisites 
# These ensure e.g. the O_FILES are built before the OVL_RELOC_FILES.
# The intermediate phony targets avoid quadratically-many dependencies between the targets and prerequisites.

o_files: $(O_FILES)
$(OVL_RELOC_FILES): | o_files

asset_files: $(TEXTURE_FILES_OUT) $(ASSET_FILES_OUT)
$(O_FILES): | asset_files

.PHONY: o_files asset_files

#### Main commands ####

## Cleaning ##
clean:
	$(RM) -rf $(ROMC) $(ROM) $(ELF) build

assetclean:
	$(RM) -rf $(ASSET_BIN_DIRS)
	$(RM) -rf assets/text/*.h
	$(RM) -rf build/assets
	$(RM) -rf .extracted-assets.json
	$(RM) -rf assets/audio/samplebanks
	$(RM) -rf assets/audio/samples
	$(RM) -rf assets/audio/soundfonts
	find assets/audio/sequences -type f -name "*.seq" -not -name "*.prg.seq" -delete

distclean: assetclean clean
	$(RM) -rf asm baserom data
	$(MAKE) -C tools distclean

## Extraction step
setup:
	$(MAKE) -C tools
	python3 tools/fixbaserom.py
	python3 tools/extract_baserom.py
	python3 tools/decompress_yars.py
	$(AUDIO_EXTRACT) -r baserom_uncompressed.z64 -v mm_u0 --full --write-xml

assets:
	python3 extract_assets.py -j $(N_THREADS) -Z Wno-hardcoded-pointer

## Assembly generation
disasm:
	$(RM) -rf asm data
	python3 tools/disasm/disasm.py -j $(N_THREADS) $(DISASM_FLAGS)

diff-init: uncompressed
	$(RM) -rf expected/
	mkdir -p expected/
	cp -r build expected/build

init:
	$(MAKE) distclean
	$(MAKE) setup
	$(MAKE) assets
	$(MAKE) disasm
	$(MAKE) all
	$(MAKE) diff-init

#### Various Recipes ####

build/undefined_syms.txt: undefined_syms.txt
	$(CPP) $(CPPFLAGS) $< > build/undefined_syms.txt

build/$(SPEC): $(SPEC)
	$(CPP) $(CPPFLAGS) $< > $@

build/ldscript.txt: build/$(SPEC)
	$(MKLDSCRIPT) $< $@

build/dmadata_table_spec.h: build/$(SPEC)
	$(MKDMADATA) $< $@

# Dependencies for files that may include the dmadata header automatically generated from the spec file
build/src/boot/z_std_dma.o: build/dmadata_table_spec.h
build/src/dmadata/dmadata.o: build/dmadata_table_spec.h

build/asm/%.o: asm/%.s
	$(AS) $(ASFLAGS) $< -o $@

build/assets/%.o: assets/%.c
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -o $@ $<
	$(OBJCOPY_BIN)
	$(RM_MDEBUG)

build/%.yar.o: build/%.o
	$(MAKEYAR) $< $(@:.yar.o=.yar.bin) $(@:.yar.o=.symbols.o)
	$(OBJCOPY) -I binary -O elf32-big $(@:.yar.o=.yar.bin) $@

build/baserom/%.o: baserom/%
	$(OBJCOPY) -I binary -O elf32-big $< $@

build/data/%.o: data/%.s
	$(AS) $(ASFLAGS) $< -o $@

build/assets/text/message_data.enc.h: assets/text/message_data.h
	python3 tools/msg/nes/msgencNES.py -o $@ $<

build/assets/text/staff_message_data.enc.h: assets/text/staff_message_data.h
	python3 tools/msg/staff/msgencStaff.py -o $@ $<

build/assets/text/message_data_static.o: build/assets/text/message_data.enc.h
build/assets/text/staff_message_data_static.o: build/assets/text/staff_message_data.enc.h
build/src/code/z_message.o: build/assets/text/message_data.enc.h build/assets/text/staff_message_data.enc.h

build/src/overlays/%.o: src/overlays/%.c
	$(CC_CHECK) $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -o $@ $<
	@$(OBJDUMP) -d $@ > $(@:.o=.s)
	$(RM_MDEBUG)

build/src/overlays/%_reloc.o: build/$(SPEC)
	$(FADO) $$(tools/buildtools/reloc_prereq $< $(notdir $*)) -n $(notdir $*) -o $(@:.o=.s) -M $(@:.o=.d)
	$(AS) $(ASFLAGS) $(@:.o=.s) -o $@

build/src/%.o: src/%.c
	$(CC_CHECK) $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -o $@ $<
	$(OBJDUMP_CMD)
	$(RM_MDEBUG)

build/src/libultra/libc/ll.o: src/libultra/libc/ll.c
	$(CC_CHECK) $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -o $@ $<
	python3 tools/set_o32abi_bit.py $@
	$(OBJDUMP_CMD)
	$(RM_MDEBUG)

build/src/libultra/libc/llcvt.o: src/libultra/libc/llcvt.c
	$(CC_CHECK) $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -o $@ $<
	python3 tools/set_o32abi_bit.py $@
	$(OBJDUMP_CMD)
	$(RM_MDEBUG)

# Build C files from assets

build/%.inc.c: %.png
	$(ZAPD) btex -eh -tt $(subst .,,$(suffix $*)) -i $< -o $@

build/assets/%.bin.inc.c: assets/%.bin
	$(ZAPD) bblb -eh -i $< -o $@

build/assets/%.jpg.inc.c: assets/%.jpg
	$(ZAPD) bren -eh -i $< -o $@

# Audio

# first build samples...

build/assets/audio/samples/%.half.aifc: assets/audio/samples/%.half.wav
	$(Z64SAMPLE) -t vadpcm-half $< $@
# TESTING:
#	@(cmp $(<D)/aifc/$(<F:.half.wav=.aifc) $@ && echo "$(<F) OK") || (mkdir -p NONMATCHINGS/$(<D) && cp $(<D)/aifc/$(<F:.half.wav=.aifc) NONMATCHINGS/$(<D)/$(<F:.half.wav=.aifc))

build/assets/audio/samples/%.aifc: assets/audio/samples/%.wav
	$(Z64SAMPLE) -t vadpcm $< $@
# TESTING:
#	@(cmp $(<D)/aifc/$(<F:.wav=.aifc) $@ && echo "$(<F) OK") || (mkdir -p NONMATCHINGS/$(<D) && cp $(<D)/aifc/$(<F:.wav=.aifc) NONMATCHINGS/$(<D)/$(<F:.wav=.aifc))

# then assemble the samplebanks... TODO have sbc handle dependency generation?

build/assets/audio/samplebanks/%.o: assets/audio/samplebanks/%.xml $(AIFC_FILES)
	$(SBC) $< $(@:.o=.s)
	$(AS) $(ASFLAGS) $(@:.o=.s) -o $@
	$(OBJCOPY) -O binary -j.rodata $@ $(@:.o=.bin)
# TESTING:
#	@cmp $(@:.o=.bin) $(patsubst build/assets/audio/samplebanks/%,baserom/audiotable_files/%,$(@:.o=.bin)) && echo "$(<F) OK"

# also assemble the soundfonts and generate the associated headers... TODO have sfc handle dependency generation?

.PRECIOUS: build/assets/audio/soundfonts/%.c build/assets/audio/soundfonts/%.h
build/assets/audio/soundfonts/%.c build/assets/audio/soundfonts/%.h: assets/audio/soundfonts/%.xml $(AIFC_FILES) $(SAMPLEBANK_XMLS)
# This rule can be triggered for either the .c or .h file, so $@ may refer to either the .c or .h file. A simple
# substitution $(@:.c=.h) will fail ~50% of the time with -j. Instead, don't assume anything about the suffix of $@.
	$(SFC) $< $(@:$(suffix $(@F))=.c) $(@:$(suffix $(@F))=.h)

build/assets/audio/soundfonts/%.o: build/assets/audio/soundfonts/%.c $(SAMPLEBANK_O_FILES)
# compile c to unlinked object
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -I include/audio -o $(@:.o=.tmp) $<
# partial link
	@$(LD) -r -T include/audio/sf.ld $(@:.o=.tmp) -o $(@:.o=.tmp2)
# patch defined symbols to be ABS symbols so that they remain file-relative offsets forever
	@$(ELFPATCH) $(@:.o=.tmp2) $(@:.o=.tmp2)
# write start and size symbols afterwards (TODO: source name shouldn't have to be the symbolic name..)
# TODO have sfc write a SoundfontX.name file containing the symbolic name and then cat that file here?
	@$(OBJCOPY) --add-symbol $(@F:.o=_Start)=.rodata:0,global --redefine-sym __LEN__=$(@F:.o=_Size) $(@:.o=.tmp2) $@
# cleanup temp files
	@$(RM) $(@:.o=.tmp) $(@:.o=.tmp2)
# TESTING: link with samplebanks and dump binary
#	$(LD) $(foreach f,$(SAMPLEBANK_O_FILES),-R $f) -T include/audio/sf.ld $@ -o $(@:.o=.elf)
#	$(OBJCOPY) -O binary -j.rodata $(@:.o=.elf) $(@:.o=.bin)
#	@(cmp $(@:.o=.bin) $(patsubst build/assets/audio/soundfonts/%,baserom/audiobank_files/%,$(@:.o=.bin)) && echo "$(<F) OK" || (mkdir -p NONMATCHINGS/soundfonts && cp $(@:.o=.bin) NONMATCHINGS/soundfonts/$(@F:.o=.bin)))

# then assemble the sequences... TODO would be nicer if these could depend only on the headers they contain instead
# of all soundfont headers, cpp can do dependency generation so look into using that + move $(SOUNDFONT_HEADERS) to order-only?

build/assets/audio/sequences/%.o: assets/audio/sequences/%.seq $(SOUNDFONT_HEADERS) include/audio/aseq.h include/audio/sequence_ids.h include/tables/sequence_table.h
	$(SEQ_CPP) $(SEQ_CPPFLAGS) $< -o $(@:.o=.S)
	$(AS) $(ASFLAGS) -I build/assets/audio/soundfonts -I include/audio $(@:.o=.S) -o $@
# TESTING:
#	$(OBJCOPY) -O binary -j.data $@ $(@:.o=.aseq)
#	@(cmp $(@:.o=.aseq) $(patsubst build/assets/audio/sequences/%,baserom/audioseq_files/%,$(@:.o=.aseq)) && echo "$(<F) OK" || (mkdir -p NONMATCHINGS/sequences && cp $(@:.o=.aseq) NONMATCHINGS/sequences/$(@F:.o=.aseq)))

# put together the tables

build/assets/audio/samplebank_table.h: $(SAMPLEBANK_XMLS)
# TODO switch from dir to listing the files? Should do this to allow samplebank xmls to be sourced from several dirs
	$(ATBLGEN) -banks $@ assets/audio/samplebanks

build/assets/audio/soundfont_table.h: $(SOUNDFONT_XMLS)
# TODO switch from dir to listing the files? Should do this to allow soundfont xmls to be sourced from several dirs
	$(ATBLGEN) -fonts $@ assets/audio/soundfonts

SEQ_ORDER_DEFS := -DDEFINE_SEQUENCE_PTR\(name,seqId,_2,_3,_4,_5\)=PTR\(name,seqId\) \
                  -DDEFINE_SEQUENCE\(name,seqId,_2,_3,_4,_5\)=\(name,seqId\)
build/include/tables/sequence_order.in: $(SEQUENCE_TABLE)
	$(CPP) $(CPPFLAGS) $< $(SEQ_ORDER_DEFS) -o $@

build/assets/audio/sequence_font_table.s: build/include/tables/sequence_order.in $(SEQUENCE_O_FILES)
# TODO switch from dir to listing the files in sequence table order? Should do this to allow sequence xmls to be sourced from several dirs
# would need some kind of validation that the files in the list actually exist.. (but we can do this in atblgen?)
# the real problem is the sequence table only lists names, and names != filenames in general, we can add filename to the table but seems dumb
	$(ATBLGEN) -sequences $@ build/include/tables/sequence_order.in build/assets/audio/sequences

# build the tables into objects, move data -> rodata

build/src/audio/tables/samplebank_table.o: src/audio/tables/samplebank_table.c build/assets/audio/samplebank_table.h
	$(CC_CHECK) -I build -I build/assets $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -I build -I build/assets -o $(@:.o=.tmp) $<
	@$(LD) -r -T include/audio/atbl_rdata.ld $(@:.o=.tmp) -o $@
	@$(RM) $(@:.o=.tmp)
	$(RM_MDEBUG)
# TESTING:
#	$(OBJCOPY) -O binary -j.rodata $@ $(@:.o=.bin)
# Can't compare this due to pointers

build/src/audio/tables/soundfont_table.o: src/audio/tables/soundfont_table.c build/assets/audio/soundfont_table.h $(SOUNDFONT_HEADERS)
	$(CC_CHECK) -I build -I build/assets $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -I build -I build/assets -o $(@:.o=.tmp) $<
	@$(LD) -r -T include/audio/atbl_rdata.ld $(@:.o=.tmp) -o $@
	@$(RM) $(@:.o=.tmp)
	$(RM_MDEBUG)
# TESTING:
#	$(OBJCOPY) -O binary -j.rodata $@ $(@:.o=.bin)
# Can't compare this due to pointers

STRIP := $(MIPS_BINUTILS_PREFIX)strip

build/src/audio/tables/sequence_table.o: src/audio/tables/sequence_table.c $(SEQUENCE_TABLE)
	$(CC_CHECK) -I include/tables $<
	$(CC) -c $(CFLAGS) $(MIPS_VERSION) $(OPTFLAGS) -I include/tables -o $(@:.o=.tmp) $<
	@$(LD) -r -T include/audio/atbl_rdata.ld $(@:.o=.tmp) -o $@
	@$(RM) $(@:.o=.tmp)
	$(RM_MDEBUG)
# TESTING:
#	$(OBJCOPY) -O binary -j.rodata $@ $(@:.o=.bin)
# Can't compare this due to pointers

build/assets/audio/sequence_font_table.o: build/assets/audio/sequence_font_table.s
	$(AS) $(ASFLAGS) $< -o $@
# TESTING:
#	$(OBJCOPY) -O binary -j.rodata $@ $(@:.o=.bin)
#	@cmp $(@:.o=.bin) baserom/audio_code_tables/sequence_font_table.bin && echo "$(@F:.o=) OK"

# make headers with file sizes and amounts

build/src/audio/session_config.o: build/assets/audio/soundfont_sizes.h build/assets/audio/sequence_sizes.h

build/assets/audio/soundfont_sizes.h: $(SOUNDFONT_O_FILES)
# TODO switch from dir to listing the files? Should do this to allow soundfont xmls to be sourced from several dirs
	$(AFILE_SIZES) $@ build/assets/audio/soundfonts NUM_SOUNDFONTS SOUNDFONT_SIZES

build/assets/audio/sequence_sizes.h: $(SEQUENCE_O_FILES)
# TODO switch from dir to listing the files? Should do this to allow soundfont xmls to be sourced from several dirs
	$(AFILE_SIZES) $@ build/assets/audio/sequences NUM_SEQUENCES SEQUENCE_SIZES

-include $(DEP_FILES)

# Print target for debugging
print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true
