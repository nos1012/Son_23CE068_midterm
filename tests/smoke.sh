#!/bin/sh
set -eu

program=${1:-./ls}
fixture=$(mktemp -d "${TMPDIR:-/tmp}/ls-smoke.XXXXXX")
trap 'rm -rf "$fixture"' EXIT HUP INT TERM

mkdir "$fixture/list" "$fixture/list/subdir"
mkdir "$fixture/quoted"
printf 'a' > "$fixture/list/a"
printf 'bbbb' > "$fixture/list/b"
printf 'hidden' > "$fixture/list/.hidden"
printf 'nested' > "$fixture/list/subdir/inside"
printf 'run' > "$fixture/list/x"
chmod +x "$fixture/list/x"
ln -s a "$fixture/list/link"
mkfifo "$fixture/list/pipe"
bad_name=$(printf 'bad\001name')
: > "$fixture/quoted/$bad_name"

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
if [ "$b_position" -ge "$a_position" ]; then
    printf 'FAIL: -S should place the larger file before the smaller file\n' >&2
    exit 1
fi

inode_listing=$("$program" -i "$fixture/list/a")
printf '%s\n' "$inode_listing" | grep -E '^[0-9]+ a$' >/dev/null

long_listing=$("$program" -l "$fixture/list")
printf '%s\n' "$long_listing" | grep -E '^[^ ]{10} .* a$' >/dev/null

block_listing=$("$program" -s "$fixture/list/a")
printf '%s\n' "$block_listing" | grep -E '^[0-9]+ a$' >/dev/null

human_listing=$("$program" -h -l "$fixture/list")
if [ -z "$human_listing" ]; then
    printf 'FAIL: -h -l should print directory entries\n' >&2
    exit 1
fi

quoted_listing=$("$program" -q "$fixture/quoted")
assert_equal "-q replaces control characters" "bad?name" "$quoted_listing"
raw_listing=$("$program" -w "$fixture/quoted")
assert_equal "-w preserves raw filename bytes" "$bad_name" "$raw_listing"

directory_operand=$("$program" -d "$fixture/list")
assert_equal "-d lists the directory itself" "$fixture/list" "$directory_operand"

recursive=$("$program" -R "$fixture/list")
assert_contains "-R lists nested entries" "$recursive" "inside"
assert_contains "-R prints subdirectory header" "$recursive" "$fixture/list/subdir:"

multiple_operands=$("$program" "$fixture/list/subdir/inside" "$fixture/list")
assert_equal "file operands are listed before directories" \
    "inside" "$(printf '%s\n' "$multiple_operands" | head -n 1)"

if "$program" -z >/dev/null 2>&1; then
    printf 'FAIL: invalid option should fail\n' >&2
    exit 1
fi
if "$program" "$fixture/missing" >/dev/null 2>&1; then
    printf 'FAIL: missing operand should fail\n' >&2
    exit 1
fi

printf 'All ls smoke tests passed.\n'
