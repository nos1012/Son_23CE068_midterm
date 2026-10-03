#define _DEFAULT_SOURCE

#include "list.h"
#include "format.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    Entry entry;
    bool is_directory;
} Operand;

static int errors_occurred;
static const Options *active_options;

static char *duplicate_string(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, text, length);
    }
    return copy;
}

static char *join_path(const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    bool needs_separator = directory_length > 0 &&
                           directory[directory_length - 1] != '/';
    size_t extra = needs_separator ? 2 : 1;
    char *path;

    if (directory_length > SIZE_MAX - extra ||
        name_length > SIZE_MAX - directory_length - extra) {
        return NULL;
    }
    path = malloc(directory_length + name_length +
                  extra);
    if (path == NULL) {
        return NULL;
    }
    memcpy(path, directory, directory_length);
    if (needs_separator) {
        path[directory_length++] = '/';
    }
    memcpy(path + directory_length, name, name_length + 1);
    return path;
}

static void free_entry(Entry *entry)
{
    free(entry->name);
    free(entry->path);
}

static void free_entry_list(EntryList *entries)
{
    size_t index;

    for (index = 0; index < entries->count; ++index) {
        free_entry(&entries->items[index]);
    }
    free(entries->items);
    entries->items = NULL;
    entries->count = 0;
    entries->capacity = 0;
}

static int append_entry(EntryList *entries, Entry *entry)
{
    if (entries->count == entries->capacity) {
        size_t new_capacity = entries->capacity == 0 ? 16 : entries->capacity * 2;
        Entry *new_items;

        if (new_capacity < entries->capacity ||
            new_capacity > SIZE_MAX / sizeof(*entries->items)) {
            return -1;
        }
        new_items = realloc(entries->items, new_capacity * sizeof(*entries->items));
        if (new_items == NULL) {
            return -1;
        }
        entries->items = new_items;
        entries->capacity = new_capacity;
    }
    entries->items[entries->count++] = *entry;
    return 0;
}

static time_t sort_time(const struct stat *metadata)
{
    if (active_options->time_mode == TIME_CHANGED) {
        return metadata->st_ctime;
    }
    if (active_options->time_mode == TIME_ACCESSED) {
        return metadata->st_atime;
    }
    return metadata->st_mtime;
}

static int compare_entries(const void *left_pointer, const void *right_pointer)
{
    const Entry *left = left_pointer;
    const Entry *right = right_pointer;
    int result = 0;

    if (active_options->sort_mode == SORT_TIME ||
        active_options->sort_mode == SORT_SIZE) {
        /* Time and size sorts are descending; names break ties. */
        if (left->has_metadata != right->has_metadata) {
            result = left->has_metadata ? -1 : 1;
        } else if (left->has_metadata &&
                   active_options->sort_mode == SORT_TIME) {
            time_t left_time = sort_time(&left->metadata);
            time_t right_time = sort_time(&right->metadata);
            if (left_time > right_time) {
                result = -1;
            } else if (left_time < right_time) {
                result = 1;
            }
        } else if (left->has_metadata) {
            if (left->metadata.st_size > right->metadata.st_size) {
                result = -1;
            } else if (left->metadata.st_size < right->metadata.st_size) {
                result = 1;
            }
        }
    }

    if (result == 0) {
        result = strcmp(left->name, right->name);
    }
    return active_options->reverse ? -result : result;
}

static int read_directory(const char *path, EntryList *entries)
{
    DIR *directory = opendir(path);
    struct dirent *directory_entry;

    if (directory == NULL) {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        errors_occurred = 1;
        return -1;
    }

    for (;;) {
        Entry entry = {0};

        /* Clear errno so a null readdir result can be distinguished from EOF. */
        errno = 0;
        directory_entry = readdir(directory);
        if (directory_entry == NULL) {
            if (errno != 0) {
                fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
                errors_occurred = 1;
            }
            break;
        }
        if (!active_options->show_hidden && directory_entry->d_name[0] == '.') {
            continue;
        }
        if (!active_options->show_dot_entries &&
            (strcmp(directory_entry->d_name, ".") == 0 ||
             strcmp(directory_entry->d_name, "..") == 0)) {
            continue;
        }

        entry.name = duplicate_string(directory_entry->d_name);
        entry.path = join_path(path, directory_entry->d_name);
        if (entry.name == NULL || entry.path == NULL) {
            free_entry(&entry);
            fprintf(stderr, "ls: unable to allocate memory while reading %s\n", path);
            errors_occurred = 1;
            closedir(directory);
            free_entry_list(entries);
            return -1;
        }
        if (lstat(entry.path, &entry.metadata) == 0) {
            entry.has_metadata = true;
        } else {
            fprintf(stderr, "ls: %s: %s\n", entry.path, strerror(errno));
            errors_occurred = 1;
        }
        if (append_entry(entries, &entry) != 0) {
            free_entry(&entry);
            fprintf(stderr, "ls: unable to allocate memory while reading %s\n", path);
            errors_occurred = 1;
            closedir(directory);
            free_entry_list(entries);
            return -1;
        }
    }
    if (closedir(directory) != 0) {
        fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
        errors_occurred = 1;
    }

    if (active_options->sort_mode != SORT_NONE && entries->count > 1) {
        qsort(entries->items, entries->count, sizeof(*entries->items),
              compare_entries);
    }
    return 0;
}

static int list_directory(const char *path, bool show_header, bool recursive)
{
    EntryList entries = {0};
    size_t index;

    if (show_header) {
        printf("%s:\n", path);
    }
    if (read_directory(path, &entries) != 0) {
        return -1;
    }

    if (active_options->long_format ||
        (active_options->show_blocks && isatty(STDOUT_FILENO))) {
        print_total(stdout, &entries, active_options);
    }
    for (index = 0; index < entries.count; ++index) {
        if (print_entry(stdout, &entries.items[index], active_options) != 0) {
            errors_occurred = 1;
        }
    }

    if (recursive) {
        for (index = 0; index < entries.count; ++index) {
            Entry *entry = &entries.items[index];

            if (entry->has_metadata &&
                S_ISDIR(entry->metadata.st_mode) &&
                strcmp(entry->name, ".") != 0 &&
                strcmp(entry->name, "..") != 0) {
                putchar('\n');
                list_directory(entry->path, true, true);
            }
        }
    }

    free_entry_list(&entries);
    return 0;
}

static bool operand_is_directory(Operand *operand)
{
    struct stat followed_metadata;

    if (!operand->entry.has_metadata) {
        return false;
    }
    if (active_options->directories_as_files) {
        return false;
    }
    if (S_ISDIR(operand->entry.metadata.st_mode)) {
        return true;
    }
    if (S_ISLNK(operand->entry.metadata.st_mode) &&
        stat(operand->entry.path, &followed_metadata) == 0 &&
        S_ISDIR(followed_metadata.st_mode)) {
        operand->entry.metadata = followed_metadata;
        return true;
    }
    return false;
}

static int compare_operands(const void *left_pointer, const void *right_pointer)
{
    const Operand *left = left_pointer;
    const Operand *right = right_pointer;

    if (left->is_directory != right->is_directory) {
        return left->is_directory ? 1 : -1;
    }
    {
        int result = strcmp(left->entry.name, right->entry.name);
        return active_options->reverse ? -result : result;
    }
}

static void free_operands(Operand *operands, size_t count)
{
    size_t index;

    for (index = 0; index < count; ++index) {
        free_entry(&operands[index].entry);
    }
    free(operands);
}

int list_operands(int argc, char **argv, int first_operand,
                  const Options *options)
{
    Operand *operands;
    size_t count = first_operand < argc ? (size_t)(argc - first_operand) : 1;
    size_t index;
    bool show_headers = count > 1;

    active_options = options;
    errors_occurred = 0;
    operands = calloc(count, sizeof(*operands));
    if (operands == NULL) {
        fprintf(stderr, "ls: unable to allocate memory for operands\n");
        return 1;
    }

    for (index = 0; index < count; ++index) {
        const char *name = first_operand < argc ? argv[first_operand + (int)index] : ".";
        Entry *entry = &operands[index].entry;

        entry->name = duplicate_string(name);
        entry->path = duplicate_string(name);
        if (entry->name == NULL || entry->path == NULL) {
            fprintf(stderr, "ls: unable to allocate memory for operand\n");
            free_operands(operands, count);
            return 1;
        }
        if (lstat(entry->path, &entry->metadata) == 0) {
            entry->has_metadata = true;
        } else {
            fprintf(stderr, "ls: %s: %s\n", name, strerror(errno));
            errors_occurred = 1;
        }
        operands[index].is_directory = operand_is_directory(&operands[index]);
    }

    if (count > 1 && options->sort_mode != SORT_NONE) {
        qsort(operands, count, sizeof(*operands), compare_operands);
    }
    for (index = 0; index < count; ++index) {
        if (!operands[index].entry.has_metadata) {
            continue;
        }
        if (operands[index].is_directory) {
            list_directory(operands[index].entry.path, show_headers,
                           options->recursive);
            show_headers = true;
        } else {
            if (print_entry(stdout, &operands[index].entry, options) != 0) {
                errors_occurred = 1;
            }
        }
    }

    free_operands(operands, count);
    return errors_occurred ? 1 : 0;
}
