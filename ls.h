#ifndef LS_H
#define LS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>

typedef enum {
    SORT_NAME,
    SORT_TIME,
    SORT_SIZE,
    SORT_NONE
} SortMode;

typedef enum {
    QUOTE_AUTO,
    QUOTE_NONPRINTABLE,
    QUOTE_RAW
} QuoteMode;

typedef enum {
    TIME_MODIFIED,
    TIME_CHANGED,
    TIME_ACCESSED
} TimeMode;

typedef struct {
    bool show_hidden;
    bool show_dot_entries;
    bool classify;
    bool show_inode;
    bool long_format;
    bool numeric_ids;
    bool reverse;
    bool show_blocks;
    bool human_readable;
    bool kilobytes;
    bool recursive;
    bool directories_as_files;
    SortMode sort_mode;
    QuoteMode quote_mode;
    TimeMode time_mode;
} Options;

typedef struct {
    char *name;
    char *path;
    struct stat metadata;
    bool has_metadata;
} Entry;

typedef struct {
    Entry *items;
    size_t count;
    size_t capacity;
} EntryList;

#endif
