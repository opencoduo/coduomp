#!/usr/bin/env bash
set -euo pipefail

if repo_root="$(git rev-parse --show-toplevel 2>/dev/null)"; then
    :
else
    script_dir="$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
    repo_root="$(cd "$script_dir/.." && pwd -P)"
fi
cd "$repo_root"

source_paths=()
for path in assets src build-mk vendor; do
    if [[ -e "$path" ]]; then
        source_paths+=("$path")
    fi
done

if [[ ${#source_paths[@]} -eq 0 ]]; then
    echo "No public source paths found" >&2
    exit 2
fi

if [[ -d public/coduomp ]]; then
    public_frontend=public/coduomp
else
    public_frontend=.
fi

scan_paths=("${source_paths[@]}")
for path in Makefile README.md CDKEY.md CHANGELOG.md docs packaging tools; do
    if [[ -e "$public_frontend/$path" ]]; then
        scan_paths+=("$public_frontend/$path")
    fi
done

scan_globs=(
    --glob '!**/docs/branch-policy.md'
    --glob '!**/tools/check-source-policy.sh'
)

search_public()
{
    pattern="$1"
    shift
    if command -v rg >/dev/null 2>&1; then
        rg -n "${scan_globs[@]}" "$pattern" "$@"
    else
        grep -R -n -I -E \
            --exclude=branch-policy.md \
            --exclude=check-source-policy.sh \
            "$pattern" "$@"
    fi
}

search_public_insensitive()
{
    pattern="$1"
    shift
    if command -v rg >/dev/null 2>&1; then
        rg -n -i "${scan_globs[@]}" "$pattern" "$@"
    else
        grep -R -n -I -E -i \
            --exclude=branch-policy.md \
            --exclude=check-source-policy.sh \
            "$pattern" "$@"
    fi
}

failed=0

if search_public 'STRICT_STOCK' "${scan_paths[@]}"; then
    echo "error: STRICT_STOCK is retired; use the concrete stock or master branch" >&2
    failed=1
fi

if search_public_insensitive \
    'SECURITY_PATCH|ORIGINAL_BINARY_BUG_FIX|original-bugs/|security[-_[:space:]]+patch' \
    "${scan_paths[@]}"; then
    echo "error: public source contains a private security/audit disclosure marker" >&2
    failed=1
fi

if search_public \
    'coduo-binary-analysis|/Users/[^/]+/|/home/[^/]+/(llm|src|work)|[A-Za-z]:\\\\Users\\\\[^\\\\]+' \
    "${scan_paths[@]}"; then
    echo "error: public source contains a machine-specific workspace path" >&2
    failed=1
fi

if [[ $failed -ne 0 ]]; then
    exit 1
fi

echo "Public source policy check passed"
