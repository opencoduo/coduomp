#!/bin/sh

set -eu

if test "$#" -lt 2; then
    echo "usage: $0 objdump binary [binary ...]" >&2
    exit 2
fi

objdump=$1
shift

if ! command -v "$objdump" >/dev/null 2>&1; then
    echo "error: Windows dependency audit tool not found: $objdump" >&2
    exit 2
fi

audit_output=$(mktemp "${TMPDIR:-/tmp}/coduomp-pe-imports.XXXXXX")
trap 'rm "$audit_output"' EXIT HUP INT TERM

for binary in "$@"; do
    if test ! -f "$binary"; then
        echo "error: Windows dependency audit input does not exist: $binary" >&2
        exit 2
    fi

    if ! "$objdump" -p "$binary" > "$audit_output"; then
        echo "error: unable to inspect Windows dependencies: $binary" >&2
        exit 2
    fi

    runtime_imports=$(grep -Ei \
        'DLL Name: (libgcc_s_|libstdc\+\+-6\.dll|libwinpthread-1\.dll)' \
        "$audit_output" || :)
    if test -n "$runtime_imports"; then
        echo "error: $binary dynamically imports a MinGW runtime DLL:" >&2
        printf '%s\n' "$runtime_imports" >&2
        exit 2
    fi
done
