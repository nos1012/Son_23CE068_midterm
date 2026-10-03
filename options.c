#define _DEFAULT_SOURCE

#include "options.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void print_usage(const char *program_name)
{
    fprintf(stderr,
            "Usage: %s [-AacdFfhiklnqRrSstuw] [--] [file ...]\n",
            program_name);
}

int parse_options(int argc, char **argv, Options *options, int *first_operand)
{
    int index;

    options->sort_mode = SORT_NAME;
    options->show_hidden = geteuid() == 0;
    options->quote_mode = isatty(STDOUT_FILENO)
                              ? QUOTE_NONPRINTABLE
                              : QUOTE_RAW;
    options->time_mode = TIME_MODIFIED;

    for (index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        size_t option_index;

        if (strcmp(argument, "--") == 0) {
            ++index;
            break;
        }
        if (argument[0] != '-' || argument[1] == '\0') {
            break;
        }

        for (option_index = 1; argument[option_index] != '\0'; ++option_index) {
            switch (argument[option_index]) {
            case 'A':
                options->show_hidden = true;
                options->show_dot_entries = false;
                break;
            case 'a':
                options->show_hidden = true;
                options->show_dot_entries = true;
                break;
            case 'c':
                options->time_mode = TIME_CHANGED;
                break;
            case 'd':
                options->directories_as_files = true;
                options->recursive = false;
                break;
            case 'F':
                options->classify = true;
                break;
            case 'f':
                options->sort_mode = SORT_NONE;
                break;
            case 'h':
                options->human_readable = true;
                options->kilobytes = false;
                break;
            case 'i':
                options->show_inode = true;
                break;
            case 'k':
                options->kilobytes = true;
                options->human_readable = false;
                break;
            case 'l':
                options->long_format = true;
                options->numeric_ids = false;
                break;
            case 'n':
                options->long_format = true;
                options->numeric_ids = true;
                break;
            case 'q':
                options->quote_mode = QUOTE_NONPRINTABLE;
                break;
            case 'R':
                options->recursive = true;
                options->directories_as_files = false;
                break;
            case 'r':
                options->reverse = true;
                break;
            case 'S':
                options->sort_mode = SORT_SIZE;
                break;
            case 's':
                options->show_blocks = true;
                break;
            case 't':
                options->sort_mode = SORT_TIME;
                break;
            case 'u':
                options->time_mode = TIME_ACCESSED;
                break;
            case 'w':
                options->quote_mode = QUOTE_RAW;
                break;
            default:
                fprintf(stderr, "%s: illegal option -- %c\n",
                        argv[0], argument[option_index]);
                print_usage(argv[0]);
                return -1;
            }
        }
    }

    *first_operand = index;
    return 0;
}
