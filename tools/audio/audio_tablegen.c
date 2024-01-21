#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "samplebank.h"
#include "soundfont.h"
#include "elf32.h"
#include "xml.h"
#include "util.h"

/*
audio_tablegen
    Generate sample bank, soundfont and sequence font tables

    Samplebank mode:
        -banks -o [header_out] [dir]

            Read in all supplied xml
            Ensure they all have unique Index, including Pointer fields
            Sort them by Index
            Output header including Pointer

    Soundfont mode:
        -fonts -o [header_out] [dir]

            Read in all supplied xml
            Ensure they all have unique Index
            Sort them by Index
            Read the samplebank xmls pointed to by the soundfont xmls to get their index
            Output header

    Sequence mode:
        -sequences -o [seq_font_tbl_out] [order] [dir]

            Read in the sequence order
            Read in all sequence elf files
            Ensure there is an elf for each sequence name by checking elf symbols against sequence order
            Read the .fonts sections of the elf files for the font table
            Output sequence font table
*/

struct samplebank_file_info {
    samplebank *samplebanks;
    size_t num_files;
};

struct soundfont_file_info {
    struct soundfont_file_info *next;
    soundfont *soundfont;
    int normal_bank_index;
    int dd_bank_index;
};

struct sequences_file_info {
    struct seqdata *file_data;
    size_t num_files;
};

struct seqdata {
    char *elf_path;
    char *name;
    uint32_t font_section_offset;
    size_t font_section_size;
};

static bool
is_xml(const char *path)
{
    size_t len = strlen(path);

    if (len < 4)
        return false;
    if (path[len - 4] == '.' && tolower(path[len - 3]) == 'x' && tolower(path[len - 2]) == 'm' &&
        tolower(path[len - 1]) == 'l')
        return true;

    return false;
}

static bool
is_ofile(const char *path)
{
    size_t len = strlen(path);

    if (len < 2)
        return false;
    if (path[len - 2] == '.' && tolower(path[len - 1]) == 'o')
        return true;

    if (len < 4)
        return false;
    if (path[len - 4] == '.' && tolower(path[len - 3]) == 'e' && tolower(path[len - 2]) == 'l' &&
        tolower(path[len - 1]) == 'f')
        return true;

    return false;
}

static void
read_sequence_elf_file(const char *path, void *udata)
{
    if (!is_ofile(path))
        return;

    // printf("\"%s\"\n", path);

    // Open ELF file

    size_t data_size;
    void *data = elf32_read(path, &data_size);

    Elf32_Shdr *symtab = elf32_get_symtab(data, data_size);
    if (symtab == NULL)
        error("No symtab?");
    Elf32_Shdr *shstrtab = elf32_get_shstrtab(data, data_size);
    if (shstrtab == NULL)
        error("No shstrtab?");
    Elf32_Shdr *strtab = elf32_get_strtab(data, data_size);
    if (symtab == NULL)
        error("No strtab?");

    Elf32_Shdr *font_section = elf32_section_forname(".fonts", shstrtab, data, data_size);
    if (font_section == NULL)
        error("No fonts defined?");

    /*
    uint8_t *font_data = GET_PTR(data, font_section->sh_offset);
    size_t num_fonts = font_section->sh_size;

    printf("[ ");
    for (size_t i = 0; i < num_fonts; i++) {
        printf("0x%02X ", font_data[i]);
    }
    printf("]\n");
    */

    // Find the <name>_Start symbol

    Elf32_Sym *sym = GET_PTR(data, symtab->sh_offset);
    Elf32_Sym *sym_end = GET_PTR(data, symtab->sh_offset + symtab->sh_size);

    char *seq_name = NULL;

    for (size_t i = 0; sym < sym_end; sym++, i++) {
        validate_read(symtab->sh_offset + i * sizeof(Elf32_Sym), sizeof(Elf32_Sym), data_size);

        // The start symbol should be defined and global
        int bind = sym->st_info >> 4;
        if (bind != SB_GLOBAL || sym->st_shndx == SHN_UND) {
            continue;
        }

        const char *sym_name = elf32_get_string(sym->st_name, strtab, data, data_size);
        size_t name_len = strlen(sym_name);

        if (name_len < 6) // _Start
            continue;
        if (sym_name[name_len - 6] != '_' || sym_name[name_len - 5] != 'S' || sym_name[name_len - 4] != 't' ||
            sym_name[name_len - 3] != 'a' || sym_name[name_len - 2] != 'r' || sym_name[name_len - 1] != 't')
            continue;

        // Got start symbol
        /*
        printf("SYM [%d, %d, %d, %d, %d, %d] %s\n",
               sym->st_value,       // value
               sym->st_size,        // size
               sym->st_info >> 4,   // bind
               sym->st_info & 0xF,  // type
               sym->st_other,       // ignore
               sym->st_shndx,       // section index
               sym_name);
        */

        // Save sequence name
        seq_name = malloc(name_len - 6 + 1);
        strncpy(seq_name, sym_name, name_len - 6);
        seq_name[name_len - 6] = '\0';

        /* printf("%s\n", seq_name); */
        break;
    }

    if (seq_name == NULL)
        error("No start symbol?");

    struct sequences_file_info *file_info = (struct sequences_file_info *)udata;

    size_t n = file_info->num_files;

    // TODO double alloc size for fast
    file_info->num_files++;
    file_info->file_data = realloc(file_info->file_data, file_info->num_files * sizeof(struct seqdata));

    // Populate new data
    struct seqdata *seqdata = &file_info->file_data[n];
    seqdata->elf_path = strdup(path);
    seqdata->name = seq_name;
    seqdata->font_section_offset = font_section->sh_offset;
    seqdata->font_section_size = font_section->sh_size;

    free(data);
}

static void
read_samplebank_xml_file(const char *path, UNUSED void *udata)
{
    if (!is_xml(path))
        return;

    // printf("\"%s\"\n", path);

    // open xml
    xmlDocPtr document = xmlReadFile(path, NULL, XML_PARSE_NONET);
    if (document == NULL)
        error("Could not read xml file \"%s\"\n", path);

    struct samplebank_file_info *finfo = (struct samplebank_file_info *)udata;

    size_t n = finfo->num_files;
    finfo->num_files++;
    finfo->samplebanks = realloc(finfo->samplebanks, finfo->num_files * sizeof(samplebank));

    samplebank *sb = &finfo->samplebanks[n];

    // parse xml
    read_samplebank_xml(sb, document);
}

static int
validate_samplebank_index(samplebank *sb, int ptr_idx)
{
    if (ptr_idx != -1) {
        // Validate pointer index
        bool found = false;

        for (size_t i = 0; i < sb->num_pointers; i++) {
            if (ptr_idx == sb->pointer_indices[i]) {
                found = true;
                break;
            }
        }
        if (!found)
            warning("Invalid pointer indirect: %d for samplebank %s\n", ptr_idx, sb->name);

        return ptr_idx;
    } else {
        return sb->index;
    }
}

static void
read_soundfont_xml_file(const char *path, UNUSED void *udata)
{
    if (!is_xml(path))
        return;

    // printf("\"%s\"\n", path);

    xmlDocPtr document = xmlReadFile(path, NULL, XML_PARSE_NONET);
    if (document == NULL)
        error("Could not read xml file \"%s\"\n", path);

    xmlNodePtr root = xmlDocGetRootElement(document);
    if (!strequ(XMLSTR_TO_STR(root->name), "Soundfont"))
        error("Root node must be <Soundfont>");

    soundfont *sf = malloc(sizeof(soundfont));

    read_soundfont_info(sf, root);

    // printf("Name: %s\n", sf->info.name);
    // printf("Medium: %s\n", sf->info.medium);
    // printf("CachePolicy: %s\n", sf->info.cache_policy);
    // printf("SampleBankNormal (Path): %s\n", sf->info.bank_path);
    // printf("Pointer index: %d\n", sf->info.pointer_index);
    // printf("SampleBankDD (Path): %s\n", sf->info.bank_path_dd);

    // Resolve samplebank indices

    int normal_idx = validate_samplebank_index(&sf->sb, sf->info.pointer_index);

    int dd_idx = 255;
    if (sf->info.bank_path_dd != NULL)
        dd_idx = validate_samplebank_index(&sf->sbdd, sf->info.pointer_index); // TODO pointer_index_dd

    // Build a linked list, insertion-sort based on sf->info.index, error on duplicates

    struct soundfont_file_info *finfo = (struct soundfont_file_info *)udata;

    struct soundfont_file_info *link = malloc(sizeof(struct soundfont_file_info));
    link->soundfont = sf;
    link->normal_bank_index = normal_idx;
    link->dd_bank_index = dd_idx;

    if (finfo->next == NULL) {
        // First
        link->next = NULL;
        finfo->next = link;
    } else {
        struct soundfont_file_info *finfo_prev = finfo;
        finfo = finfo->next;

        while (finfo != NULL) {
            if (finfo->soundfont->info.index == sf->info.index) {
                error("Duplicate soundfont indices");
            }
            if (finfo->soundfont->info.index > sf->info.index) {
                break;
            }
            finfo_prev = finfo;
            finfo = finfo->next;
        }

        // either reached the end or at the sort position, insert new link
        finfo_prev->next = link;
        link->next = finfo;
    }
}

struct seq_order {
    size_t num_sequences;
    char **names;
    char **enum_names;
    bool *ptr_flags;
    void *buf_ptr;
};

void
read_seq_order(struct seq_order *order, const char *path)
{
    // Read from file, we assume the file has been preprocessed with cpp so that whitespace is collapsed and each line
    // has the form:
    // (name,enum_name) or PTR(name,enum_name)
    UNUSED size_t data_size;
    char *filedata = util_read_whole_file(path, &data_size);

    // We expect one per line

    size_t total_size = 0;

    for (char *p = filedata; *p != '\0'; p++) {
        if (*p == '\n') {
            total_size++;
        } else if (isspace(*p)) {
            goto malformed;
        }
    }

    char **name_list = malloc(total_size * sizeof(char *));
    char **enum_name_list = malloc(total_size * sizeof(char *));
    bool *ptr_flags = malloc(total_size * sizeof(bool));

    char *startp = filedata;

    size_t i = 0;
    bool newline = true;
    bool eol = false;
    bool gotname = false;
    for (char *p = filedata; *p != '\0'; p++) {
        // printf("%c (%d, %d, %d)\n", *p, newline, eol, gotname);

        if (newline) {
            bool isptr = false;
            // Started a new line, look for either an opening '(' or 'PTR(', anything else is invalid.
            if (*p == 'P') {
                // Should be "PTR"
                p++;
                if (*p != 'T')
                    goto malformed;
                p++;
                if (*p != 'R')
                    goto malformed;
                p++;
                isptr = true;
            }

            // Record whether it was a pointer or not
            ptr_flags[i] = isptr;

            // Should always have '(' here
            if (*p != '(')
                goto malformed;

            // End of newline, start name
            newline = false;
            gotname = false;
            startp = p + 1;
            continue;
        }

        if (eol) {
            // At end of line, only whitespace is permitted
            if (!isspace(*p))
                goto malformed;

            // When we find the new line, return to newline state
            if (*p == '\n') {
                newline = true;
                eol = false;
            }
            continue;
        }

        // Deal with middle of the line, of the form: "<name>,<enum_name>)"

        // Whitespace is prohibited
        if (isspace(*p))
            goto malformed;

        if (!gotname) {
            // Haven't found name yet, wait for comma
            if (*p == ',') {
                // Record start pos
                name_list[i] = startp;
                // Null-terminate (replaces ',')
                *p = '\0';
                // Set new start pos for enum_name
                startp = p + 1;
                gotname = true;
            }
        } else {
            // Have found name, now we want enum_name
            if (*p == ')') {
                // Record start pos
                enum_name_list[i] = startp;
                // Null-terminate (replaces ')')
                *p = '\0';
                // Enum name is the last thing we care about on this line, advance index
                i++;
                // Continue to eol handling
                eol = true;
            }
        }
    }
    // We should have found one entry for each line
    assert(i == total_size);

    // Write results
    order->num_sequences = total_size;
    order->names = name_list;
    order->enum_names = enum_name_list;
    order->ptr_flags = ptr_flags;
    order->buf_ptr = filedata; // so we can free it
    return;
malformed:
    error("Malformed sequence_order.in?");
}

int
tablegen_samplebanks(const char *sb_hdr_out, const char *samplebanks_path)
{
    struct samplebank_file_info finfo;
    finfo.samplebanks = NULL;
    finfo.num_files = 0;

    dir_walk_rec(samplebanks_path, read_samplebank_xml_file, &finfo);

#if 0
    for (size_t i = 0; i < finfo.num_files; i++) {
        samplebank *sb = &finfo.samplebanks[i];

        // debug print stuff

        printf("    Name: \"%s\"\n", sb->name);
        printf("    Index: %d\n", sb->index);
        printf("    Medium: \"%s\"\n", sb->medium);
        printf("    CachePolicy: \"%s\"\n", sb->cache_policy);
        printf("    BufferBug: %d\n", sb->buffer_bug);

        printf("    NumSamples: %lu\n", sb->num_samples);
        for (size_t i = 0; i < sb->num_samples; i++) {
            printf("      \"%s\" : \"%s\" : %d\n", sb->sample_paths[i], sb->sample_names[i], sb->is_sample[i]);
        }

        printf("    NumPointers: %lu\n", sb->num_pointers);
        printf("      Pointers: [ ");
        for (size_t i = 0; i < sb->num_pointers; i++) {
            printf("%d ", sb->pointer_indices[i]);
        }
        printf("]\n");
    }
#endif

    // find largest index

    size_t indices_max = 0;

    for (size_t i = 0; i < finfo.num_files; i++) {
        samplebank *sb = &finfo.samplebanks[i];
        unsigned index = sb->index;
        if (index > indices_max)
            indices_max = index;

        for (size_t j = 0; j < sb->num_pointers; j++) {
            index = sb->pointer_indices[j];
            if (index > indices_max)
                indices_max = index;
        }
    }

    size_t indices_len = indices_max + 1;

    // classify indices, sort cache policies, check that no two indices are the same

#define INDEX_NONE      0
#define INDEX_NOPOINTER 1
#define INDEX_POINTER   2

    // TODO coalesce mallocs into one big allocation

    char *indices_classes = malloc(indices_len);
    memset(indices_classes, INDEX_NONE, indices_len);

    const char **names = malloc(indices_len * sizeof(const char *));
    unsigned *ptr_indices = malloc(indices_len * sizeof(unsigned));

    const char **media = malloc(indices_len * sizeof(const char *));
    const char **cache_policies = malloc(indices_len * sizeof(const char *));

    for (size_t i = 0; i < finfo.num_files; i++) {
        samplebank *sb = &finfo.samplebanks[i];
        unsigned index = sb->index;

        if (indices_classes[index] != INDEX_NONE)
            error("Overlapping indices");
        indices_classes[index] = INDEX_NOPOINTER;

        names[index] = sb->name;
        media[index] = sb->medium;
        cache_policies[index] = sb->cache_policy;

        unsigned real_index = index;

        for (size_t j = 0; j < sb->num_pointers; j++) {
            index = sb->pointer_indices[j];

            if (indices_classes[index] != INDEX_NONE)
                error("Overlapping indices");
            indices_classes[index] = INDEX_POINTER;

            ptr_indices[index] = real_index;
            media[index] = sb->medium;
            cache_policies[index] = sb->cache_policy;
        }
    }

    // check that we have no gaps

    for (size_t i = 0; i < indices_len; i++) {
        if (indices_classes[i] == INDEX_NONE) {
            error("Missing index");
        }
    }

    // emit table

    FILE *out = fopen(sb_hdr_out, "w");
    if (out == NULL)
        error("Failed to open samplebank header output");

    fprintf(out,
            // clang-format off
           "/**"                                                    "\n"
           " * DEFINE_SAMPLE_BANK(name, medium, cachePolicy)"       "\n"
           " * DEFINE_SAMPLE_BANK_PTR(name, medium, cachePolicy)"   "\n"
           " */"                                                    "\n"
            // clang-format on
    );

    for (size_t i = 0; i < indices_len; i++) {
        if (indices_classes[i] == INDEX_NOPOINTER) {
            fprintf(out, "DEFINE_SAMPLE_BANK    (%s, %s, %s)\n", names[i], media[i], cache_policies[i]);
        } else if (indices_classes[i] == INDEX_POINTER) {
            fprintf(out, "DEFINE_SAMPLE_BANK_PTR(%u, %s, %s)\n", ptr_indices[i], media[i], cache_policies[i]);
        } else {
            assert(false);
        }
    }

    fclose(out);

    free(indices_classes);

    return EXIT_SUCCESS;
}

int
tablegen_soundfonts(const char *sf_hdr_out, const char *soundfonts_path)
{
    struct soundfont_file_info finfo_base;
    finfo_base.next = NULL;
    finfo_base.soundfont = NULL;

    dir_walk_rec(soundfonts_path, read_soundfont_xml_file, &finfo_base);

    FILE *out = fopen(sf_hdr_out, "w");

    fprintf(out,
            // clang-format off
           "/**"                                                                    "\n"
           " * DEFINE_SOUNDFONT(name, medium, cachePolicy, sampleBankNormal, "
                               "sampleBankDD, nInstruments, nDrums, nSfx)"          "\n"
           " */"                                                                    "\n"
            // clang-format on
    );

    for (struct soundfont_file_info *finfo = finfo_base.next; finfo != NULL; finfo = finfo->next) {
        soundfont *sf = finfo->soundfont;

        // printf("%s\n", sf->info.name);
        // printf("%s\n", sf->info.medium);
        // printf("%s\n", sf->info.cache_policy);
        // printf("%d\n", finfo->normal_bank_index);
        // printf("%d\n", finfo->dd_bank_index);
        // printf("%d\n", sf->num_instruments);
        // printf("%d\n", sf->num_drums);
        // printf("%d\n", sf->num_effects);

        // fprintf(out, "DEFINE_SOUNDFONT(%s, %s, %s, %d, %d, %d, %d, %d)\n",
        //         sf->info.name, sf->info.medium, sf->info.cache_policy,
        //         finfo->normal_bank_index, finfo->dd_bank_index, sf->num_instruments, sf->num_drums, sf->num_effects);

        fprintf(out, "DEFINE_SOUNDFONT(%s, %s, %s, %d, %d, SF%d_NUM_INSTRUMENTS, SF%d_NUM_DRUMS, SF%d_NUM_SFX)\n",
                sf->info.name, sf->info.medium, sf->info.cache_policy, finfo->normal_bank_index, finfo->dd_bank_index,
                sf->info.index, sf->info.index, sf->info.index);
    }

    return EXIT_SUCCESS;
}

static void
gen_sequence_font_table(FILE *out, struct seqdata **sequences, size_t num_sequences)
{
    fprintf(out,
            // clang-format off
                                            "\n"
           ".section .rodata"               "\n"
                                            "\n"
           ".global gSequenceFontTable"     "\n"
           "gSequenceFontTable:"            "\n"
            // clang-format on
    );

    for (size_t i = 0; i < num_sequences; i++) {
        fprintf(out, "    .half Fonts_%lu - gSequenceFontTable\n", i);
    }
    fprintf(out, "\n");

    for (size_t i = 0; i < num_sequences; i++) {
        fprintf(out,
                // clang-format off
               "Fonts_%lu:"                         "\n"
               "    .byte %ld"                      "\n"
               "    .incbin \"%s\", 0x%X, %lu"      "\n"
                                                    "\n",
                // clang-format on
                i, sequences[i]->font_section_size, sequences[i]->elf_path, sequences[i]->font_section_offset,
                sequences[i]->font_section_size);
    }
    fprintf(out, ".balign 16\n");
}

int
tablegen_sequences(const char *seq_font_tbl_out, const char *sequences_path, const char *seq_order_path)
{
    struct seq_order order;
    read_seq_order(&order, seq_order_path);

    // for (size_t i = 0; i < order.num_sequences; i++) {
    //     printf("\"%s\" \"%s\" %d\n", order.names[i], order.enum_names[i], order.ptr_flags[i]);
    // }

    struct sequences_file_info seq_info;
    seq_info.file_data = NULL;
    seq_info.num_files = 0;

    dir_walk_rec(sequences_path, read_sequence_elf_file, &seq_info);

#if 0
    printf(                 "\n");
    printf("Result:"        "\n");
    printf("Num files: %lu" "\n", seq_info.num_files);
    printf(                 "\n");

    for (size_t i = 0; i < seq_info.num_files; i++) {
        struct seqdata *seqdata = &seq_info.file_data[i];

        printf("    elf path    : \"%s\""   "\n", seqdata->elf_path);
        printf("    name        : \"%s\""   "\n", seqdata->name);
        printf("    font offset : 0x%X"     "\n", seqdata->font_section_offset);
        printf("    num fonts   : %lu"      "\n", seqdata->font_section_size);
        printf(                             "\n");
    }
#endif

    // link against seq order

    struct seqdata **final_seqdata = malloc(order.num_sequences * sizeof(struct seqdata));

    for (size_t i = 0; i < order.num_sequences; i++) {
        final_seqdata[i] = NULL;
    }

    for (size_t i = 0; i < order.num_sequences; i++) {
        // Skip pointers for now
        if (order.ptr_flags[i])
            continue;

        // If it's not a pointer, "name" is the name as it appears in a sequence file, find it in the list of sequences
        char *name = order.names[i];

        for (size_t j = 0; j < seq_info.num_files; j++) {
            struct seqdata *seqdata = &seq_info.file_data[j];

            if (strequ(name, seqdata->name)) {
                // Found name, done
                final_seqdata[i] = seqdata;
                break;
            }
        }
    }

    for (size_t i = 0; i < order.num_sequences; i++) {
        // Now we only care about pointers
        if (!order.ptr_flags[i])
            continue;

        // If it's a pointer, "name" is the ENUM name of the REAL sequence and "enum_name" is irrel
        char *name = order.names[i];

        for (size_t j = 0; j < order.num_sequences; j++) {
            // Skip pointers
            if (order.ptr_flags[j])
                continue;

            if (strequ(name, order.enum_names[j])) {
                // For pointers, we just duplicate the fonts for the original into the pointer entry.
                // TODO ideally we would allow fonts to be different when a sequence is accessed by pointer, but how
                // to supply this info?
                final_seqdata[i] = final_seqdata[j];
                break;
            }
        }
    }

    for (size_t i = 0; i < order.num_sequences; i++) {
        if (final_seqdata[i] == NULL)
            error("Could not find object file for sequence %lu : %s", i, order.names[i]);
    }

    FILE *out = fopen(seq_font_tbl_out, "w");
    if (out == NULL)
        error("Failed to open output file");

    gen_sequence_font_table(out, final_seqdata, order.num_sequences);

    fclose(out);

    // Free all

    for (size_t i = 0; i < seq_info.num_files; i++) {
        struct seqdata *seqdata = &seq_info.file_data[i];

        free(seqdata->elf_path);
        free(seqdata->name);
    }
    free(seq_info.file_data);

    free(order.buf_ptr);
    free(order.names);
    free(order.enum_names);
    free(order.ptr_flags);

    return EXIT_SUCCESS;
}

static int
usage(const char *progname)
{
    fprintf(stderr,
            // clang-format off
           "Usage:"                                                                             "\n"
           "    %s -banks     <samplebank_table.h> <samplebank xml directory>"                  "\n"
           "    %s -fonts     <soundfont_table.h> <soundfont xml directory>"                    "\n"
           "    %s -sequences <seq_font_table.s> <sequence_order.in> <sequence elf directory>"  "\n",
            // clang-format on
            progname, progname, progname);
    return EXIT_FAILURE;
}

int
main(int argc, char **argv)
{
    // -banks -o [header_out] [xml dir]
    // -fonts -o [header_out] [xml dir]
    // -sequences -o [seq_font_tbl_out] [order] [ofile dir]
    int ret = EXIT_SUCCESS;

    const char *progname = argv[0];

    if (argc < 2)
        return usage(progname);

    const char *mode = argv[1];

    if (strequ(mode, "-banks")) {
        if (argc != 4)
            return usage(progname);

        const char *sb_hdr_out = argv[2];       // build/include/tables/oot/samplebank_table.h
        const char *samplebanks_path = argv[3]; // assets/samplebanks/oot/

        ret = tablegen_samplebanks(sb_hdr_out, samplebanks_path);
    } else if (strequ(mode, "-fonts")) {
        if (argc != 4)
            return usage(progname);

        const char *sf_hdr_out = argv[2];      // build/include/tables/oot/soundfont_table.h
        const char *soundfonts_path = argv[3]; // assets/soundfonts/oot/

        ret = tablegen_soundfonts(sf_hdr_out, soundfonts_path);
    } else if (strequ(mode, "-sequences")) {
        if (argc != 5)
            return usage(progname);

        const char *seq_font_tbl_out = argv[2]; // build/include/tables/oot/sequence_font_table.s
        const char *seq_order_path = argv[3];   // build/include/tables/sequence_order.in
        const char *sequences_path = argv[4];   // build/assets/sequences/oot/

        ret = tablegen_sequences(seq_font_tbl_out, sequences_path, seq_order_path);
    } else {
        return usage(progname);
    }
    return ret;
}
