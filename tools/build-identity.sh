#!/bin/sh
# NOT_FROM_ORIGINAL_SOURCE: build tooling, independent of runtime behavior.
set -eu
source_root=$1
output=$2
revision=unknown
# Require metadata in this source root: an archive nested in another checkout
# must not accidentally report the enclosing repository's identity.
if [ -e "$source_root/.git" ]; then
    # Use a bounded commit identifier so arbitrary tag names cannot expand UI text.
    # Without Git, do not assert that a checkout is clean.
    revision=$(git -C "$source_root" describe --always --dirty --match '' --abbrev=12 2>/dev/null) || revision=unknown
    [ -n "$revision" ] || revision=unknown
fi
mkdir -p "$(dirname "$output")"
temporary=$(mktemp "$output.XXXXXX")
trap 'rm "$temporary"' EXIT
cat > "$temporary" <<EOF
/* Generated build metadata; do not edit. */
#ifndef CODUOMP_BUILD_IDENTITY_H
#define CODUOMP_BUILD_IDENTITY_H
#define CODUOMP_GIT_REVISION "$revision"
#endif
EOF
if [ -e "$output" ] && cmp -s "$temporary" "$output"; then
    exit 0
fi
# GNU Make 3.81 compares whole seconds. A changed header must be newer
# than objects compiled earlier in this same second, even on fast rebuilds.
if [ -e "$output" ]; then
    sleep 1
fi
# Atomically publish only changed generated content.
mv "$temporary" "$output"
trap - EXIT
