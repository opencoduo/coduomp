#!/usr/bin/env bash

set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/OpenCoDUO.app" >&2
    exit 2
fi

app_path=$1
if [ ! -d "$app_path" ] || [ "${app_path##*.}" != "app" ]; then
    echo "error: expected an application bundle: $app_path" >&2
    exit 2
fi

project_root=$(CDPATH= cd "$(dirname "$0")/../.." && pwd -P)
"$project_root/tools/audit-package.sh" "$app_path"
audit_failed=0

report_package_issue()
{
    category=$1
    file_path=$2
    relative_path=${file_path#"$app_path"/}
    echo "package audit: $category in $relative_path" >&2
    audit_failed=1
}

while IFS= read -r file_path; do
    while IFS= read -r attribute_name; do
        [ -n "$attribute_name" ] || continue
        case "$attribute_name" in
            com.apple.provenance) ;;
            *) report_package_issue "unexpected extended attribute" \
                   "$file_path" ;;
        esac
    done <<EOF
$(xattr "$file_path")
EOF
done <<EOF
$(find "$app_path" -print)
EOF

while IFS= read -r file_path; do
    if ! file -b "$file_path" | grep -q 'Mach-O'; then
        continue
    fi

    forbidden_dependencies=$(otool -L "$file_path" | awk '
        NR > 1 {
            path = $1
            if (path !~ /^@/ &&
                path !~ /^\/usr\/lib\// &&
                path !~ /^\/System\/Library\//) {
                print path
            }
        }')
    if [ -n "$forbidden_dependencies" ]; then
        report_package_issue "nonportable Mach-O dependency" "$file_path"
    fi

    forbidden_rpaths=$(otool -l "$file_path" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" {
            getline
            getline
            path = $2
            if (path !~ /^@/ &&
                path !~ /^\/usr\/lib\// &&
                path !~ /^\/System\/Library\//) {
                print path
            }
        }')
    if [ -n "$forbidden_rpaths" ]; then
        report_package_issue "nonportable Mach-O runtime path" "$file_path"
    fi
done <<EOF
$(find "$app_path" -type f -print)
EOF

if [ "$audit_failed" -ne 0 ]; then
    echo "package audit failed" >&2
    exit 1
fi

echo "package audit passed: portable dependencies and clean metadata in $app_path"
