#!/bin/sh

set -eu

if [ "$#" -ne 9 ]; then
    echo "usage: $0 engine cgame ui app info-plist bundle-id version build-number sign-identity" >&2
    exit 2
fi

engine_path=$1
cgame_path=$2
ui_path=$3
app_path=$4
info_plist_path=$5
bundle_identifier=$6
bundle_version=$7
bundle_build_number=$8
sign_identity=$9

if [ "$(uname -s)" != "Darwin" ]; then
    echo "error: macOS application packaging must run on macOS" >&2
    exit 2
fi
for required_path in "$engine_path" "$cgame_path" "$ui_path" "$info_plist_path"; do
    if [ ! -f "$required_path" ]; then
        echo "error: missing packaging input: $required_path" >&2
        exit 2
    fi
done
case "$app_path" in
    */OpenCoDUO.app) ;;
    *)
        echo "error: refusing to stage unexpected application path: $app_path" >&2
        exit 2
        ;;
esac
if [ -e "$app_path" ]; then
    echo "error: refusing to overwrite existing application: $app_path" >&2
    exit 2
fi

project_root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
icon_source_path="$project_root/assets/macos/coduomp.icns"
if [ ! -f "$icon_source_path" ]; then
    echo "error: missing application icon: $icon_source_path" >&2
    exit 2
fi
app_parent=$(dirname "$app_path")
staging_root="$app_parent/.opencoduo-app-stage-$$"
staged_app="$staging_root/OpenCoDUO.app"
contents_path="$staged_app/Contents"
macos_path="$contents_path/MacOS"
frameworks_path="$contents_path/Frameworks"
resources_path="$contents_path/Resources"

cleanup_stage()
{
    if [ -d "$staging_root" ]; then
        rm -r "$staging_root"
    fi
}
trap cleanup_stage EXIT HUP INT TERM

mkdir -p "$macos_path" "$frameworks_path" "$resources_path"
COPYFILE_DISABLE=1 cp "$info_plist_path" "$contents_path/Info.plist"
COPYFILE_DISABLE=1 cp "$engine_path" "$macos_path/CoDUOMP"
COPYFILE_DISABLE=1 cp "$cgame_path" "$frameworks_path/uo_cgame_mp_arm64.dylib"
COPYFILE_DISABLE=1 cp "$ui_path" "$frameworks_path/uo_ui_mp_arm64.dylib"
chmod 755 "$macos_path/CoDUOMP" "$frameworks_path/uo_cgame_mp_arm64.dylib" \
    "$frameworks_path/uo_ui_mp_arm64.dylib"

plutil -replace CFBundleIdentifier -string "$bundle_identifier" \
    "$contents_path/Info.plist"
plutil -replace CFBundleShortVersionString -string "$bundle_version" \
    "$contents_path/Info.plist"
plutil -replace CFBundleVersion -string "$bundle_build_number" \
    "$contents_path/Info.plist"
plutil -lint "$contents_path/Info.plist"

COPYFILE_DISABLE=1 cp "$icon_source_path" "$resources_path/OpenCoDUO.icns"

dependency_path()
{
    dependency_pattern=$1
    otool -L "$engine_path" | awk -v pattern="$dependency_pattern" '
        NR > 1 && $1 ~ pattern { print $1; exit }'
}

copy_dependency()
{
    dependency_pattern=$1
    source_path=$(dependency_path "$dependency_pattern")
    if [ -z "$source_path" ] || [ ! -f "$source_path" ]; then
        echo "error: could not locate linked dependency matching $dependency_pattern" >&2
        exit 1
    fi

    library_name=$(basename "$source_path")
    destination_path="$frameworks_path/$library_name"
    COPYFILE_DISABLE=1 cp -L "$source_path" "$destination_path"
    chmod 755 "$destination_path"
    install_name_tool -change "$source_path" "@rpath/$library_name" \
        "$macos_path/CoDUOMP"
    install_name_tool -id "@rpath/$library_name" "$destination_path"

    otool -l "$destination_path" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" {
            getline
            getline
            print $2
        }' | while IFS= read -r runtime_path; do
            case "$runtime_path" in
                @*|/usr/lib/*|/System/Library/*) ;;
                *) install_name_tool -delete_rpath "$runtime_path" \
                       "$destination_path" ;;
            esac
        done
}

copy_dependency 'libSDL2-[^/]*[.]dylib$'
copy_dependency 'libjpeg[.][^/]*[.]dylib$'
copy_dependency 'libminizip[.][^/]*[.]dylib$'

install_name_tool -add_rpath '@executable_path/../Frameworks' \
    "$macos_path/CoDUOMP"
install_name_tool -id '@rpath/uo_cgame_mp_arm64.dylib' \
    "$frameworks_path/uo_cgame_mp_arm64.dylib"
install_name_tool -id '@rpath/uo_ui_mp_arm64.dylib' \
    "$frameworks_path/uo_ui_mp_arm64.dylib"

for code_path in "$macos_path/CoDUOMP" "$frameworks_path"/*.dylib; do
    xcrun strip -S -x "$code_path"
    codesign --remove-signature "$code_path" >/dev/null 2>&1 || true
done

for module_path in "$frameworks_path/uo_cgame_mp_arm64.dylib" \
                   "$frameworks_path/uo_ui_mp_arm64.dylib"; do
    if ! dyld_info -exports "$module_path" | grep -q '_dllEntry$' ||
       ! dyld_info -exports "$module_path" | grep -q '_vmMain$'; then
        echo "error: required module exports were stripped from $module_path" >&2
        exit 1
    fi
done

xattr -cr "$staged_app"
for code_path in "$frameworks_path"/*.dylib; do
    if [ "$sign_identity" = "-" ]; then
        codesign --sign - --timestamp=none "$code_path"
    else
        codesign --sign "$sign_identity" --options runtime --timestamp \
            "$code_path"
    fi
done
if [ "$sign_identity" = "-" ]; then
    codesign --sign - --timestamp=none "$staged_app"
else
    codesign --sign "$sign_identity" --options runtime --timestamp \
        "$staged_app"
fi

codesign --verify --deep --strict --verbose=2 "$staged_app"
"$project_root/tools/macos/audit-app-privacy.sh" "$staged_app"

mv "$staged_app" "$app_path"
rmdir "$staging_root"
trap - EXIT HUP INT TERM

echo "created $app_path"
