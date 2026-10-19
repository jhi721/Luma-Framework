"""Builds a shader warm-up pack (Core "includes/shader_warmup.h") from folders of dumped shaders.

Usage: python build_warmup_pack.py <out .bin> [--only=<profile version, e.g. 5_0>] <dump dir> [<dump dir> ...]

Takes every "*.vs_5_0.cso" and "*.ps_5_0.cso" (also "_4_0") of the folders, deduplicated by container MD5. The dumps must be the
game's shaders as created (before Luma's patches, which the warm-up applies itself), e.g. a game's "Shaders/<Game>/Dump" folder and
the translations "_tools/dgv_hashgen --pairs" makes of every D3D9 shader of a dgVoodoo game. Merges into the pack when it exists.
"""

import pathlib
import struct
import sys

MAGIC = b"LSW1"
STAGES = {"vs": 0, "ps": 1}


def read_pack(path):
    entries = {}
    if not path.exists():
        return entries
    data = path.read_bytes()
    if data[:4] != MAGIC:
        sys.exit(f"{path} isn't a warm-up pack")
    count = struct.unpack_from("<I", data, 4)[0]
    offset = 8
    for _ in range(count):
        stage, size = struct.unpack_from("<II", data, offset)
        code = data[offset + 8 : offset + 8 + size]
        entries[code[4:20]] = (stage, code)
        offset += 8 + size
    return entries


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out = pathlib.Path(sys.argv[1])
    folders = [arg for arg in sys.argv[2:] if not arg.startswith("--only=")]
    only = next((arg.split("=", 1)[1] for arg in sys.argv[2:] if arg.startswith("--only=")), None)
    entries = read_pack(out)
    before = len(entries)
    for folder in map(pathlib.Path, folders):
        for path in sorted(folder.glob("*.cso")):
            parts = path.name.split(".")
            if len(parts) < 3 or parts[-2][:2] not in STAGES or (only and not parts[-2].endswith(only)):
                continue
            code = path.read_bytes()
            if code[:4] != b"DXBC" or struct.unpack_from("<I", code, 24)[0] != len(code):
                print(f"skipped {path} (not a DXBC container)")
                continue
            entries.setdefault(code[4:20], (STAGES[parts[-2][:2]], code))
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "wb") as file:
        file.write(MAGIC + struct.pack("<I", len(entries)))
        for stage, code in entries.values():
            file.write(struct.pack("<II", stage, len(code)) + code)
    vertex = sum(1 for stage, _ in entries.values() if stage == 0)
    print(f"{out}: {len(entries)} shaders ({vertex} VS, {len(entries) - vertex} PS), {len(entries) - before} new")


if __name__ == "__main__":
    main()
