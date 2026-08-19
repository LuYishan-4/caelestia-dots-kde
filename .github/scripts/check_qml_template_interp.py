#!/usr/bin/env python3
r"""Check for unescaped bash parameter expansion inside QML template literals.

QML (like JavaScript) treats `` `...${...}...` `` as a template literal where
``${...}`` is evaluated as JavaScript before the string is used. Embedded bash
scripts therefore must escape a literal ``${`` as ``\${`` — e.g. ``${1#v}``
must be written ``\${1#v}``.

Leaving it unescaped makes the QML engine parse bash syntax as JavaScript and
fail with an error like ``Expected token ','``. Because every singleton that
transitively imports the broken file then reports "Type X unavailable", the
failure cascades across the whole shell (see shell/services/UpdateChecker.qml,
which shipped ``${1#v}`` unescaped and broke every singleton on load).

This checker scans every backtick template literal in QML files and flags
unescaped ``${...}`` whose body is clearly bash parameter expansion rather than
valid JavaScript. Legitimate interpolation (identifiers, member access, calls,
ternaries) is left alone.

Usage:
    python3 check_qml_template_interp.py            # all *.qml under repo root
    python3 check_qml_template_interp.py <paths...> # specific files/dirs

Exit code is 1 if any offending interpolation is found.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

RED = "\033[0;31m"
GREEN = "\033[0;32m"
BOLD = "\033[1m"
RESET = "\033[0m"

# Bash parameter-expansion bodies that are never valid JavaScript. A legitimate
# JS interpolation here is an identifier/member/call expression, none of which
# match these forms.
BASH_INTERP_RE = re.compile(
    r"#"            # ${#var} length, ${var#pat} / ${var##pat} removal, ${1#v}
    r"|%"           # ${var%pat} / ${var%%pat} removal
    r"|:-|:=|:\?|:\+"  # ${var:-def} ${var:=def} ${var:?err} ${var:+alt}
    r"|:[0-9]"      # ${var:0} / ${var:0:5} substring
    r"|\^"          # ${var^} / ${var^^} case conversion
    r"|,$"          # ${var,} / ${var,,} lowercase
    r"|\[[@*]\]"    # ${arr[@]} / ${arr[*]} array expansion
)


def _unescaped_interpolations(src: str) -> list[tuple[int, str]]:
    """Return (offset, body) for unescaped ${...} inside backtick literals."""
    found: list[tuple[int, str]] = []
    in_template = False
    i = 0
    n = len(src)
    while i < n:
        ch = src[i]
        if ch == "\\" and i + 1 < n:
            # Escaped char (e.g. \` or \${): skip both so an escaped
            # interpolation is not mistaken for an unescaped one.
            i += 2
            continue
        if ch == "`":
            in_template = not in_template
            i += 1
            continue
        if in_template and ch == "$" and i + 1 < n and src[i + 1] == "{":
            body_start = i + 2
            j = body_start
            depth = 1
            while j < n and depth > 0:
                if src[j] == "{":
                    depth += 1
                elif src[j] == "}":
                    depth -= 1
                j += 1
            body = src[body_start:j - 1]
            if BASH_INTERP_RE.search(body):
                found.append((i, body))
            i = j
            continue
        i += 1
    return found


def check_file(path: Path) -> list[str]:
    """Return list of error strings for one QML file."""
    try:
        src = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        return [f"{path}: cannot read file: {exc}"]

    errors: list[str] = []
    for offset, body in _unescaped_interpolations(src):
        line = src.count("\n", 0, offset) + 1
        last_nl = src.rfind("\n", 0, offset)
        col = offset - last_nl
        errors.append(
            f"{path}:{line}:{col}: unescaped bash parameter expansion "
            f"${{{body}}} inside a template literal - escape it as \\${{{body}}}"
        )
    return errors


def main(argv: list[str]) -> int:
    if argv:
        targets: list[Path] = []
        for arg in argv:
            p = Path(arg)
            if not p.is_absolute():
                p = ROOT / p
            targets.append(p)
    else:
        targets = [ROOT]

    files: list[Path] = []
    for t in targets:
        if t.is_dir():
            files.extend(sorted(t.rglob("*.qml")))
        elif t.suffix == ".qml":
            files.append(t)

    print(f"{BOLD}=== QML template interpolation check ({len(files)} files) ==={RESET}")
    all_errors: list[str] = []
    for f in files:
        all_errors.extend(check_file(f))

    for err in all_errors:
        print(f"{RED}[ERR]{RESET}  {err}")

    print()
    if all_errors:
        print(f"{BOLD}{RED}{len(all_errors)} unescaped bash interpolation(s) found.{RESET}")
        return 1
    print(f"{BOLD}{GREEN}No unescaped bash interpolation in QML template literals.{RESET}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
