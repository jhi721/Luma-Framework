"""PE header patches 32 bit D3D9 games commonly need under the dgVoodoo + Luma stack, with a backup of the untouched exe.

  python Scripts/patch_exe.py GAME.exe              show LAA, stack reserve and checksum, change nothing
  python Scripts/patch_exe.py GAME.exe --laa        set IMAGE_FILE_LARGE_ADDRESS_AWARE: 4 GB of VA instead of 2 under WOW64
                                                    (dgVoodoo + ReShade + Luma + DLSS exhaust 2 GB: ME1, Bayonetta, CoD2)
  python Scripts/patch_exe.py GAME.exe --stack 1M   raise SizeOfStackReserve (the default for every thread without its own size:
                                                    CoD2's 64 KB overflowed ReShade's threads, c00000fd)
  python Scripts/patch_exe.py --self-test

The first patch copies the exe to GAME.exe.bak-vanilla (kept if any GAME.exe.bak-* exists already), then the PE checksum is
recomputed. Patched exes are local test setups: record them in the game's NOTES.md, never ship them.
A SteamStub (.bind section) exe is refused: its stub checks the image; dump and patch it as in the exe-re skill instead."""

import argparse
import glob
import os
import shutil
import sys
import tempfile

import pefile

LARGE_ADDRESS_AWARE = 0x20


def describe(pe):
    return {"pe32+": pe.OPTIONAL_HEADER.Magic == 0x20B, "laa": bool(pe.FILE_HEADER.Characteristics & LARGE_ADDRESS_AWARE),
            "stack_reserve": pe.OPTIONAL_HEADER.SizeOfStackReserve, "checksum": pe.OPTIONAL_HEADER.CheckSum,
            "steamstub": any(section.Name.rstrip(b"\0") == b".bind" for section in pe.sections)}


def parse_size(text):
    units = {"K": 1 << 10, "M": 1 << 20}
    return int(text[:-1]) * units[text[-1].upper()] if text[-1].upper() in units else int(text, 0)


def patch(path, laa=False, stack=None):
    """(description after, backup path or None)"""
    pe = pefile.PE(data=open(path, "rb").read())
    if not laa and stack is None:
        return describe(pe), None
    if describe(pe)["steamstub"]:
        raise ValueError("SteamStub exe (.bind section): patch a dump of the running image instead")
    if laa:
        pe.FILE_HEADER.Characteristics |= LARGE_ADDRESS_AWARE
    if stack is not None:
        pe.OPTIONAL_HEADER.SizeOfStackReserve = stack
    pe.OPTIONAL_HEADER.CheckSum = pe.generate_checksum()
    backup = None
    if not glob.glob(glob.escape(path) + ".bak-*"):
        backup = path + ".bak-vanilla"
        shutil.copyfile(path, backup)
    pe.write(path)
    return describe(pe), backup


def self_test():
    root = tempfile.mkdtemp(prefix="patch-exe-selftest-")
    vanilla = os.path.join(os.environ["SystemRoot"], "SysWOW64", "where.exe")
    exe = os.path.join(root, "game.exe")
    shutil.copyfile(vanilla, exe)
    original = patch(exe)[0]
    assert not original["pe32+"] and patch(exe)[1] is None and patch(exe)[0] == original
    after, backup = patch(exe, laa=True, stack=1 << 20)
    assert backup and open(backup, "rb").read() == open(vanilla, "rb").read()
    reread = pefile.PE(exe)
    assert after["laa"] and after["stack_reserve"] == 1 << 20 and reread.OPTIONAL_HEADER.CheckSum == reread.generate_checksum() != 0, after
    reread.close()
    assert patch(exe, stack=2 << 20)[1] is None  # The vanilla backup is never overwritten
    assert parse_size("1M") == 1 << 20 and parse_size("0x10000") == 0x10000
    shutil.rmtree(root)
    print("self test passed")


def main():
    if "--self-test" in sys.argv:
        return self_test()
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("exe")
    parser.add_argument("--laa", action="store_true")
    parser.add_argument("--stack", type=parse_size)
    args = parser.parse_args()
    after, backup = patch(args.exe, args.laa, args.stack)
    if backup:
        print(f"vanilla copy: {backup}")
    print(f"LAA={after['laa']} stack_reserve={after['stack_reserve']} checksum=0x{after['checksum']:08X} "
          f"{'PE32+' if after['pe32+'] else 'PE32'}{' SteamStub' if after['steamstub'] else ''}")


if __name__ == "__main__":
    sys.exit(main())
