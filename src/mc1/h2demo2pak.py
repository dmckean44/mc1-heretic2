#!/usr/bin/env python3
"""Packs the base folder of the Loki Heretic II demo into base/Htic2-0.pak.

The Loki release stores almost every file gzip-compressed and relies on its engine to unpack them.
This unpacks them, lowercases the names and writes one Quake II style pak, which also makes file
lookups case-insensitive. Videos are left out.

usage: h2demo2pak.py DEMO_BASE_DIR OUTPUT_DATA_DIR
"""

import gzip
import os
import struct
import sys

src, dst = sys.argv[1], sys.argv[2]

entries = []
for dirpath, _, filenames in os.walk(src):
    for filename in filenames:
        path = os.path.join(dirpath, filename)
        name = os.path.relpath(path, src).replace(os.sep, "/").lower()
        if name.startswith("video/"):
            continue
        if len(name) >= 56:
            sys.exit(f"name too long for a pak: {name}")

        with open(path, "rb") as f:
            data = f.read()
        if data[:2] == b"\x1f\x8b":
            data = gzip.decompress(data)

        entries.append((name, data))

entries.sort()

os.makedirs(os.path.join(dst, "base"), exist_ok=True)
pak = os.path.join(dst, "base", "Htic2-0.pak")

with open(pak, "wb") as out:
    out.write(b"\0" * 12)

    directory = []
    for name, data in entries:
        directory.append(struct.pack("<56sii", name.encode(), out.tell(), len(data)))
        out.write(data)

    dirofs = out.tell()
    out.write(b"".join(directory))
    out.seek(0)
    out.write(struct.pack("<4sii", b"PACK", dirofs, 64 * len(directory)))

print(f"{pak}: {len(entries)} files, {os.path.getsize(pak) // 1024} KB")
