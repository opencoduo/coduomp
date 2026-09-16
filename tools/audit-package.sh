#!/usr/bin/env bash

set -eu

if test "$#" -ne 1; then
    echo "usage: $0 /path/to/package-root" >&2
    exit 2
fi

package_root=$1
if test ! -d "$package_root"; then
    echo "error: expected a package directory: $package_root" >&2
    exit 2
fi

audit_failed=0

while IFS= read -r unwanted_path; do
    test -n "$unwanted_path" || continue
    echo "package audit: unexpected build artifact ${unwanted_path#"$package_root"/}" >&2
    audit_failed=1
done <<EOF
$(find "$package_root" \( -name .DS_Store -o -name '*.dSYM' -o \
    -name '*.o' -o -name '*.d' -o -name '*.log' -o -name __MACOSX \) -print)
EOF

while IFS= read -r symlink_path; do
    test -n "$symlink_path" || continue
    echo "package audit: symbolic link ${symlink_path#"$package_root"/}" >&2
    audit_failed=1
done <<EOF
$(find "$package_root" -type l -print)
EOF

if test "$audit_failed" -ne 0; then
    echo "package audit failed" >&2
    exit 1
fi

echo "package audit passed: no build debris or symbolic links in $package_root"
