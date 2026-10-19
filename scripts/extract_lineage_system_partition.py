#!/usr/bin/env python3
"""Extract Lineage's logical /system partition from Husk's vda qcow2 image."""
import os
import struct
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: extract_lineage_system_partition.py <vda.qcow2|vda.raw> <system.img>")
    image, output = sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="husk-lineage-") as work:
        raw = image
        if image.endswith(".qcow2"):
            raw = os.path.join(work, "vda.raw")
            subprocess.run(["qemu-img", "convert", "-p", "-O", "raw", image, raw], check=True)

        table = subprocess.run(["gpt", "-r", "show", raw], check=True,
                               capture_output=True, text=True).stdout
        super_starts = []
        for line in table.splitlines():
            fields = line.split()
            if len(fields) >= 4 and fields[2:4] == ["2", "GPT"]:
                super_starts.append(int(fields[0]))
        if len(super_starts) != 1:
            raise RuntimeError("could not identify the Lineage super partition in the GPT")

        base = super_starts[0] * 512
        temporary_output = output + ".part"
        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        try:
            with open(raw, "rb") as source:
                source.seek(base + 12288)
                header = source.read(256)
                magic, _, _, header_size = struct.unpack_from("<IHHI", header, 0)
                if magic != 0x414C5030:
                    raise RuntimeError("super partition has no Android logical-partition metadata")

                offset = 12 + 32
                tables_size = struct.unpack_from("<I", header, offset)[0]
                offset += 4 + 32
                descriptors = [struct.unpack_from("<III", header, offset + 12 * i) for i in range(4)]
                source.seek(base + 12288 + header_size)
                tables = source.read(tables_size)
                partitions, extents = descriptors[0], descriptors[1]
                partition_offset, partition_count, partition_stride = partitions
                extent_offset, extent_count, extent_stride = extents
                extent_table = [struct.unpack_from("<QIQI", tables, extent_offset + extent_stride * i)
                                for i in range(extent_count)]

                written_system = False
                with open(temporary_output, "wb") as destination:
                    for index in range(partition_count):
                        entry = tables[partition_offset + partition_stride * index:
                                       partition_offset + partition_stride * (index + 1)]
                        name = entry[:36].split(b"\0", 1)[0].decode("utf-8")
                        _, first_extent, extent_count_for_partition, _ = struct.unpack_from("<IIII", entry, 36)
                        if name != "system":
                            continue
                        for extent in extent_table[first_extent:first_extent + extent_count_for_partition]:
                            sectors, _, data_sector, _ = extent
                            source.seek(base + data_sector * 512)
                            remaining = sectors * 512
                            while remaining:
                                chunk = source.read(min(remaining, 64 << 20))
                                if not chunk:
                                    raise RuntimeError("unexpected end of vda image while extracting /system")
                                destination.write(chunk)
                                remaining -= len(chunk)
                        written_system = True
                        break
                if not written_system:
                    raise RuntimeError("no logical system partition was found")
            os.replace(temporary_output, output)
        finally:
            if os.path.exists(temporary_output):
                os.unlink(temporary_output)
    print(f"Extracted Lineage /system to {output} ({os.path.getsize(output)} bytes)")


if __name__ == "__main__":
    main()
