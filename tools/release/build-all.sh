#!/bin/sh

set -eu

if test "$#" -ne 6; then
    echo "usage: $0 remote-host remote-base remote-mss32 remote-mingw32-prefix remote-mingw64-prefix output-directory" >&2
    exit 2
fi

remote_host=$1
remote_base=$2
remote_mss32=$3
remote_mingw32_prefix=$4
remote_mingw64_prefix=$5
output_directory=$6

if test "$(uname -s)" != Darwin; then
    echo 'error: release-builds must run on macOS so it can create the macOS application archive' >&2
    exit 2
fi

for remote_path in "$remote_base" "$remote_mss32" "$remote_mingw32_prefix" "$remote_mingw64_prefix"; do
    case "$remote_path" in
        /*) ;;
        *) echo "error: remote paths must be absolute: $remote_path" >&2; exit 2 ;;
    esac
    case "$remote_path" in
        *[!A-Za-z0-9_./-]*) echo "error: unsupported character in remote path: $remote_path" >&2; exit 2 ;;
    esac
done

case "$remote_host" in
    *[!A-Za-z0-9_.@:-]*) echo "error: unsupported character in remote host: $remote_host" >&2; exit 2 ;;
esac

project_root=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd -P)
cd "$project_root"

if test -n "$(git status --porcelain --untracked-files=no)"; then
    echo 'error: release-builds requires a clean tracked Git worktree' >&2
    exit 2
fi

source_commit=$(git rev-parse HEAD)
short_commit=$(printf '%s' "$source_commit" | cut -c1-9)
release_stamp=$(date +%Y%m%d%H%M%S)
remote_archive="/tmp/coduomp-release-$short_commit-$release_stamp.tar"
remote_build_root="$remote_base/coduomp-release-$short_commit-$release_stamp"
source_archive=$(mktemp "${TMPDIR:-/tmp}/coduomp-release.XXXXXX.tar")
trap 'rm "$source_archive"' EXIT HUP INT TERM

mkdir -p "$output_directory"
for output in \
    "$output_directory/opencoduo-macos-arm64-$short_commit.zip" \
    "$output_directory/opencoduo-linux-x86_64-$short_commit.tar.gz" \
    "$output_directory/opencoduo-windows-i686-$short_commit.zip" \
    "$output_directory/opencoduo-windows-x86_64-$short_commit.zip"; do
    if test -e "$output"; then
        echo "error: refusing to overwrite existing release artifact: $output" >&2
        exit 2
    fi
done

make macos-zip \
    BUILD_DIR=".workbench/build/release-work-$short_commit/macos" \
    MACOS_APP_DIR=".workbench/build/release-work-$short_commit/macos/OpenCoDUO.app" \
    MACOS_ZIP="$output_directory/opencoduo-macos-arm64-$short_commit.zip" \
    SOURCE_COMMIT="$source_commit"

git archive --format=tar --output "$source_archive" "$source_commit"
scp "$source_archive" "$remote_host:$remote_archive"

ssh "$remote_host" "set -eu
archive='$remote_archive'
build_root='$remote_build_root'
mkdir -p \"\$build_root\"
tar -xf \"\$archive\" -C \"\$build_root\"
cd \"\$build_root\"
make JOBS='${JOBS:-}' SOURCE_COMMIT='$source_commit' client-linux64-package
make JOBS='${JOBS:-}' SOURCE_COMMIT='$source_commit' \\
    MSS32_DLL='$remote_mss32' \\
    MINGW32_DEP_PREFIX='$remote_mingw32_prefix' \\
    client-windows-i686-package
make JOBS='${JOBS:-}' SOURCE_COMMIT='$source_commit' \\
    MINGW64_DEP_PREFIX='$remote_mingw64_prefix' \\
    client-windows-x86_64-package"

scp \
    "$remote_host:$remote_build_root/.workbench/build/linux/opencoduo-linux-x86_64-$short_commit.tar.gz" \
    "$remote_host:$remote_build_root/.workbench/build/windows/opencoduo-windows-i686-$short_commit.zip" \
    "$remote_host:$remote_build_root/.workbench/build/windows/opencoduo-windows-x86_64-$short_commit.zip" \
    "$output_directory/"

printf '%s\n' \
    "$output_directory/opencoduo-macos-arm64-$short_commit.zip" \
    "$output_directory/opencoduo-linux-x86_64-$short_commit.tar.gz" \
    "$output_directory/opencoduo-windows-i686-$short_commit.zip" \
    "$output_directory/opencoduo-windows-x86_64-$short_commit.zip"
