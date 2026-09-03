#!/usr/bin/env python3

import hashlib
import os
import pathlib
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

from tray_patch_fixture import ICON, ICON_OFFSET, elf_image, write_home


USAGE = b"usage: synodrive-tray-patch status|apply|restore\n"
APPLY_NOTICE = b"synodrive-tray-patch: restart Synology Drive Client to load the tray patch\n"
RESTORE_NOTICE = b"synodrive-tray-patch: restart Synology Drive Client to remove the tray patch\n"


def run(binary, home, *args, fail=None):
    environment = os.environ.copy()
    environment["HOME"] = str(home)
    if fail is None:
        environment.pop("SYNODRIVE_TRAY_PATCH_TEST_FAIL", None)
    else:
        environment["SYNODRIVE_TRAY_PATCH_TEST_FAIL"] = fail
    return subprocess.run([binary, *args], capture_output=True, env=environment, check=False)


def digest(path):
    return hashlib.sha256(path.read_bytes()).digest()


def metadata(path):
    value = path.stat()
    return value.st_uid, value.st_gid, stat.S_IMODE(value.st_mode)


def assert_failure(result):
    assert result.returncode == 1, result
    assert result.stdout == b"", result
    assert result.stderr.startswith(b"synodrive-tray-patch: "), result


def test_transitions(release, testing, root):
    target = write_home(root)
    original = target.read_bytes()
    original_metadata = metadata(target)

    for arguments in [(), ("unknown",), ("status", "extra")]:
        result = run(release, root, *arguments)
        assert (result.returncode, result.stdout, result.stderr) == (2, b"", USAGE), result

    result = run(release, root, "status")
    assert (result.returncode, result.stdout, result.stderr) == (0, b"unpatched\n", b""), result
    assert target.read_bytes() == original

    result = run(release, root, "apply")
    assert (result.returncode, result.stdout, result.stderr) == (0, b"patched\n", APPLY_NOTICE), result
    patched = target.read_bytes()
    changed = {index for index, pair in enumerate(zip(original, patched)) if pair[0] != pair[1]}
    assert changed == {ICON_OFFSET + 27, *range(ICON_OFFSET + 86, ICON_OFFSET + 92)}, changed
    assert metadata(target) == original_metadata

    result = run(release, root, "apply")
    assert (result.returncode, result.stdout, result.stderr) == (0, b"patched\n", b""), result
    assert target.read_bytes() == patched
    result = run(release, root, "status")
    assert (result.returncode, result.stdout, result.stderr) == (0, b"patched\n", b""), result

    result = run(release, root, "restore")
    assert (result.returncode, result.stdout, result.stderr) == (0, b"unpatched\n", RESTORE_NOTICE), result
    assert target.read_bytes() == original
    assert metadata(target) == original_metadata
    result = run(release, root, "restore")
    assert (result.returncode, result.stdout, result.stderr) == (0, b"unpatched\n", b""), result

    image = pathlib.Path(release).read_bytes()
    assert b"SYNODRIVE_TRAY_PATCH_TEST_FAIL" not in image
    assert run(release, root, "status", str(target)).returncode == 2

    for point in ("write", "rename"):
        before = target.read_bytes()
        result = run(testing, root, "apply", fail=point)
        assert_failure(result)
        assert target.read_bytes() == before
        assert list(target.parent.glob("cloud-drive-ui.synodrive-dolphin.*")) == []

    before = target.read_bytes()
    externally_changed = bytes([before[0] ^ 1]) + before[1:]
    result = run(testing, root, "apply", fail="target-change")
    assert_failure(result)
    assert target.read_bytes() == externally_changed
    assert list(target.parent.glob("cloud-drive-ui.synodrive-dolphin.*")) == []
    target.write_bytes(before)

    result = run(testing, root, "apply", fail="directory-fsync")
    assert_failure(result)
    assert b"target changed, but directory synchronization failed; run status before retrying\n" in result.stderr
    assert run(release, root, "status").stdout == b"patched\n"
    assert run(release, root, "restore").returncode == 0


def test_rejections(release, root):
    target = write_home(root)
    info = target.parent.parent / "INFO"
    baseline = target.read_bytes()

    for contents in ("[Version]\nmajor_version = 3\n", "[Version]\nmajor_version = 5\n", "broken\n"):
        before = target.read_bytes(), metadata(target)
        info.write_text(contents)
        result = run(release, root, "apply")
        assert_failure(result)
        assert (target.read_bytes(), metadata(target)) == before
    info.write_text("[Version]\nmajor_version = 4\n")
    info.unlink()
    assert_failure(run(release, root, "apply"))
    assert target.read_bytes() == baseline
    info.write_text("[Version]\nmajor_version = 4\n")

    variants = []
    for offset, value in ((4, 1), (5, 2), (16, 2), (18, 3)):
        image = bytearray(baseline)
        image[offset] = value
        variants.append(image)
    variants.extend((baseline[:63], baseline[:-20]))
    changed_handler = bytearray(baseline)
    changed_handler[ICON_OFFSET + 40] ^= 1
    variants.append(changed_handler)
    partial_patch = bytearray(baseline)
    partial_patch[ICON_OFFSET + 27] = 0x42
    variants.append(partial_patch)
    missing_symbol = bytearray(baseline)
    location = missing_symbol.find(b"_ZN7SysTray14showStyledMenuEv")
    missing_symbol[location] = ord("X")
    variants.append(missing_symbol)

    section_offset = struct.unpack_from("<Q", baseline, 40)[0]
    symtab_offset = struct.unpack_from("<Q", baseline, section_offset + 2 * 64 + 24)[0]
    variants.append(elf_image(duplicate=ICON))
    wrong_section = bytearray(baseline)
    struct.pack_into("<H", wrong_section, symtab_offset + 24 + 6, 3)
    variants.append(wrong_section)
    wrong_size = bytearray(baseline)
    struct.pack_into("<Q", wrong_size, symtab_offset + 24 + 16, 95)
    variants.append(wrong_size)

    distant_show = bytearray(baseline)
    struct.pack_into("<H", distant_show, 60, 6)
    struct.pack_into("<H", distant_show, symtab_offset + 2 * 24 + 6, 5)
    struct.pack_into("<Q", distant_show, symtab_offset + 2 * 24 + 8, 0x90000000)
    distant_show += struct.pack(
        "<IIQQQQIIQQ", 1, 1, 6, 0x90000000,
        ICON_OFFSET + 0x100, 134, 0, 0, 16, 0,
    )
    variants.append(distant_show)

    for image in variants:
        target.write_bytes(image)
        before = digest(target), metadata(target)
        result = run(release, root, "apply")
        assert_failure(result)
        assert (digest(target), metadata(target)) == before

    target.unlink()
    target.symlink_to(root / "elsewhere")
    (root / "elsewhere").write_bytes(baseline)
    before = digest(root / "elsewhere")
    assert_failure(run(release, root, "apply"))
    assert digest(root / "elsewhere") == before
    target.unlink()
    assert_failure(run(release, root, "apply"))


def cmake(source, build, home, *arguments):
    environment = os.environ.copy()
    if home is None:
        environment.pop("HOME", None)
    else:
        environment["HOME"] = home
    return subprocess.run(
        ["cmake", *arguments], capture_output=True, env=environment, check=False,
        cwd=source,
    )


def test_source_install(source, root):
    build = root / "source-build"
    prefix = root / "prefix"
    configured_home = root / 'configured-"\\]=]home'
    installer_home = root / "installer-home"
    configured_target = write_home(configured_home)
    installer_target = write_home(installer_home)
    installer_before = installer_target.read_bytes()

    result = cmake(
        source, build, str(configured_home), "-S", str(source), "-B", str(build),
        "-G", "Ninja", "-DBUILD_TESTING=OFF", f"-DCMAKE_INSTALL_PREFIX={prefix}",
        "-DSYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL=ON",
    )
    assert result.returncode == 0, result.stderr.decode()
    result = cmake(source, build, str(installer_home), "--build", str(build))
    assert result.returncode == 0, result.stderr.decode()
    result = cmake(source, build, str(installer_home), "--install", str(build))
    assert result.returncode == 0, result.stderr.decode()
    assert configured_target.read_bytes() == elf_image("patched")
    assert installer_target.read_bytes() == installer_before

    configured_target.write_bytes(elf_image("unpatched"))
    before = configured_target.read_bytes()
    environment = os.environ.copy()
    environment.update(HOME=str(installer_home), DESTDIR=str(root / "stage-on"))
    result = subprocess.run(
        ["cmake", "--install", str(build)], capture_output=True, env=environment, check=False,
    )
    assert result.returncode != 0, result
    assert configured_target.read_bytes() == before
    assert installer_target.read_bytes() == installer_before

    result = cmake(
        source, build, str(configured_home), "-S", str(source), "-B", str(build),
        "-DSYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL=OFF",
    )
    assert result.returncode == 0, result.stderr.decode()
    configured_target.write_bytes(elf_image("patched"))
    configured_before = configured_target.read_bytes()
    environment["DESTDIR"] = str(root / "stage-off")
    result = subprocess.run(
        ["cmake", "--install", str(build)], capture_output=True, env=environment, check=False,
    )
    assert result.returncode == 0, result.stderr.decode()
    assert configured_target.read_bytes() == configured_before
    assert installer_target.read_bytes() == installer_before

    for index, invalid_home in enumerate((None, "", "relative")):
        invalid_build = root / f"invalid-build-{index}"
        result = cmake(
            source, invalid_build, invalid_home, "-S", str(source), "-B", str(invalid_build),
            "-G", "Ninja", "-DBUILD_TESTING=OFF",
            "-DSYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL=ON",
        )
        assert result.returncode != 0, (invalid_home, result)


def main():
    release = pathlib.Path(sys.argv[1]).resolve()
    testing = pathlib.Path(sys.argv[2]).resolve()
    source = pathlib.Path(sys.argv[3]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        test_transitions(release, testing, root / "transitions")
        test_rejections(release, root / "rejections")
        test_source_install(source, root / "install")


if __name__ == "__main__":
    main()
