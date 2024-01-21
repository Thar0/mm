#define _GNU_SOURCE
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "util.h"

// TODO ideally we should be collecting all errors and displaying them all before exiting

NORETURN void
error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\x1b[91m"
                    "Error: "
                    "\x1b[97m");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\x1b[0m"
                    "\n");
    va_end(ap);

    exit(EXIT_FAILURE);
}

void
warning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\x1b[95m"
                    "Warning: "
                    "\x1b[97m");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\x1b[0m"
                    "\n");
    va_end(ap);
}

void *
util_read_whole_file(const char *filename, size_t *size_out)
{
    FILE *file = fopen(filename, "rb");
    void *buffer = NULL;
    size_t size;

    if (file == NULL)
        error("failed to open file '%s' for reading: %s", filename, strerror(errno));

    // get size
    fseek(file, 0, SEEK_END);
    size = ftell(file);

    // if the file is empty, return NULL buffer and 0 size
    if (size != 0) {
        // allocate buffer
        buffer = malloc(size + 1);
        if (buffer == NULL)
            error("could not allocate buffer for file '%s'", filename);

        // read file
        fseek(file, 0, SEEK_SET);
        if (fread(buffer, size, 1, file) != 1)
            error("error reading from file '%s': %s", filename, strerror(errno));

        // null-terminate the buffer (in case of text files)
        ((char *)buffer)[size] = '\0';
    }

    fclose(file);

    if (size_out != NULL)
        *size_out = size;
    return buffer;
}

void
util_write_whole_file(const char *filename, const void *data, size_t size)
{
    FILE *file = fopen(filename, "wb");

    if (file == NULL)
        error("failed to open file '%s' for writing: %s", filename, strerror(errno));

    if (fwrite(data, size, 1, file) != 1)
        error("error writing to file '%s': %s", filename, strerror(errno));

    fclose(file);
}

bool
isdir(struct dirent *dp)
{
#ifdef _DIRENT_HAVE_D_TYPE
    if (dp->d_type != DT_UNKNOWN && dp->d_type != DT_LNK) {
        return dp->d_type == DT_DIR;
    }
#endif
    struct stat sb;
    stat(dp->d_name, &sb);
    if (errno != 0) {
        error("Count not stat file \"%.*s\"", (int)sizeof(dp->d_name), dp->d_name);
    }
    return S_ISDIR(sb.st_mode);
}

char *
path_join(const char *root, const char *f)
{
    size_t l1 = strlen(root);
    size_t l2 = strlen(f);

    // allocate room for the full path
    char *fullpath = malloc(l1 + l2 + 2);

    // emplace root path
    strcpy(fullpath, root);
    // add a separator if necessary
    if (root[l1 - 1] != '/')
        fullpath[l1++] = '/';
    // emplace specific path
    strcpy(fullpath + l1, f);
    // done
    return fullpath;
}

void
dir_walk_rec(const char *root, void (*callback)(const char *, void *), void *udata)
{
    DIR *dir = opendir(root);
    struct dirent *dp;

    while ((dp = readdir(dir)) != NULL) {
        char *path = path_join(root, dp->d_name);
        // printf("\"%s\" || \"%s\" -> \"%s\"\n", root, dp->d_name, path);

        if (isdir(dp)) {
            if (dp->d_name[0] == '.' && dp->d_name[1] == '\0')
                ;
            else if (dp->d_name[0] == '.' && dp->d_name[1] == '.' && dp->d_name[2] == '\0')
                ;
            else
                dir_walk_rec(path, callback, udata);
        } else {
            if (callback != NULL)
                callback(path, udata);
        }
        free(path);
    }
    closedir(dir);
}

bool
str_is_c_identifier(const char *str)
{
    // A C language identifier must:
    // - ONLY contain [_, abc..xyz, ABC..XYZ, 0..9] (we do not support unicode or extensions like $)
    // - NOT be a keyword
    // - NOT start with a digit [0..9]

    static const char *const c_kwds[] = {
        "auto",           "break",         "case",    "char",     "const",    "continue", "default",    "do",
        "double",         "else",          "enum",    "extern",   "float",    "for",      "goto",       "if",
        "inline",         "int",           "long",    "register", "restrict", "return",   "short",      "signed",
        "sizeof",         "static",        "struct",  "switch",   "typedef",  "union",    "unsigned",   "void",
        "volatile",       "while",

        "_Alignas",       "_Alignof",      "_Atomic", "_Bool",    "_Complex", "_Generic", "_Imaginary", "_Noreturn",
        "_Static_assert", "_Thread_local",
    };

    if (str == NULL) {
        return false;
    }
    if (isdigit(str[0])) {
        // Starts with a digit, fail
        return false;
    }

    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        char c = str[i];

        bool alpha = isalpha(c);
        bool digit = isdigit(c);
        bool uscore = c == '_';

        if (!(alpha || digit || uscore)) {
            // Contains bad character, fail
            return false;
        }
    }

    for (size_t i = 0; i < ARRAY_COUNT(c_kwds); i++) {
        if (strequ(str, c_kwds[i])) {
            // Matched a C keyword, fail
            return false;
        }
    }
    return true;
}
