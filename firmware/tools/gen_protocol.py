#!/usr/bin/env python3
"""Landlink protocol codegen.

Reads firmware/protocol.yaml and emits:

  firmware/src/shared/protocol/opcodes.h       (C++17 constexpr + enums)
  firmware/src/shared/protocol/tlv_tags.h      (C++17 constexpr + enums)
  src/shared/protocol/landlink.ts              (TS enums + types)
  src/shared/protocol/uuids.ts                 (TS UUID constants)

No third-party deps beyond PyYAML. Run:

    python3 firmware/tools/gen_protocol.py

CI should run this and fail if the generated files differ from tracked ones.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.stderr.write("error: PyYAML not installed. Run: pip install pyyaml\n")
    sys.exit(1)


REPO_ROOT = Path(__file__).resolve().parents[2]
FIRMWARE_ROOT = REPO_ROOT / "firmware"
WEB_ROOT = REPO_ROOT / "src"

HEADER_BANNER = (
    "// GENERATED FILE — do not edit.\n"
    "// Source: firmware/protocol.yaml\n"
    "// Regenerate via: python3 firmware/tools/gen_protocol.py\n"
)

TS_BANNER = (
    "// GENERATED FILE — do not edit.\n"
    "// Source: firmware/protocol.yaml\n"
    "// Regenerate via: python3 firmware/tools/gen_protocol.py\n"
)


def load_spec(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return yaml.safe_load(f)


# ---------------------------------------------------------------------------
# C++ emission
# ---------------------------------------------------------------------------

def emit_opcodes_h(spec: dict) -> str:
    ops = spec["opcodes"]
    fsm = spec["fsm_states"]
    errs = spec["errors"]
    regions = spec["regions"]
    kinds = spec["mesh_kinds"]

    lines = [HEADER_BANNER, "#pragma once\n", "#include <cstdint>\n",
             "namespace landlink::proto {\n"]

    lines.append(f"inline constexpr uint8_t kProtoVersion = {spec['proto_version']};\n")
    lines.append("\nenum class Opcode : uint8_t {")
    for name, meta in ops.items():
        lines.append(f"    {name} = {meta['code']:#04x},")
    lines.append("};\n")

    lines.append("\nenum class FsmState : uint8_t {")
    for name, val in fsm.items():
        lines.append(f"    {name} = {val:#04x},")
    lines.append("};\n")

    lines.append("\nenum class MeshKind : uint8_t {")
    for name, val in kinds.items():
        lines.append(f"    {name} = {val:#04x},")
    lines.append("};\n")

    lines.append("\nenum class Region : uint8_t {")
    for name, val in regions.items():
        lines.append(f"    {name} = {val:#04x},")
    lines.append("};\n")

    lines.append("\nenum class ErrorCode : uint8_t {")
    for name, val in errs.items():
        lines.append(f"    {name} = {val:#04x},")
    lines.append("};\n")

    lines.append("\n} // namespace landlink::proto\n")
    return "\n".join(lines)


def emit_tlv_h(spec: dict) -> str:
    tags = spec["tlv_tags"]
    lines = [HEADER_BANNER, "#pragma once\n", "#include <cstdint>\n",
             "namespace landlink::proto {\n",
             "enum class TlvTag : uint8_t {"]
    for name, meta in tags.items():
        desc = meta.get("desc", "")
        comment = f"  // {desc}" if desc else ""
        lines.append(f"    {name} = {meta['tag']:#04x},{comment}")
    lines.append("};\n")
    lines.append("} // namespace landlink::proto\n")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# TypeScript emission
# ---------------------------------------------------------------------------

def emit_uuids_ts(spec: dict) -> str:
    ble = spec["ble"]
    svc = ble["service_uuid"].lower()
    lines = [TS_BANNER, ""]
    lines.append(f'export const LANDLINK_SERVICE_UUID = "{svc}" as const;')
    lines.append(f'export const LANDLINK_DEVICE_NAME_PREFIX = "{ble["device_name_prefix"]}" as const;')
    lines.append("")
    lines.append("export const LANDLINK_CHARACTERISTIC = {")
    for name, meta in ble["characteristics"].items():
        lines.append(f'  {name}: "{meta["uuid"].lower()}",')
    lines.append("} as const;")
    lines.append("")
    lines.append("export type LandlinkCharacteristicName = keyof typeof LANDLINK_CHARACTERISTIC;")
    lines.append("")
    return "\n".join(lines)


def emit_landlink_ts(spec: dict) -> str:
    ops = spec["opcodes"]
    fsm = spec["fsm_states"]
    tags = spec["tlv_tags"]
    kinds = spec["mesh_kinds"]
    regions = spec["regions"]
    errs = spec["errors"]

    def enum_block(name: str, items: dict, value_key: str | None = None) -> list[str]:
        out = [f"export const {name} = {{"]
        for k, v in items.items():
            val = v[value_key] if value_key else v
            out.append(f"  {k}: {val:#04x},")
        out.append("} as const;")
        out.append(f"export type {name}Name = keyof typeof {name};")
        out.append(f"export type {name}Value = (typeof {name})[{name}Name];")
        return out

    lines = [TS_BANNER, ""]
    lines.append(f"export const LANDLINK_PROTO_VERSION = {spec['proto_version']} as const;")
    lines.append("")
    lines.extend(enum_block("Opcode", ops, "code"))
    lines.append("")
    lines.extend(enum_block("FsmState", fsm))
    lines.append("")
    lines.extend(enum_block("TlvTag", tags, "tag"))
    lines.append("")
    lines.extend(enum_block("MeshKind", kinds))
    lines.append("")
    lines.extend(enum_block("Region", regions))
    lines.append("")
    lines.extend(enum_block("ErrorCode", errs))
    lines.append("")

    # Minimal framing helper interface. Written to pass the project's strict
    # TS options (noUncheckedIndexedAccess + verbatimModuleSyntax).
    lines.append("""export type BleFrame = {
  opcode: OpcodeValue;
  seq: number;
  payload: Uint8Array;
};

export type Tlv = { tag: TlvTagValue; value: Uint8Array };

export function encodeFrame(op: OpcodeValue, seq: number, payload: Uint8Array): Uint8Array {
  const len = payload.byteLength;
  const out = new Uint8Array(4 + len);
  out[0] = op;
  out[1] = seq & 0xff;
  out[2] = len & 0xff;
  out[3] = (len >> 8) & 0xff;
  out.set(payload, 4);
  return out;
}

export function decodeFrame(bytes: Uint8Array): BleFrame | null {
  if (bytes.byteLength < 4) return null;
  const opcode = (bytes[0] ?? 0) as OpcodeValue;
  const seq = bytes[1] ?? 0;
  const len = (bytes[2] ?? 0) | ((bytes[3] ?? 0) << 8);
  if (bytes.byteLength < 4 + len) return null;
  return { opcode, seq, payload: bytes.slice(4, 4 + len) };
}

export function encodeTlvs(tlvs: readonly Tlv[]): Uint8Array {
  let total = 0;
  for (const t of tlvs) total += 2 + t.value.byteLength;
  const out = new Uint8Array(total);
  let off = 0;
  for (const t of tlvs) {
    out[off++] = t.tag;
    out[off++] = t.value.byteLength & 0xff;
    out.set(t.value, off);
    off += t.value.byteLength;
  }
  return out;
}

export function decodeTlvs(bytes: Uint8Array): Tlv[] {
  const out: Tlv[] = [];
  let off = 0;
  while (off + 2 <= bytes.byteLength) {
    const tag = (bytes[off] ?? 0) as TlvTagValue;
    const len = bytes[off + 1] ?? 0;
    off += 2;
    if (off + len > bytes.byteLength) break;
    out.push({ tag, value: bytes.slice(off, off + len) });
    off += len;
  }
  return out;
}
""")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def write_if_changed(path: Path, content: str) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.write_text(content, encoding="utf-8")
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="exit 1 if any generated file would change")
    args = parser.parse_args()

    spec = load_spec(FIRMWARE_ROOT / "protocol.yaml")

    targets = [
        (FIRMWARE_ROOT / "src/shared/protocol/opcodes.h",   emit_opcodes_h(spec)),
        (FIRMWARE_ROOT / "src/shared/protocol/tlv_tags.h",  emit_tlv_h(spec)),
        (WEB_ROOT / "shared/protocol/landlink.ts",          emit_landlink_ts(spec)),
        (WEB_ROOT / "shared/protocol/uuids.ts",             emit_uuids_ts(spec)),
    ]

    changed = False
    for path, content in targets:
        if args.check:
            current = path.read_text(encoding="utf-8") if path.exists() else ""
            if current != content:
                sys.stderr.write(f"drift: {path}\n")
                changed = True
        else:
            if write_if_changed(path, content):
                print(f"wrote {path.relative_to(REPO_ROOT)}")

    if args.check and changed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
