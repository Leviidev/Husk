"""Flip Husk's two GRUB toggles inside a GRUB environment block.

A GRUB environment block is exactly 1024 bytes: a header comment, then
`name=value` lines, then `#` padding out to the end. grub-editenv rewrites the
whole block to keep that size, so anything editing it by hand has to do the
same -- a block of the wrong length is not read back.

Both edits here replace a single `0` with a single `1`, so the byte length is
unchanged and the padding never has to move. That is deliberate: same-length
substitution is the one form of edit that cannot corrupt the block.
"""
import sys

TOGGLES = [
    # Clears every SELinux denial standing between Husk and a saveable GPU
    # machine -- ctl.stop from the bridge above all.
    (b"android_selinux_permissive=0", b"android_selinux_permissive=1"),
    # The boot animation ran for 87.8 seconds in the last cold boot, drawing a
    # logo nobody is waiting to admire.
    (b"android_nobootanim=0", b"android_nobootanim=1"),
]

SIZE = 1024


def verify(path):
    data = open(path, "rb").read()
    if len(data) != SIZE:
        sys.exit(f"grubenv is {len(data)} bytes, expected {SIZE}")
    for _, want in TOGGLES:
        if want not in data:
            sys.exit(f"missing {want.decode()}")
        print(f"    {want.decode()}")
    print(f"    block is {len(data)} bytes")


def main():
    if sys.argv[1] == "--verify":
        verify(sys.argv[2])
        return

    src, dst = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()
    if len(data) != SIZE:
        sys.exit(f"grubenv is {len(data)} bytes, expected {SIZE}")

    for old, new in TOGGLES:
        if new in data:
            print(f"    already set: {new.decode()}")
            continue
        if old not in data:
            sys.exit(f"cannot find {old.decode()} to change")
        if len(old) != len(new):
            sys.exit("replacement changes length; padding would have to move")
        data = data.replace(old, new, 1)
        print(f"    {old.decode()} -> {new.decode()}")

    if len(data) != SIZE:
        sys.exit(f"edit changed the block to {len(data)} bytes")
    open(dst, "wb").write(data)


if __name__ == "__main__":
    main()
