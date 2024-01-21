
ifeq ($(shell which xml2-config),)
	$(error Install libxml2-dev)
endif

CLANG_FORMAT := clang-format-14
FORMAT_ARGS := -i -style=file

CC := gcc
CFLAGS := -Wall -Wextra -pedantic
OPTFLAGS := -Og -g3

.PHONY: all clean distclean format

all: afile_sizes atblgen elfpatch sbc sfc
	$(MAKE) -C z64sample

clean:
	$(RM) afile_sizes atblgen elfpatch sbc sfc

distclean: clean
	$(MAKE) -C z64sample clean-all

format:
	$(CLANG_FORMAT) $(FORMAT_ARGS) $(shell find . -maxdepth 1 -type f -name "*.[ch]")

afile_sizes: afile_sizes.c util.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $^ -o $@

atblgen: audio_tablegen.c samplebank.c soundfont.c xml.c util.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $(shell xml2-config --cflags) $^ $(shell xml2-config --libs) -o $@

elfpatch: elfpatch.c util.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $^ -o $@

sbc: samplebank_compiler.c samplebank.c aifc.c xml.c util.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $(shell xml2-config --cflags) $^ $(shell xml2-config --libs) -o $@

sfc: soundfont_compiler.c samplebank.c soundfont.c aifc.c xml.c util.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $(shell xml2-config --cflags) $^ $(shell xml2-config --libs) -o $@
