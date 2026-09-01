#!/bin/sh

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

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
project_root=$(cd "$script_dir/../.." && pwd -P)
builder_home=${HOME-}
builder_user=$(id -un)
builder_host=$(hostname)
audit_failed=0

report_private_match()
{
    category=$1
    file_path=$2
    relative_path=${file_path#"$package_root"/}
    echo "privacy audit: $category in $relative_path" >&2
    audit_failed=1
}

contains_literal()
{
    literal=$1
    file_path=$2
    test -n "$literal" && LC_ALL=C grep -a -F -q "$literal" "$file_path"
}

while IFS= read -r file_path; do
    if contains_literal "/Users/" "$file_path" ||
       contains_literal "/home/" "$file_path" ||
       contains_literal ':\Users\' "$file_path"; then
        report_private_match "absolute user path" "$file_path"
    fi
    if contains_literal "/private/var/folders/" "$file_path"; then
        report_private_match "temporary user path" "$file_path"
    fi
    if contains_literal "$project_root" "$file_path"; then
        report_private_match "source workspace path" "$file_path"
    fi
    if contains_literal ".workbench/" "$file_path"; then
        report_private_match "private workspace path" "$file_path"
    fi
    if test -n "$builder_home" && contains_literal "$builder_home" "$file_path"; then
        report_private_match "builder home path" "$file_path"
    fi
    if test "${#builder_user}" -ge 5 && contains_literal "$builder_user" "$file_path"; then
        report_private_match "builder username" "$file_path"
    fi
    if test "${#builder_host}" -ge 8 && contains_literal "$builder_host" "$file_path"; then
        report_private_match "builder hostname" "$file_path"
    fi
done <<EOF
$(find "$package_root" -type f -print)
EOF

while IFS= read -r unwanted_path; do
    test -n "$unwanted_path" || continue
    echo "privacy audit: forbidden release artifact ${unwanted_path#"$package_root"/}" >&2
    audit_failed=1
done <<EOF
$(find "$package_root" \( -name .DS_Store -o -name '*.dSYM' -o \
    -name '*.o' -o -name '*.d' -o -name '*.log' -o -name __MACOSX \) -print)
EOF

while IFS= read -r symlink_path; do
    test -n "$symlink_path" || continue
    echo "privacy audit: symbolic link ${symlink_path#"$package_root"/}" >&2
    audit_failed=1
done <<EOF
$(find "$package_root" -type l -print)
EOF

if test "$audit_failed" -ne 0; then
    echo "privacy audit failed" >&2
    exit 1
fi

echo "privacy audit passed: no personal or build-host paths in $package_root"
