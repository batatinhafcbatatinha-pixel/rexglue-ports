"""Read-only STFS package identity probe; never executes package contents."""

from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} PACKAGE", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        prefix = stream.read(0x500)
        digest.update(prefix)
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    if len(prefix) < 4:
        raise ValueError("package is shorter than the STFS magic")
    result = {
        "path": str(path),
        "size": path.stat().st_size,
        "sha256": digest.hexdigest(),
        "magic_ascii": prefix[:4].decode("ascii", errors="replace"),
        "header_0x320_hex": prefix[0x320:0x340].hex(),
        "title_utf16le_probe": prefix[0x410:0x510].decode("utf-16le", errors="ignore").split("\x00", 1)[0],
        "read_only": True,
        "execution": "not performed",
    }
    print(json.dumps(result, indent=2, ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
