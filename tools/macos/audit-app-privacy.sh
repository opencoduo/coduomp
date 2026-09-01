#!/bin/sh

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

audit_failed=0
builder_home=${HOME-}
builder_user=$(id -un)
builder_host=$(hostname)

report_private_match()
{
    category=$1
    file_path=$2
    relative_path=${file_path#"$app_path"/}
    echo "privacy audit: $category in $relative_path" >&2
    audit_failed=1
}

contains_literal()
{
    literal=$1
    file_path=$2
    [ -n "$literal" ] && LC_ALL=C grep -a -F -q "$literal" "$file_path"
}

while IFS= read -r file_path; do
    if contains_literal "/Users/" "$file_path"; then
        report_private_match "absolute user path" "$file_path"
    fi
    if contains_literal "/private/var/folders/" "$file_path"; then
        report_private_match "temporary user path" "$file_path"
    fi
    if contains_literal "/opt/homebrew" "$file_path" ||
       contains_literal "/usr/local/Cellar/" "$file_path"; then
        report_private_match "build-host package path" "$file_path"
    fi
    if contains_literal ".workbench" "$file_path"; then
        report_private_match "private workspace path" "$file_path"
    fi
    if [ -n "$builder_home" ] &&
       contains_literal "$builder_home" "$file_path"; then
        report_private_match "builder home path" "$file_path"
    fi
    if contains_literal "/$builder_user/" "$file_path"; then
        report_private_match "builder username path segment" "$file_path"
    fi
    if [ "${#builder_user}" -ge 5 ] &&
       contains_literal "$builder_user" "$file_path"; then
        report_private_match "builder username" "$file_path"
    fi
    if [ -n "$builder_host" ] &&
       contains_literal "$builder_host" "$file_path"; then
        report_private_match "builder hostname" "$file_path"
    fi
done <<EOF
$(find "$app_path" -type f -print)
EOF

while IFS= read -r unwanted_path; do
    [ -n "$unwanted_path" ] || continue
    echo "privacy audit: forbidden release artifact ${unwanted_path#"$app_path"/}" >&2
    audit_failed=1
done <<EOF
$(find "$app_path" \( -name .DS_Store -o -name '*.dSYM' -o \
    -name '*.o' -o -name '*.d' -o -name '*.log' -o -name __MACOSX \) -print)
EOF

while IFS= read -r symlink_path; do
    [ -n "$symlink_path" ] || continue
    echo "privacy audit: symbolic link ${symlink_path#"$app_path"/}" >&2
    audit_failed=1
done <<EOF
$(find "$app_path" -type l -print)
EOF

while IFS= read -r file_path; do
    while IFS= read -r attribute_name; do
        [ -n "$attribute_name" ] || continue
        case "$attribute_name" in
            com.apple.provenance) ;;
            *) report_private_match "unexpected extended attribute" \
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
        report_private_match "nonportable Mach-O dependency" "$file_path"
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
        report_private_match "nonportable Mach-O runtime path" "$file_path"
    fi
done <<EOF
$(find "$app_path" -type f -print)
EOF

if [ "$audit_failed" -ne 0 ]; then
    echo "privacy audit failed" >&2
    exit 1
fi

echo "privacy audit passed: no personal or build-host paths in $app_path"
