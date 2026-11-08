#!/usr/bin/env python3
import pathlib, struct, sys
out = pathlib.Path(sys.argv[1])
items = sys.argv[2:]
lines = ["#pragma once", "#include <cstdint>", "#include <cstddef>", "namespace aromafg::embedded {"]
for item in items:
    name, path = item.split("=", 1)
    data = pathlib.Path(path).read_bytes()
    if len(data) % 4:
        raise SystemExit(f"{path}: SPIR-V byte count is not divisible by four")
    words = struct.unpack("<" + "I" * (len(data)//4), data)
    lines.append(f"inline constexpr uint32_t {name}[] = {{")
    for i in range(0, len(words), 8):
        lines.append("  " + ", ".join(f"0x{x:08x}u" for x in words[i:i+8]) + ",")
    lines.append("};")
    lines.append(f"inline constexpr size_t {name}_bytes = sizeof({name});")
lines.append("}")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text("\n".join(lines) + "\n")
