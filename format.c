#define _DEFAULT_SOURCE

#include "format.h"

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#if defined(__linux__)
#include <sys/sysmacros.h>
#endif

static unsigned long long block_unit(const Options *options)
{
    const char *setting;
    char *end;
    unsigned long long value;
    unsigned long long multiplier = 1;

    if (options->human_readable) {
        return 1;
    }
    if (options->kilobytes) {
        return 1024;
    }
    setting = getenv("BLOCKSIZE");
    if (setting == NULL || *setting == '\0') {
        return 512;
    }

    value = strtoull(setting, &end, 10);
    if (end == setting || value == 0) {
        return 512;
    }
    if (*end != '\0') {
        switch (tolower((unsigned char)*end)) {
        case 'k':
            multiplier = 1024;
            break;
        case 'm':
            multiplier = 1024ULL * 1024ULL;
            break;
        case 'g':
            multiplier = 1024ULL * 1024ULL * 1024ULL;
            break;
        default:
            return 512;
        }
        if (end[1] != '\0') {
            return 512;
        }
    }
    if (value > ULLONG_MAX / multiplier) {
        return 512;
    }
    return value * multiplier;
}

unsigned long long block_count(const struct stat *metadata,
                              const Options *options)
{
    unsigned long long bytes;
    unsigned long long unit = block_unit(options);

    if (metadata->st_blocks < 0) {
        return 0;
    }
    /* st_blocks is measured in 512-byte units. */
    if ((unsigned long long)metadata->st_blocks > ULLONG_MAX / 512ULL) {
        return ULLONG_MAX / unit;
    }
    bytes = (unsigned long long)metadata->st_blocks * 512ULL;
    return bytes / unit + (bytes % unit != 0);
}

static void print_human_size(FILE *stream, unsigned long long size)
{
    static const char suffixes[] = "BKMGTPE";
    double value = (double)size;
    size_t suffix = 0;

    while (value >= 1024.0 && suffix < sizeof(suffixes) - 2) {
        value /= 1024.0;
        ++suffix;
    }
    if (suffix == 0) {
        fprintf(stream, "%llu", size);
    } else if (value < 10.0) {
        fprintf(stream, "%.1f%c", value, suffixes[suffix]);
    } else {
        fprintf(stream, "%.0f%c", value, suffixes[suffix]);
    }
}

static void print_name(FILE *stream, const char *name, QuoteMode mode)
{
    const char *character = name;
    size_t remaining = strlen(name);
    mbstate_t state = {0};

    if (mode == QUOTE_RAW) {
        fputs(name, stream);
        return;
    }

    while (remaining > 0) {
        wchar_t wide_character;
        size_t length = mbrtowc(&wide_character, character, remaining, &state);

        if (length == (size_t)-1 || length == (size_t)-2) {
            fputc('?', stream);
            ++character;
            --remaining;
            memset(&state, 0, sizeof(state));
            continue;
        }
        if (length == 0) {
            break;
        }
        if (iswprint(wide_character)) {
            fwrite(character, 1, length, stream);
        } else {
            fputc('?', stream);
        }
        character += length;
        remaining -= length;
    }
}

static char file_type(mode_t mode)
{
#if defined(S_ISWHT)
    if (S_ISWHT(mode)) {
        return 'w';
    }
#endif
    if (S_ISDIR(mode)) {
        return 'd';
    }
    if (S_ISLNK(mode)) {
        return 'l';
    }
    if (S_ISCHR(mode)) {
        return 'c';
    }
    if (S_ISBLK(mode)) {
        return 'b';
    }
    if (S_ISFIFO(mode)) {
        return 'p';
    }
    if (S_ISSOCK(mode)) {
        return 's';
    }
    return '-';
}

static void print_permissions(FILE *stream, mode_t mode)
{
    char permissions[10] = {
        (mode & S_IRUSR) ? 'r' : '-',
        (mode & S_IWUSR) ? 'w' : '-',
        (mode & S_IXUSR) ? 'x' : '-',
        (mode & S_IRGRP) ? 'r' : '-',
        (mode & S_IWGRP) ? 'w' : '-',
        (mode & S_IXGRP) ? 'x' : '-',
        (mode & S_IROTH) ? 'r' : '-',
        (mode & S_IWOTH) ? 'w' : '-',
        (mode & S_IXOTH) ? 'x' : '-',
        '\0'
    };

    if (mode & S_ISUID) {
        permissions[2] = (mode & S_IXUSR) ? 's' : 'S';
    }
    if (mode & S_ISGID) {
        permissions[5] = (mode & S_IXGRP) ? 's' : 'S';
    }
    if (mode & S_ISVTX) {
        permissions[8] = (mode & S_IXOTH) ? 't' : 'T';
    }
    fprintf(stream, "%c%s", file_type(mode), permissions);
}

static void print_owner(FILE *stream, uid_t owner, bool numeric)
{
    struct passwd *record = numeric ? NULL : getpwuid(owner);

    if (record != NULL) {
        fprintf(stream, "%s", record->pw_name);
    } else {
        fprintf(stream, "%ju", (uintmax_t)owner);
    }
}

static void print_group(FILE *stream, gid_t group, bool numeric)
{
    struct group *record = numeric ? NULL : getgrgid(group);

    if (record != NULL) {
        fprintf(stream, "%s", record->gr_name);
    } else {
        fprintf(stream, "%ju", (uintmax_t)group);
    }
}

static time_t entry_time(const struct stat *metadata, TimeMode mode)
{
    if (mode == TIME_CHANGED) {
        return metadata->st_ctime;
    }
    if (mode == TIME_ACCESSED) {
        return metadata->st_atime;
    }
    return metadata->st_mtime;
}

static void print_timestamp(FILE *stream, time_t timestamp)
{
    struct tm *local = localtime(&timestamp);
    time_t now = time(NULL);
    bool use_year = difftime(now, timestamp) > 180.0 * 24.0 * 60.0 * 60.0 ||
                    difftime(timestamp, now) > 60.0 * 60.0;
    char buffer[32];

    if (local == NULL ||
        strftime(buffer, sizeof(buffer),
                 use_year ? "%b %e  %Y" : "%b %e %H:%M", local) == 0) {
        fprintf(stream, "??? ?? ??:??");
        return;
    }
    fprintf(stream, "%s", buffer);
}

static void print_suffix(FILE *stream, const Entry *entry)
{
    if (!entry->has_metadata) {
        return;
    }
#if defined(S_ISWHT)
    if (S_ISWHT(entry->metadata.st_mode)) {
        fputc('%', stream);
        return;
    }
#endif
    if (S_ISDIR(entry->metadata.st_mode)) {
        fputc('/', stream);
    } else if (S_ISLNK(entry->metadata.st_mode)) {
        fputc('@', stream);
    } else if (S_ISFIFO(entry->metadata.st_mode)) {
        fputc('|', stream);
    } else if (S_ISSOCK(entry->metadata.st_mode)) {
        fputc('=', stream);
    } else if (S_ISREG(entry->metadata.st_mode) &&
               (entry->metadata.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
        fputc('*', stream);
    }
}

static int print_link_target(FILE *stream, const Entry *entry,
                             QuoteMode quote_mode)
{
    size_t capacity = 128;

    for (;;) {
        char *target = malloc(capacity + 1);
        ssize_t length;

        if (target == NULL) {
            fprintf(stderr, "ls: unable to allocate memory for link target\n");
            return -1;
        }
        length = readlink(entry->path, target, capacity);
        if (length < 0) {
            int error = errno;
            free(target);
            fprintf(stderr, "ls: %s: %s\n", entry->path, strerror(error));
            return -1;
        }
        if ((size_t)length < capacity) {
            target[length] = '\0';
            fprintf(stream, " -> ");
            print_name(stream, target, quote_mode);
            free(target);
            return 0;
        }
        free(target);
        if (capacity > SIZE_MAX / 2) {
            fprintf(stderr, "ls: symbolic link target is too long\n");
            return -1;
        }
        capacity *= 2;
    }
}

int print_entry(FILE *stream, const Entry *entry, const Options *options)
{
    if (options->show_inode) {
        if (entry->has_metadata) {
            fprintf(stream, "%ju ",
                    (uintmax_t)entry->metadata.st_ino);
        } else {
            fprintf(stream, "? ");
        }
    }
    if (options->show_blocks) {
        if (entry->has_metadata) {
            unsigned long long count =
                block_count(&entry->metadata, options);

            if (options->human_readable) {
                print_human_size(stream, count);
            } else {
                fprintf(stream, "%llu", count);
            }
            fputc(' ', stream);
        } else {
            fprintf(stream, "? ");
        }
    }
    if (options->long_format) {
        if (!entry->has_metadata) {
            fprintf(stream, "?????????? ? ? ? ?????????? ??? ?? ??:?? ");
            print_name(stream, entry->name, options->quote_mode);
            fputc('\n', stream);
            return 0;
        }

        print_permissions(stream, entry->metadata.st_mode);
        fprintf(stream, " %3ju ", (uintmax_t)entry->metadata.st_nlink);
        print_owner(stream, entry->metadata.st_uid, options->numeric_ids);
        fputc(' ', stream);
        print_group(stream, entry->metadata.st_gid, options->numeric_ids);
        fputc(' ', stream);
        if (S_ISCHR(entry->metadata.st_mode) ||
            S_ISBLK(entry->metadata.st_mode)) {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__APPLE__)
            fprintf(stream, "%3u, %3u",
                    (unsigned int)major(entry->metadata.st_rdev),
                    (unsigned int)minor(entry->metadata.st_rdev));
#else
            fprintf(stream, "%9jd", (intmax_t)entry->metadata.st_size);
#endif
        } else if (options->human_readable) {
            print_human_size(stream, (unsigned long long)entry->metadata.st_size);
        } else {
            fprintf(stream, "%9jd", (intmax_t)entry->metadata.st_size);
        }
        fputc(' ', stream);
        print_timestamp(stream, entry_time(&entry->metadata, options->time_mode));
        fputc(' ', stream);
        print_name(stream, entry->name, options->quote_mode);
        if (options->classify) {
            print_suffix(stream, entry);
        }
        if (S_ISLNK(entry->metadata.st_mode)) {
            if (print_link_target(stream, entry, options->quote_mode) != 0) {
                fputc('\n', stream);
                return -1;
            }
        }
        fputc('\n', stream);
        return 0;
    }

    print_name(stream, entry->name, options->quote_mode);
    if (options->classify) {
        print_suffix(stream, entry);
    }
    fputc('\n', stream);
    return 0;
}

void print_total(FILE *stream, const EntryList *entries, const Options *options)
{
    unsigned long long total = 0;
    size_t index;

    for (index = 0; index < entries->count; ++index) {
        if (entries->items[index].has_metadata) {
            unsigned long long count =
                block_count(&entries->items[index].metadata, options);
            total = count > ULLONG_MAX - total ? ULLONG_MAX : total + count;
        }
    }
    if (options->human_readable) {
        fprintf(stream, "total ");
        print_human_size(stream, total);
        fputc('\n', stream);
    } else {
        fprintf(stream, "total %llu\n", total);
    }
}
