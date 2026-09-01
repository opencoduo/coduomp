#!/bin/sh

set -eu

if test "$#" -ne 7; then
    echo "usage: $0 engine cgame ui game output.zip package-arch source-commit" >&2
    exit 2
fi

engine=$1
cgame=$2
ui=$3
game=$4
output=$5
package_arch=$6
source_commit=$7

for input in "$engine" "$cgame" "$ui" "$game"; do
    if test ! -f "$input"; then
        echo "error: required package input does not exist: $input" >&2
        exit 2
    fi
done

if test -e "$output"; then
    echo "error: refusing to overwrite existing package: $output" >&2
    exit 2
fi

case "$output" in
    /*) output_absolute=$output ;;
    *) output_absolute=$(pwd)/$output ;;
esac

case "$package_arch" in
    i686)
        architecture='Windows PE32 / Intel i686'
        build_target=client-windows-i686-package
        audio_note='Copy mss32.dll from a legally owned retail installation beside CoDUOMP.exe.'
        objdump=${MINGW32_OBJDUMP:-i686-w64-mingw32-objdump}
        strip_tool=${MINGW32_STRIP:-i686-w64-mingw32-strip}
        ;;
    x86_64)
        architecture='Windows PE32+ / x86-64'
        build_target=client-windows-x86_64-package
        audio_note='This experimental x86-64 build uses the no-audio compatibility backend.'
        objdump=${MINGW64_OBJDUMP:-x86_64-w64-mingw32-objdump}
        strip_tool=${MINGW64_STRIP:-x86_64-w64-mingw32-strip}
        ;;
    *)
        echo "error: unsupported Windows package architecture: $package_arch" >&2
        exit 2
        ;;
esac

if ! command -v "$strip_tool" >/dev/null 2>&1; then
    echo "error: Windows strip tool not found: $strip_tool" >&2
    exit 2
fi

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
project_root=$(cd "$script_dir/../.." && pwd -P)

"$project_root/tools/windows/audit-runtime-imports.sh" "$objdump" \
    "$engine" "$cgame" "$ui" "$game"

output_directory=$(dirname "$output_absolute")
mkdir -p "$output_directory"

package_suffix=$(printf '%s' "$source_commit" | cut -c1-9)
package_name="opencoduo-windows-$package_arch-$package_suffix"
staging_parent=$(mktemp -d "${TMPDIR:-/tmp}/opencoduo-windows-package.XXXXXX")
trap 'rm -r "$staging_parent"' EXIT HUP INT TERM
package_root="$staging_parent/$package_name"

mkdir -p "$package_root/uo"
cp "$engine" "$package_root/CoDUOMP.exe"
cp "$cgame" "$package_root/uo/uo_cgame_mp_x86.dll"
cp "$ui" "$package_root/uo/uo_ui_mp_x86.dll"
cp "$game" "$package_root/uo/uo_game_mp_x86.dll"

"$strip_tool" --strip-all \
    "$package_root/CoDUOMP.exe" \
    "$package_root/uo/uo_cgame_mp_x86.dll" \
    "$package_root/uo/uo_ui_mp_x86.dll" \
    "$package_root/uo/uo_game_mp_x86.dll"

"$project_root/tools/windows/audit-runtime-imports.sh" "$objdump" \
    "$package_root/CoDUOMP.exe" \
    "$package_root/uo/uo_cgame_mp_x86.dll" \
    "$package_root/uo/uo_ui_mp_x86.dll" \
    "$package_root/uo/uo_game_mp_x86.dll"

{
    printf '%s\n' "Open CoD:UO multiplayer Windows $package_arch build"
    printf '\n%s\n' 'Copy the contents of this directory into the root of an existing'
    printf '%s\n' 'Call of Duty: United Offensive installation. Preserve the uo/ directory.'
    printf '%s\n' 'This package is not standalone and contains no retail game data.'
    printf '\n%s\n' "$audio_note"
    printf '\n%s\n' 'Packaged components:'
    printf '%s\n' '  CoDUOMP.exe                     Open CoD:UO multiplayer client'
    printf '%s\n' '  uo/uo_cgame_mp_x86.dll          Open CoD:UO client-game module'
    printf '%s\n' '  uo/uo_ui_mp_x86.dll             Open CoD:UO user-interface module'
    printf '%s\n' '  uo/uo_game_mp_x86.dll           Open CoD:UO listen-server game module'
} > "$package_root/README.txt"

{
    printf 'source_commit=%s\n' "$source_commit"
    printf 'architecture=%s\n' "$architecture"
    printf 'build_target=%s\n' "$build_target"
} > "$package_root/BUILDINFO.txt"

"$project_root/tools/release/audit-package-privacy.sh" "$package_root"

(
    cd "$package_root"
    sha256sum \
        CoDUOMP.exe \
        uo/uo_cgame_mp_x86.dll \
        uo/uo_ui_mp_x86.dll \
        uo/uo_game_mp_x86.dll > SHA256SUMS.txt
)

(
    cd "$staging_parent"
    zip -X -q -r "$output_absolute" "$package_name"
)

printf '%s\n' "$output_absolute"
