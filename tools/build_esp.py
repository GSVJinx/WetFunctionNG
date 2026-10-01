"""Build plugin/WetFunctionNG.esp: an ESL-flagged plugin with one start-game-enabled quest carrying the
legacy-named WetFunctionNGMCM script. The script is now only a SexLab event bridge; configuration is native
through SKSE Menu Framework and has no MCM Helper or SkyUI dependency. Every record is form version 44.

Usage: python tools/build_esp.py
"""

import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "plugin" / "WetFunctionNG.esp"

FORM_VERSION = 44
FLAG_LIGHT = 0x200
QUEST_ID = 0x01000800  # load-order slot 01 = this plugin (one master)
PLAYER_REF = 0x00000014


def sub(sig, data):
    return sig.encode("ascii") + struct.pack("<H", len(data)) + data


def zstring(text):
    return text.encode("ascii") + b"\0"


def wstring(text):
    raw = text.encode("ascii")
    return struct.pack("<H", len(raw)) + raw


def record(sig, form_id, data, flags=0):
    return sig.encode("ascii") + struct.pack("<IIIIHH", len(data), flags, form_id, 0, FORM_VERSION, 0) + data


def group(label, payload):
    return b"GRUP" + struct.pack("<I4siHHI", 24 + len(payload), label.encode("ascii"), 0, 0, 0, 0) + payload


def script_block(name):
    # VMAD object: version 5, object format 2, one script, no properties
    return struct.pack("<hhH", 5, 2, 1) + wstring(name) + b"\0" + struct.pack("<H", 0)


def quest_vmad():
    data = script_block("WetFunctionNGMCM")
    data += b"\x02"                        # quest fragment data version
    data += struct.pack("<H", 0)           # fragment count
    data += struct.pack("<H", 0)           # fragment file name (empty)
    data += struct.pack("<H", 1)           # alias count
    data += struct.pack("<hhI", 0, 0, QUEST_ID)  # alias object: unused, alias id 0, owning quest
    data += script_block("WetFunctionNGPlayerAlias")
    return data


def build():
    quest = b"".join([
        sub("EDID", zstring("WetFunctionNG_MCM")),
        sub("VMAD", quest_vmad()),
        sub("FULL", zstring("WetFunction NG")),
        sub("DNAM", bytes.fromhex("010100d10000000000000000")),
        sub("NEXT", b""),
        sub("ANAM", struct.pack("<I", 1)),
        sub("ALST", struct.pack("<I", 0)),
        sub("ALID", zstring("PlayerAlias")),
        sub("FNAM", struct.pack("<I", 0)),
        sub("ALFR", struct.pack("<I", PLAYER_REF)),
        sub("VTCK", struct.pack("<I", 0)),
        sub("ALED", b""),
    ])
    body = group("QUST", record("QUST", QUEST_ID, quest))

    header = b"".join([
        sub("HEDR", struct.pack("<fII", 1.71, 2, 0x801)),  # version, records + groups, next object id
        sub("CNAM", zstring("WetFunctionNG")),
        sub("SNAM", zstring("WetFunction NG - native SKSE port of Wet Function Redux")),
        sub("MAST", zstring("Skyrim.esm")),
        sub("DATA", struct.pack("<Q", 0)),
        sub("INTV", struct.pack("<I", 1)),
    ])
    data = record("TES4", 0, header, FLAG_LIGHT) + body
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(data)
    print(f"wrote {OUT} ({len(data)} bytes)")


if __name__ == "__main__":
    build()
