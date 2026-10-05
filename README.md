# Son_23CE068_midterm

A modular, simplified implementation of the UNIX `ls(1)` command in C, based
on the subset of behavior described in the course-provided NetBSD manual.

## Features

Supported command-line options:

| Option | Description |
| --- | --- |
| `-A` | Include hidden entries but omit `.` and `..`. |
| `-a` | Include all entries, including `.` and `..`. |
| `-c` | Use status-change time for `-t` sorting and long-format timestamps. |
| `-d` | List directory operands themselves; do not recurse. |
| `-F` | Append a marker for directories, executables, symlinks, sockets, and FIFOs. |
| `-f` | Leave directory entries unsorted. |
| `-h` | Use human-readable sizes; with `-s`, display allocated bytes. |
| `-i` | Display inode numbers. |
| `-k` | Display `-s` counts in 1024-byte units. |
| `-l` | Display permissions, link count, owner, group, size, time, and name. |
| `-n` | Use numeric user and group IDs in long format. |
| `-q` | Replace non-printable filename characters with `?`. |
| `-R` | Recursively list subdirectories. |
| `-r` | Reverse the selected sort order. |
| `-S` | Sort by size, largest first. |
| `-s` | Display allocated filesystem block counts. |
| `-t` | Sort by the selected file time, newest first. |
| `-u` | Use access time for `-t` sorting and long-format timestamps. |
| `-w` | Print filename characters without replacing non-printable characters. |

Options can be combined. The last option in each pair `-h`/`-k`, `-l`/`-n`,
`-q`/`-w`, `-c`/`-u`, and `-R`/`-d` takes precedence. Multiple operands are
displayed with non-directories first. Errors are reported to standard error
and produce a non-zero exit status.

This is a coursework implementation of the specified subset, not a replacement
for every platform-specific feature or output-layout detail of the system
`ls`.

## Build

Requires a C compiler and `make` on a POSIX-compatible system.

```sh
make
make test
```

The smoke tests exercise hidden entries, classification, sorting, long and
human-readable output, filename quoting, recursive traversal, and error
handling. A GitHub Actions workflow builds the program and runs these tests on
pushes and pull requests.

## Testing

Tested on NetBSD 11.0:

- `make` completed successfully.
- `make test` reported `All ls smoke tests passed.`
- `make clean` removed the executable and object files.

## Usage

```sh
./ls
./ls -lah
./ls -R /path/to/directory
./ls -d /path/to/directory
make clean
```

`BLOCKSIZE` sets the default unit used by `-s`, unless overridden by `-h` or
`-k`.

## Source layout

- `main.c`: initializes locale and starts the program.
- `options.c`, `options.h`: parses command-line options.
- `list.c`, `list.h`: handles operands, directory traversal, sorting, and
  recursion.
- `format.c`, `format.h`: formats names, metadata, long listings, and block
  counts.
- `ls.h`: shared option and entry types.
- `Makefile`: build and cleanup rules.
- `.gitignore`: excludes build products and object files.

## Repository

https://github.com/nos1012/Son_23CE068_midterm

## Author

Student: HoangThanhSon_23CE068
