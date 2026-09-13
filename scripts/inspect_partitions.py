"""Report the guest image's partition table and each partition's filesystem.

Reads bytes, never writes, and never converts: the last time this image was
converted with too little free space it took the host down. qemu-io serves
individual byte ranges straight out of the qcow2, so the largest read here is
seventeen kilobytes.
"""
import re, subprocess, sys, uuid

IMG = sys.argv[1]


def rd(off, length):
    out = subprocess.run(
        ["qemu-io", "-r", "-f", "qcow2", "-c", f"read -v {off} {length}", IMG],
        capture_output=True, text=True, check=True).stdout
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]{8}:\s+((?:[0-9a-f]{2} )+)", line)
        if m:
            data += bytes.fromhex(m.group(1).replace(" ", ""))
    return bytes(data[:length])


hdr = rd(512, 512)
assert hdr[:8] == b"EFI PART", "no GPT where one was expected"
entry_lba = int.from_bytes(hdr[72:80], "little")
count = int.from_bytes(hdr[80:84], "little")
size = int.from_bytes(hdr[84:88], "little")

table = rd(entry_lba * 512, count * size)
print(f"{'name':<16} {'offset':>14} {'bytes':>13}  filesystem")
for i in range(count):
    e = table[i * size:(i + 1) * size]
    if e[:16] == b"\0" * 16:
        continue
    first = int.from_bytes(e[32:40], "little") * 512
    last = int.from_bytes(e[40:48], "little") * 512 + 511
    name = e[56:128].decode("utf-16-le").rstrip("\0")

    head = rd(first, 2048)
    if head[1024:1028] == b"\xe2\xe1\xf5\xe0":
        fs = "EROFS (read-only; cannot be edited in place)"
    elif head[0x438:0x43a] == b"\x53\xef":
        fs = "ext4/ext2"
    elif head[:4] == b"\x41\x56\x42\x30" or head[:4] == b"AVB0":
        fs = "AVB vbmeta"
    elif head[54:59] == b"FAT12" or head[82:87] == b"FAT32":
        fs = "FAT"
    else:
        fs = "unrecognised (" + head[:8].hex() + ")"
    print(f"{name:<16} {first:>14} {last - first + 1:>13}  {fs}")


# --- logical partitions inside `super` -------------------------------------
#
# Android's dynamic partitions: system, vendor, product and system_ext do not
# appear in the GPT at all. They live inside `super`, described by liblp
# metadata, so finding out what filesystem /system carries means parsing that
# first. Still read-only, still a few kilobytes.
SUPER = 269484032
geo = rd(SUPER + 4096, 4096)
if int.from_bytes(geo[:4], "little") != 0x616C4467:
    print("\nsuper: no liblp geometry where one was expected")
    raise SystemExit
slot_size = int.from_bytes(geo[40:44], "little")
print(f"\nsuper: liblp geometry ok, metadata slot {slot_size} bytes")

meta = rd(SUPER + 4096 * 3, slot_size)
if int.from_bytes(meta[:4], "little") != 0x414C5030:
    print("super: no liblp header at the first metadata slot")
    raise SystemExit

tables = 80
part_off, part_num, part_sz = [int.from_bytes(meta[tables + i * 4:tables + i * 4 + 4], "little")
                               for i in range(3)]
ext_off, ext_num, ext_sz = [int.from_bytes(meta[tables + 12 + i * 4:tables + 12 + i * 4 + 4], "little")
                            for i in range(3)]
hdr_size = int.from_bytes(meta[8:12], "little")

extents = []
for i in range(ext_num):
    e = meta[hdr_size + ext_off + i * ext_sz:][:ext_sz]
    extents.append((int.from_bytes(e[0:8], "little"),      # sectors
                    int.from_bytes(e[12:20], "little")))   # target_data (sector)

print(f"{'logical':<16} {'offset in image':>16} {'bytes':>13}  filesystem")
for i in range(part_num):
    p = meta[hdr_size + part_off + i * part_sz:][:part_sz]
    name = p[:36].split(b"\0")[0].decode()
    first_ext = int.from_bytes(p[40:44], "little")
    n_ext = int.from_bytes(p[44:48], "little")
    if not n_ext:
        print(f"{name:<16} {'(empty)':>16}")
        continue
    sectors, start = extents[first_ext]
    off = SUPER + start * 512
    head = rd(off, 2048)
    if head[1024:1028] == b"\xe2\xe1\xf5\xe0":
        fs = "EROFS (read-only; needs a full repack to edit)"
    elif head[0x438:0x43a] == b"\x53\xef":
        fs = "ext4 (a single file can be added in place)"
    else:
        fs = "unrecognised (" + head[:8].hex() + ")"
    print(f"{name:<16} {off:>16} {sectors * 512:>13}  {fs}")
