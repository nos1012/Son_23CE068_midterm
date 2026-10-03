#ifndef FORMAT_H
#define FORMAT_H

#include "ls.h"

#include <stdio.h>

int print_entry(FILE *stream, const Entry *entry, const Options *options);
void print_total(FILE *stream, const EntryList *entries, const Options *options);
unsigned long long block_count(const struct stat *metadata,
                              const Options *options);

#endif
