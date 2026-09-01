#!/bin/sh

set -eu

if test "$#" -ne 8; then
    echo "usage: $0 engine cgame ui game output.tar.gz package-arch module-arch source-commit" >&2
    exit 2
fi

engine=$1
cgame=$2
ui=$3
game=$4
output=$5
package_arch=$6
module_arch=$7
source_commit=$8

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
        architecture='Linux ELF32 / Intel i386'
        build_target=client-linux32-package
        strip_tool=${LINUX32_STRIP:-strip}
        ;;
    x86_64)
        architecture='Linux ELF64 / x86-64'
        build_target=client-linux64-package
        strip_tool=${LINUX64_STRIP:-strip}
        ;;
    *)
        echo "error: unsupported Linux package architecture: $package_arch" >&2
        exit 2
        ;;
esac

if ! command -v "$strip_tool" >/dev/null 2>&1; then
    echo "error: Linux strip tool not found: $strip_tool" >&2
    exit 2
fi

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
project_root=$(cd "$script_dir/../.." && pwd -P)

output_directory=$(dirname "$output_absolute")
mkdir -p "$output_directory"

package_suffix=$(printf '%s' "$source_commit" | cut -c1-9)
package_name="opencoduo-linux-$package_arch-$package_suffix"
staging_parent=$(mktemp -d "${TMPDIR:-/tmp}/opencoduo-linux-package.XXXXXX")
trap 'rm -r "$staging_parent"' EXIT HUP INT TERM
package_root="$staging_parent/$package_name"

mkdir -p "$package_root/uo"
cp "$engine" "$package_root/CoDUOMP"
cp "$cgame" "$package_root/uo/uo_cgame_mp_$module_arch.so"
cp "$ui" "$package_root/uo/uo_ui_mp_$module_arch.so"
cp "$game" "$package_root/uo/uo_game_mp_$module_arch.so"

"$strip_tool" --strip-all \
    "$package_root/CoDUOMP" \
    "$package_root/uo/uo_cgame_mp_$module_arch.so" \
    "$package_root/uo/uo_ui_mp_$module_arch.so" \
    "$package_root/uo/uo_game_mp_$module_arch.so"

{
    printf '%s\n' "Open CoD:UO multiplayer Linux $package_arch build"
    printf '\n%s\n' 'Copy the contents of this directory into the root of an existing'
    printf '%s\n' 'Call of Duty: United Offensive installation. Preserve the uo/ directory.'
    printf '%s\n' 'This package is not standalone and contains no retail game data.'
    printf '\n%s\n' 'The matching architecture of the Linux runtime libraries listed in the client'
    printf '%s\n' 'build documentation must be installed on the target system.'
    printf '\n%s\n' 'Packaged components:'
    printf '%s\n' '  CoDUOMP                              Open CoD:UO multiplayer client'
    printf '%s\n' "  uo/uo_cgame_mp_$module_arch.so          Open CoD:UO client-game module"
    printf '%s\n' "  uo/uo_ui_mp_$module_arch.so             Open CoD:UO user-interface module"
    printf '%s\n' "  uo/uo_game_mp_$module_arch.so           Open CoD:UO listen-server game module"
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
        CoDUOMP \
        "uo/uo_cgame_mp_$module_arch.so" \
        "uo/uo_ui_mp_$module_arch.so" \
        "uo/uo_game_mp_$module_arch.so" > SHA256SUMS.txt
)

(
    cd "$staging_parent"
    tar -czf "$output_absolute" "$package_name"
)

printf '%s\n' "$output_absolute"
