#!/usr/bin/env python3

import argparse
import pathlib
import struct


ICON = b"_ZN7SysTray13iconActivatedEN15QSystemTrayIcon16ActivationReasonE"
SHOW = b"_ZN7SysTray14showStyledMenuEv"
LAUNCH = b"_ZN7SysTray19sigLaunchPreferenceEv"
RAISE = b"_ZN7SysTray14sigRaiseWizardEv"

BASE = bytearray([
    0x55, 0x48, 0x89, 0xE5, 0x48, 0x83, 0xEC, 0x10,
    0x48, 0x89, 0x7D, 0xF8, 0x89, 0x75, 0xF4, 0x8B,
    0x45, 0xF4, 0x83, 0xF8, 0x03, 0x74, 0x3F, 0x83,
    0xF8, 0x04, 0x74, 0x3D, 0x83, 0xF8, 0x02, 0x74,
    0x02, 0xEB, 0x3A, 0x48, 0x8B, 0x45, 0xF8, 0x8B,
    0x40, 0x38, 0x83, 0xF8, 0x01, 0x75, 0x0E, 0x48,
    0x8B, 0x45, 0xF8, 0x48, 0x89, 0xC7, 0xE8, 0, 0,
    0, 0, 0xEB, 0x1F, 0x48, 0x8B, 0x45, 0xF8, 0x8B,
    0x40, 0x38, 0x85, 0xC0, 0x75, 0x14, 0x48, 0x8B,
    0x45, 0xF8, 0x48, 0x89, 0xC7, 0xE8, 0, 0, 0, 0,
    0xEB, 0x06, 0x90, 0xEB, 0x04, 0x90, 0xEB, 0x01,
    0x90, 0x90, 0xC9, 0xC3,
])

TEXT_OFFSET = 0x100
TEXT_ADDRESS = 0x1000
ICON_OFFSET = TEXT_OFFSET


def align(value, boundary):
    return (value + boundary - 1) & ~(boundary - 1)


def handler(state="unpatched"):
    result = bytearray(BASE)
    struct.pack_into("<i", result, 55, 0x1200 - (0x1000 + 59))
    struct.pack_into("<i", result, 80, 0x1240 - (0x1000 + 84))
    if state == "patched":
        result[27] = 0x42
        result[86:88] = b"\xC9\xE9"
        struct.pack_into("<i", result, 88, 0x1100 - (0x1000 + 92))
    return result


def elf_image(state="unpatched", duplicate=None):
    text = bytearray(b"\x90" * 0x300)
    text[:96] = handler(state)

    strings = bytearray(b"\0")
    name_offsets = {}
    for name in (ICON, SHOW, LAUNCH, RAISE):
        name_offsets[name] = len(strings)
        strings += name + b"\0"

    symbols = bytearray(24)
    for name, address, size in (
        (ICON, 0x1000, 96),
        (SHOW, 0x1100, 134),
        (LAUNCH, 0x1200, 44),
        (RAISE, 0x1240, 44),
    ):
        symbols += struct.pack("<IBBHQQ", name_offsets[name], 0x12, 0, 1, address, size)
    if duplicate is not None:
        symbols += struct.pack("<IBBHQQ", name_offsets[duplicate], 0x12, 0, 1, 0x1280, 44)

    section_names = b"\0.text\0.symtab\0.strtab\0.shstrtab\0"
    section_name_offsets = {
        name: section_names.index(name.encode())
        for name in (".text", ".symtab", ".strtab", ".shstrtab")
    }

    symtab_offset = align(TEXT_OFFSET + len(text), 8)
    strtab_offset = symtab_offset + len(symbols)
    shstrtab_offset = strtab_offset + len(strings)
    section_offset = align(shstrtab_offset + len(section_names), 8)

    header = struct.pack(
        "<16sHHIQQQIHHHHHH",
        b"\x7fELF\x02\x01\x01" + b"\0" * 9,
        3, 62, 1, 0, 0, section_offset, 0, 64, 56, 0, 64, 5, 4,
    )
    sections = bytearray(64)
    sections += struct.pack(
        "<IIQQQQIIQQ", section_name_offsets[".text"], 1, 6,
        TEXT_ADDRESS, TEXT_OFFSET, len(text), 0, 0, 16, 0,
    )
    sections += struct.pack(
        "<IIQQQQIIQQ", section_name_offsets[".symtab"], 2, 0,
        0, symtab_offset, len(symbols), 3, 1, 8, 24,
    )
    sections += struct.pack(
        "<IIQQQQIIQQ", section_name_offsets[".strtab"], 3, 0,
        0, strtab_offset, len(strings), 0, 0, 1, 0,
    )
    sections += struct.pack(
        "<IIQQQQIIQQ", section_name_offsets[".shstrtab"], 3, 0,
        0, shstrtab_offset, len(section_names), 0, 0, 1, 0,
    )

    image = bytearray(header)
    image += b"\0" * (TEXT_OFFSET - len(image))
    image += text
    image += b"\0" * (symtab_offset - len(image))
    image += symbols
    image += strings
    image += section_names
    image += b"\0" * (section_offset - len(image))
    image += sections
    return image


def write_home(home, state="unpatched", major="4"):
    app = pathlib.Path(home) / ".SynologyDrive/SynologyDrive.app"
    target = app / "bin/cloud-drive-ui"
    target.parent.mkdir(parents=True, exist_ok=True)
    (app / "INFO").write_text(f"[Version]\nmajor_version = {major}\n")
    target.write_bytes(elf_image(state))
    target.chmod(0o755)
    return target


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("home", type=pathlib.Path)
    parser.add_argument("state", choices=("unpatched", "patched"))
    args = parser.parse_args()
    write_home(args.home, args.state)


if __name__ == "__main__":
    main()
