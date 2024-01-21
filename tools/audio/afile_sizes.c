#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "elf32.h"
#include "util.h"

static int
usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <header_out.h> <object files directory> <num_def> <header_guard>\n", progname);
    return EXIT_FAILURE;
}

static bool
is_ofile(const char *path)
{
    size_t len = strlen(path);

    if (len < 2)
        return false;
    if (path[len - 2] == '.' && tolower(path[len - 1]) == 'o')
        return true;

    return false;
}

struct afile_udata {
    FILE *out;
    size_t total_num;
};

static void
file_handler(const char *path, void *udata)
{
    struct afile_udata *audata = (struct afile_udata *)udata;

    if (!is_ofile(path))
        return;

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

    char *seq_name = NULL;
    size_t seq_size = 0;

    Elf32_Sym *sym = GET_PTR(data, symtab->sh_offset);
    Elf32_Sym *sym_end = GET_PTR(data, symtab->sh_offset + symtab->sh_size);

    for (size_t i = 0; sym < sym_end; sym++, i++) {
        validate_read(symtab->sh_offset + i * sizeof(Elf32_Sym), sizeof(Elf32_Sym), data_size);

        // The start and size symbol should be defined and global
        int bind = sym->st_info >> 4;
        if (bind != SB_GLOBAL || sym->st_shndx == SHN_UND) {
            continue;
        }

        const char *sym_name = elf32_get_string(sym->st_name, strtab, data, data_size);
        size_t name_len = strlen(sym_name);

        if (name_len > 5 && strcmp(&sym_name[name_len - 5], "_Size") == 0) {
            if (sym->st_shndx != SHN_ABS)
                continue;

            seq_size = sym->st_value;

            if (seq_name != NULL)
                break;

            continue;
        }

        if (name_len > 6 && strcmp(&sym_name[name_len - 6], "_Start") == 0) {
            seq_name = (char *)sym_name;
            seq_name[name_len - 6] = '\0';

            if (seq_size != 0)
                break;

            continue;
        }
    }

    if (seq_name == NULL)
        error("No start symbol?");

    fprintf(audata->out, "#define %s_SIZE 0x%lX\n", seq_name, seq_size);
    audata->total_num++;

    free(data);
}

int
main(int argc, char **argv)
{
    const char *progname = argv[0];

    if (argc != 5)
        return usage(progname);

    const char *header_out = argv[1];
    const char *dir_root = argv[2];
    const char *num_def = argv[3];
    const char *header_guard = argv[4];

    FILE *out = fopen(header_out, "w");
    if (out == NULL)
        error("Could not open output file \"%s\" for writing", header_out);

    fprintf(out,
            // clang-format off
           "#ifndef %s_H_"      "\n"
           "#define %s_H_"      "\n"
                                "\n",
            // clang-format on
            header_guard, header_guard);

    struct afile_udata data = { out, 0 };
    dir_walk_rec(dir_root, file_handler, &data);

    fprintf(out,
            // clang-format off
                                "\n"
           "#define %s %lu"     "\n"
                                "\n"
           "#endif"             "\n",
            // clang-format on
            num_def, data.total_num);
    fclose(out);

    return EXIT_SUCCESS;
}
