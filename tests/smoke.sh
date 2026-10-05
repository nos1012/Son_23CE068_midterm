#!/bin/sh
set -eu

program=${1:-./ls}
fixture=$(mktemp -d "${TMPDIR:-/tmp}/ls-smoke.XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM

mkdir "$fixture/list" "$fixture/list/subdir"
mkdir "$fixture/quoted"
mkdir "$fixture/timestamps"
printf 'a' > "$fixture/list/a"
printf 'bbbb' > "$fixture/list/b"
printf 'hidden' > "$fixture/list/.hidden"
printf 'nested' > "$fixture/list/subdir/inside"
printf 'run' > "$fixture/list/x"
chmod +x "$fixture/list/x"
ln -s a "$fixture/list/link"
mkfifo "$fixture/list/pipe"
bad_name=$(printf 'bad\001name')
utf8_name=$(printf 'caf\303\251')
: > "$fixture/quoted/$bad_name"
: > "$fixture/quoted/$utf8_name"
touch -t 200001010000 "$fixture/timestamps/old"

assert_equal()
{
    description=$1
    expected=$2
    actual=$3
    if [ "$actual" != "$expected" ]; then
        printf 'FAIL: %s\nExpected:\n%s\nActual:\n%s\n' \
            "$description" "$expected" "$actual" >&2
        exit 1
    fi
}

assert_contains()
{
    description=$1
    content=$2
    line=$3
    if ! printf '%s\n' "$content" | grep -F -x "$line" >/dev/null; then
        printf 'FAIL: %s (missing line: %s)\n' "$description" "$line" >&2
        exit 1
    fi
}

assert_not_contains()
{
    description=$1
    content=$2
    line=$3
    if printf '%s\n' "$content" | grep -F -x "$line" >/dev/null; then
        printf 'FAIL: %s (unexpected line: %s)\n' "$description" "$line" >&2
        exit 1
    fi
}

assert_matches()
{
    description=$1
    pattern=$2
    content=$3
    if ! printf '%s\n' "$content" | grep -E "$pattern" >/dev/null; then
        printf 'FAIL: %s (pattern not found: %s)\n' \
            "$description" "$pattern" >&2
        exit 1
    fi
}

assert_numeric_prefix()
{
    description=$1
    content=$2
    path=$3
    prefix=${content%" $path"}

    if [ "$prefix" = "$content" ] || [ -z "$prefix" ]; then
        printf 'FAIL: %s (missing numeric prefix)\n' "$description" >&2
        exit 1
    fi
    case "$prefix" in
        *[!0-9]*)
            printf 'FAIL: %s (prefix is not numeric: %s)\n' \
                "$description" "$prefix" >&2
            exit 1
            ;;
    esac
}

listing=$("$program" "$fixture/list")
assert_equal "default sorted listing" \
    "$(printf 'a\nb\nlink\npipe\nsubdir\nx')" "$listing"

all_but_dot=$("$program" -A "$fixture/list")
assert_contains "-A includes hidden entries" "$all_but_dot" ".hidden"
assert_not_contains "-A omits dot entries" "$all_but_dot" "."
assert_not_contains "-A omits parent entry" "$all_but_dot" ".."

all_entries=$("$program" -a "$fixture/list")
assert_contains "-a includes current directory" "$all_entries" "."
assert_contains "-a includes parent directory" "$all_entries" ".."

classified=$("$program" -F "$fixture/list")
assert_contains "-F marks executable files" "$classified" "x*"
assert_contains "-F marks symlinks" "$classified" "link@"
assert_contains "-F marks FIFOs" "$classified" "pipe|"
assert_contains "-F marks directories" "$classified" "subdir/"

reverse=$("$program" -r "$fixture/list")
assert_equal "-r reverses name order" \
    "$(printf 'x\nsubdir\npipe\nlink\nb\na')" "$reverse"

by_size=$("$program" -S "$fixture/list")
b_position=$(printf '%s\n' "$by_size" | grep -n -x 'b' | cut -d: -f1)
a_position=$(printf '%s\n' "$by_size" | grep -n -x 'a' | cut -d: -f1)
if [ -z "$b_position" ] || [ -z "$a_position" ] ||
   [ "$b_position" -ge "$a_position" ]; then
    printf 'FAIL: -S should place the larger file before the smaller file\n' >&2
    exit 1
fi

inode_listing=$("$program" -i "$fixture/list/a")
assert_numeric_prefix "-i prefixes the filename with its inode" \
    "$inode_listing" "$fixture/list/a"

long_listing=$("$program" -l "$fixture/list")
assert_matches "-l includes a long-format record" '^[^ ]{10} .* a$' "$long_listing"

block_listing=$("$program" -s "$fixture/list/a")
assert_numeric_prefix "-s prefixes the filename with a block count" \
    "$block_listing" "$fixture/list/a"

human_listing=$("$program" -h -l "$fixture/list")
if [ -z "$human_listing" ]; then
    printf 'FAIL: -h -l should print directory entries\n' >&2
    exit 1
fi

quoted_listing=$(LC_ALL=C.UTF-8 "$program" -q "$fixture/quoted")
assert_contains "-q replaces control characters" "$quoted_listing" "bad?name"
assert_contains "-q preserves printable UTF-8 characters" "$quoted_listing" "$utf8_name"
raw_listing=$("$program" -w "$fixture/quoted")
assert_contains "-w preserves raw filename bytes" "$raw_listing" "$bad_name"

old_timestamp_listing=$("$program" -l "$fixture/timestamps")
assert_matches "-l displays the year for old timestamps" \
    '  2000 old$' "$old_timestamp_listing"

directory_operand=$("$program" -d "$fixture/list")
assert_equal "-d lists the directory itself" "$fixture/list" "$directory_operand"

recursive=$("$program" -R "$fixture/list")
assert_contains "-R lists nested entries" "$recursive" "inside"
assert_contains "-R prints subdirectory header" "$recursive" "$fixture/list/subdir:"

directory_after_recursive=$("$program" -R -d "$fixture/list")
assert_equal "-d overrides an earlier -R" "$fixture/list" "$directory_after_recursive"
recursive_after_directory=$("$program" -d -R "$fixture/list")
assert_contains "-R overrides an earlier -d" "$recursive_after_directory" "inside"

multiple_operands=$("$program" "$fixture/list/subdir/inside" "$fixture/list")
assert_equal "file operands are listed before directories" \
    "$fixture/list/subdir/inside" \
    "$(printf '%s\n' "$multiple_operands" | head -n 1)"

if "$program" -z >/dev/null 2>&1; then
    printf 'FAIL: invalid option should fail\n' >&2
    exit 1
fi
if "$program" "$fixture/missing" >/dev/null 2>&1; then
    printf 'FAIL: missing operand should fail\n' >&2
    exit 1
fi

printf 'All ls smoke tests passed.\n'
