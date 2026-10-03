# Son_23CE068_midterm

UNIX `ls(1)` command implementation - Midterm Project

## Overview

This repository contains a simplified implementation of the UNIX `ls(1)` command in C. The project is designed to follow the subset of behavior described in the assignment manual and demonstrates core UNIX filesystem concepts, command-line parsing, and modular C program design.

## Project Goal

The objective of this project is to implement a small `ls`-like utility that can:

- list entries in a directory
- support a limited set of options
- handle hidden files when requested
- print file metadata in a long format when required
- process multiple directory arguments correctly
- avoid crashes and handle invalid paths gracefully

## Repository

GitHub repository:
https://github.com/nos1012/Son_23CE068_midterm

## Expected Features

This implementation is expected to include the required subset of `ls` behavior, such as:

- default directory listing
- option parsing (`-a`, `-l`, `-R`, and any other options specified by the assignment)
- sorting of directory entries
- recursive directory traversal when enabled
- proper output formatting for files and directories
- robust error handling for permission and path problems

## Project Structure

The project is organized into multiple source and header files, for example:

- `src/main.c`
- `src/ls.c`
- `src/options.c`
- `src/filesystem.c`
- `include/ls.h`
- `include/options.h`
- `include/filesystem.h`
- `Makefile`
- `.gitignore`

## Build Instructions

To compile the project, run:

```bash
make
```

This will create the executable for the simplified `ls` implementation.

## Run the Program

Examples:

```bash
./ls_sim .
./ls_sim -a .
./ls_sim -l /tmp
./ls_sim -R .
```

The exact command name may vary depending on the final executable name chosen in the Makefile.

## Cleanup

To remove compiled object files and the executable:

```bash
make clean
```

## Notes

- This is a simplified version of `ls` and should follow the assignment manual rather than full GNU `ls` behavior.
- The program should be modular, readable, and well-commented.
- Object files and compiled binaries must not be committed to the repository.

## Author

Student: Son_23CE068
