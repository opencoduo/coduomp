#!/usr/bin/env python3
"""Generate build identity without changing the header on an unchanged build."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


# NOT_FROM_ORIGINAL_SOURCE: build tooling, independent of runtime behavior.
def coduomp_revision(root):
    # Require metadata in this source root: an archive nested in another checkout
    # must not accidentally report the enclosing repository's identity.
    if not (root / ".git").exists():
        return "unknown"
    try:
        result = subprocess.run(["git", "-C", str(root), "describe", "--always", "--dirty"],
                                capture_output=True, text=True, check=True)
        return result.stdout.strip() or "unknown"
    except (OSError, subprocess.CalledProcessError):
        # Without Git, do not assert that a checkout is clean.
        return "unknown"


# NOT_FROM_ORIGINAL_SOURCE: atomically publish only changed generated content.
def coduomp_main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    revision = coduomp_revision(args.source.resolve())
    content = ("/* Generated build metadata; do not edit. */\n"
               "#ifndef CODUOMP_BUILD_IDENTITY_H\n#define CODUOMP_BUILD_IDENTITY_H\n"
               "#define CODUOMP_GIT_REVISION " + json.dumps(revision, ensure_ascii=True) + "\n"
               "#endif\n")
    output = args.output
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and output.read_text(encoding="utf-8") == content:
        return
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n",
                                         dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(content)
        os.replace(temporary, output)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


if __name__ == "__main__":
    coduomp_main()
