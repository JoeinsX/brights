#!/usr/bin/env python3
import re
import sys
from pathlib import Path

INCLUDE_RE = re.compile(r'(#[ \t]*include[ \t]*")((?:\.\./)+[^"]*)(")')


def resolve_include(raw: str, file_dir: Path, src_root: Path) -> str | None:
    stripped = re.sub(r"^(?:\.\./)+", "", raw)
    for target in ((file_dir / raw).resolve(), src_root / stripped):
        if target.is_file() and target.is_relative_to(src_root):
            return target.relative_to(src_root).as_posix()
    return None


def fix(path: Path, src_root: Path) -> bool:
    text = path.read_bytes().decode("utf-8")

    def repl(m: re.Match) -> str:
        rel = resolve_include(m.group(2), path.parent, src_root)
        return m.group(1) + rel + m.group(3) if rel else m.group(0)

    new = INCLUDE_RE.sub(repl, text)
    if new == text:
        return False
    path.write_bytes(new.encode("utf-8"))
    return True


def main(argv: list[str]) -> None:
    src_root = (Path(__file__).resolve().parent.parent / "src").resolve()
    files = [Path(a) for a in argv] or [*src_root.rglob("*.hpp"), *src_root.rglob("*.cpp")]

    for path in files:
        path = path.resolve()
        if path.suffix not in (".hpp", ".cpp") or src_root not in path.parents:
            continue
        if fix(path, src_root):
            print(f"format-includes: rewrote {path.relative_to(src_root).as_posix()}")


if __name__ == "__main__":
    main(sys.argv[1:])
