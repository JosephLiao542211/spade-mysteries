"""List the largest tensors in a GGUF file (works on a partial download: only the header is read).

Usage: python -I Tools/llm-harness/gguf_tensors.py <file.gguf[.part]> [total_file_size_bytes]
Used to find which tensors to keep in system RAM with llama-server --override-tensor.
"""

import struct
import sys
from collections import defaultdict


def read_str(f):
    (n,) = struct.unpack("<Q", f.read(8))
    return f.read(n).decode("utf-8", "replace")


SCALAR = {0: "<B", 1: "<b", 2: "<H", 3: "<h", 4: "<I", 5: "<i", 6: "<f", 7: "<?", 10: "<Q", 11: "<q", 12: "<d"}


def read_val(f, t):
    if t in SCALAR:
        fmt = SCALAR[t]
        return struct.unpack(fmt, f.read(struct.calcsize(fmt)))[0]
    if t == 8:
        return read_str(f)
    if t == 9:
        (et,) = struct.unpack("<I", f.read(4))
        (n,) = struct.unpack("<Q", f.read(8))
        return [read_val(f, et) for _ in range(n)]
    raise ValueError(f"unknown GGUF value type {t}")


def main():
    path = sys.argv[1]
    total = int(sys.argv[2]) if len(sys.argv) > 2 else None
    with open(path, "rb") as f:
        assert f.read(4) == b"GGUF", "not a GGUF file"
        version, n_tensors, n_kv = struct.unpack("<IQQ", f.read(20))
        kv = {}
        for _ in range(n_kv):
            k = read_str(f)
            (t,) = struct.unpack("<I", f.read(4))
            v = read_val(f, t)
            kv[k] = v if not isinstance(v, list) or len(v) < 16 else f"[{len(v)} items]"
        tensors = []
        for _ in range(n_tensors):
            name = read_str(f)
            (nd,) = struct.unpack("<I", f.read(4))
            dims = struct.unpack(f"<{nd}Q", f.read(8 * nd))
            gtype, off = struct.unpack("<IQ", f.read(12))
            tensors.append([name, dims, gtype, off, 0])
        align = kv.get("general.alignment", 32)
        data_start = (f.tell() + align - 1) // align * align

    tensors.sort(key=lambda t: t[3])
    for i, t in enumerate(tensors):
        nxt = tensors[i + 1][3] if i + 1 < len(tensors) else ((total - data_start) if total else t[3])
        t[4] = nxt - t[3]

    print(f"GGUF v{version}, {n_tensors} tensors, arch={kv.get('general.architecture')}, name={kv.get('general.name')}")
    groups = defaultdict(int)
    for name, dims, gtype, off, size in tensors:
        key = name.split(".")[0] if not name.startswith("blk.") else "blk.*." + ".".join(name.split(".")[2:])
        groups[key] += size
    print("\nLargest tensor groups (MB):")
    for k, v in sorted(groups.items(), key=lambda kv: -kv[1])[:15]:
        print(f"  {v / 1e6:9.1f}  {k}")
    print("\nLargest single tensors:")
    for name, dims, gtype, off, size in sorted(tensors, key=lambda t: -t[4])[:8]:
        print(f"  {size / 1e6:9.1f} MB  {name}  dims={dims} type={gtype}")


if __name__ == "__main__":
    main()
