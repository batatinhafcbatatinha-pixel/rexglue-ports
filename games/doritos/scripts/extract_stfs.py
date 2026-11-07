"""Extract the validated Doritos STFS file table without executing contents."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

FILES = (
    ("achievement01.png", 6129, 0x1),
    ("achievement02.png", 5701, 0x3),
    ("achievement03.png", 5940, 0x5),
    ("achievement04.png", 6855, 0x7),
    ("achievement05.png", 5690, 0x9),
    ("achievement06.png", 6261, 0xB),
    ("achievement07.png", 6043, 0xD),
    ("achievement08.png", 5171, 0xF),
    ("achievement09.png", 5805, 0x11),
    ("achievement10.png", 5668, 0x13),
    ("achievement11.png", 5377, 0x15),
    ("achievement12.png", 6164, 0x17),
    ("ArcadeInfo.xml", 2308, 0x19),
    ("data.pak", 55672832, 0x1A),
    ("default.xex", 4501504, 0x3532),
    ("game.png", 10634, 0x397D),
    ("gamerpicture1big.png", 6003, 0x3980),
    ("gamerpicture1small.png", 921, 0x3982),
    ("gamerpicture2big.png", 3136, 0x3983),
    ("gamerpicture2small.png", 2134, 0x3984),
    ("game_bkgnd.jpg", 459931, 0x3985),
    ("game_boxart.jpg", 199448, 0x39F6),
    ("game_marketplace.png", 78328, 0x3A27),
    ("kmrb_91.png", 4542, 0x3A3B),
)


def data_offset(block: int) -> int:
    block_base = 0xAA
    backing = block
    for _ in range(3):
        backing += (block + block_base) // block_base
        if block < block_base:
            break
        block_base *= 0xAA
    return 0xB000 + backing * 0x1000


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("package", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if args.destination.exists():
        raise SystemExit(f"destination already exists: {args.destination}")
    args.destination.mkdir(parents=True)
    records = []
    with args.package.open("rb") as source:
        for name, size, first_block in FILES:
            block_count = (size + 0xFFF) // 0x1000
            source.seek(data_offset(first_block))
            target = args.destination / name
            remaining = size
            with target.open("wb") as output:
                while remaining:
                    chunk = source.read(min(0x1000, remaining))
                    if not chunk:
                        raise IOError(f"unexpected end of package while reading {name}")
                    output.write(chunk)
                    remaining -= len(chunk)
                    if remaining:
                        source.seek(data_offset(first_block + (size - remaining) // 0x1000))
            records.append({
                "path": name,
                "size": size,
                "first_block": first_block,
                "block_count": block_count,
                "sha256": sha256(target),
            })
    (args.destination.parent / "manifest.json").write_text(
        json.dumps({"source": str(args.package), "files": records}, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps({"destination": str(args.destination), "files": records}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
