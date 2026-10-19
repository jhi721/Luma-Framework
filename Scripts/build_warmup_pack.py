"""Builds a shader warm-up pack (Core "includes/shader_warmup.h") from folders of dumped shaders.

Usage: python build_warmup_pack.py <out .bin> [--only=<profile version, e.g. 5_0>] <dump dir> [<dump dir> ...]

Takes every "*.vs_5_0.cso", "*.ps_5_0.cso" and "*.cs_5_0.cso" (also "_4_0") of the folders, deduplicated by container MD5, and
every input layout the game created ("layout_*.bin", dumped by a DEVELOPMENT build while it dumps shaders), deduplicated by content.
The dumps must be the game's shaders as created (before Luma's patches, which the warm-up applies itself), e.g. a game's
"Shaders/<Game>/Dump" folder and, for a dgVoodoo game, what "_tools/dgv_hashgen" makes: "--pairs" every D3D9 shader, "--internals"
the wrapper's own. Merges into the pack when it exists.
"""

import pathlib
import struct
import sys

MAGIC = b"LSW1"
STAGES = {"vs": 0, "ps": 1, "cs": 3}  # Compute: the wrapper's own (dgVoodoo), after the vertex and pixel shaders
LAYOUT_STAGE = 2  # Last: a build without layouts stops reading at the first one


def read_pack(path):
    shaders, layouts = {}, {}
    if not path.exists():
        return shaders, layouts
    data = path.read_bytes()
    if data[:4] != MAGIC:
        sys.exit(f"{path} isn't a warm-up pack")
    count = struct.unpack_from("<I", data, 4)[0]
    offset = 8
    for _ in range(count):
        stage, size = struct.unpack_from("<II", data, offset)
        payload = data[offset + 8 : offset + 8 + size]
        if stage == LAYOUT_STAGE:
            layouts[payload] = None
        else:
            shaders[payload[4:20]] = (stage, payload)
        offset += 8 + size
    return shaders, layouts


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    out = pathlib.Path(sys.argv[1])
    folders = [arg for arg in sys.argv[2:] if not arg.startswith("--only=")]
    only = next((arg.split("=", 1)[1] for arg in sys.argv[2:] if arg.startswith("--only=")), None)
    shaders, layouts = read_pack(out)
    shaders_before, layouts_before = len(shaders), len(layouts)
    for folder in map(pathlib.Path, folders):
        for path in sorted(folder.glob("*.cso")):
            parts = path.name.split(".")
            if len(parts) < 3 or parts[-2][:2] not in STAGES or (only and not parts[-2].endswith(only)):
                continue
            code = path.read_bytes()
            if code[:4] != b"DXBC" or struct.unpack_from("<I", code, 24)[0] != len(code):
                print(f"skipped {path} (not a DXBC container)")
                continue
            shaders.setdefault(code[4:20], (STAGES[parts[-2][:2]], code))
        for path in sorted(folder.glob("layout_*.bin")):
            layouts.setdefault(path.read_bytes(), None)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "wb") as file:
        file.write(MAGIC + struct.pack("<I", len(shaders) + len(layouts)))
        for stage, code in sorted(shaders.values(), key=lambda entry: entry[0]):
            file.write(struct.pack("<II", stage, len(code)) + code)
        for layout in layouts:
            file.write(struct.pack("<II", LAYOUT_STAGE, len(layout)) + layout)
    counts = {name: sum(1 for stage, _ in shaders.values() if stage == value) for name, value in STAGES.items()}
    print(f"{out}: {len(shaders)} shaders ({counts['vs']} VS, {counts['ps']} PS, {counts['cs']} CS), {len(shaders) - shaders_before} new; {len(layouts)} layouts, {len(layouts) - layouts_before} new")


if __name__ == "__main__":
    main()
