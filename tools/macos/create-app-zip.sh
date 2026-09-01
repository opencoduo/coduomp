#!/bin/sh

set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 /path/to/OpenCoDUO.app /path/to/opencoduo.zip" >&2
    exit 2
fi

app_path=$1
zip_path=$2
if [ ! -d "$app_path" ] || [ "${app_path##*/}" != "OpenCoDUO.app" ]; then
    echo "error: expected OpenCoDUO.app: $app_path" >&2
    exit 2
fi
case "$zip_path" in
    *.zip) ;;
    *)
        echo "error: expected a ZIP output path: $zip_path" >&2
        exit 2
        ;;
esac
if [ -e "$zip_path" ]; then
    echo "error: refusing to overwrite existing ZIP: $zip_path" >&2
    exit 2
fi

zip_parent=$(dirname "$zip_path")
temporary_zip="$zip_parent/.opencoduo-archive-$$.zip"
mkdir -p "$zip_parent"
if [ -e "$temporary_zip" ]; then
    echo "error: temporary ZIP path already exists: $temporary_zip" >&2
    exit 1
fi

cleanup_zip()
{
    if [ -f "$temporary_zip" ]; then
        rm "$temporary_zip"
    fi
}
trap cleanup_zip EXIT HUP INT TERM

COPYFILE_DISABLE=1 DITTONORSRC=1 ditto -c -k --norsrc --noextattr \
    --noqtn --noacl --keepParent "$app_path" "$temporary_zip"

archive_entries=$(zipinfo -1 "$temporary_zip")
if printf '%s\n' "$archive_entries" | grep -q '^__MACOSX/' ||
   printf '%s\n' "$archive_entries" | grep -E -q '(^|/)[.]_' ||
   printf '%s\n' "$archive_entries" | grep -E -q '(^|/)[.]DS_Store$' ||
   printf '%s\n' "$archive_entries" | grep -q '^/' ||
   printf '%s\n' "$archive_entries" | grep -q '\.\./' ||
   printf '%s\n' "$archive_entries" | grep -v '^OpenCoDUO[.]app/' | grep -q .; then
    echo "error: ZIP contains an unexpected entry" >&2
    exit 1
fi

mv "$temporary_zip" "$zip_path"
trap - EXIT HUP INT TERM

echo "created $zip_path"
