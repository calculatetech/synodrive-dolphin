#!/usr/bin/env python3

import os
import pathlib
import select
import shutil
import subprocess
import sys
import tempfile


def run(binary, env, *args, data=None):
    return subprocess.run(
        [binary, *args], input=data, capture_output=True, env=env, check=False
    )


def read_frame(process):
    data = bytearray()
    while True:
        byte = process.stdout.read(1)
        assert byte, "wrapper ended before a complete response"
        if byte == b"\0":
            return bytes(data)
        data.extend(byte)


def write_info(path, major="4", extra=""):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "[Package]\n"
        "installer = deb\n\n"
        "[Version]\n"
        f"major_version = {major}\n"
        "minor_version = 0\n"
        "mini_version = 2\n"
        "build_version = 17889\n"
        f"{extra}"
    )


def assert_status(binary, env, status):
    result = run(binary, env, "/tmp")
    assert (result.returncode, result.stdout) == (0, status + b"\n"), result
    result = run(binary, env, "--stdio", data=b"/tmp\0")
    assert (result.returncode, result.stdout) == (0, status + b"\0"), result


def assert_failure(binary, env):
    result = run(binary, env, "/tmp")
    assert result.returncode == 1 and not result.stdout and result.stderr, result
    result = run(binary, env, "--stdio", data=b"/tmp\0")
    assert (result.returncode, result.stdout) == (0, b"error\0"), result


def check_release_binary(binary, helper, env, control, root):
    image = binary.read_bytes()
    assert b"/opt/Synology/SynologyDrive/INFO" in image
    assert b"libnautilus-extension.so.4" in image
    for forbidden in [
        b"dpkg-query",
        b"SYNODRIVE_STATUS_TEST_INFO",
        b"package/cloudstation/INFO",
        b"icon-overlay/INFO",
        b"libnautilus-extension.so.3",
    ]:
        assert forbidden not in image, forbidden

    if os.environ.get("SYNODRIVE_CONTAINER_RELEASE_TEST") != "1":
        return

    product = pathlib.Path("/opt/Synology/SynologyDrive")
    assert os.geteuid() == 0 and not product.exists()
    top = product / "INFO"
    deeper = product / "package/cloudstation/INFO"
    overlay_info = product / "icon-overlay/INFO"
    runtime = root / "runtime"
    runtime.mkdir()
    shutil.copy2(helper, runtime / "libnautilus-extension.so.4")

    release_env = env.copy()
    release_env.pop("SYNODRIVE_STATUS_TEST_INFO", None)
    release_env.pop("SYNODRIVE_STATUS_TEST_NAUTILUS", None)
    release_env["LD_LIBRARY_PATH"] = str(runtime)
    control.write_text("1")

    write_info(top, "4")
    write_info(deeper, "3")
    overlay_info.parent.mkdir(parents=True)
    overlay_info.write_text('{"major_version": 3}\n')
    assert_status(binary, release_env, b"synced")

    write_info(top, "3")
    write_info(deeper, "4")
    overlay_info.write_text('{"major_version": 4}\n')
    assert_failure(binary, release_env)


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    release_binary = pathlib.Path(sys.argv[2]).resolve()
    helper = pathlib.Path(sys.argv[3]).resolve()
    missing_symbol_helper = pathlib.Path(sys.argv[4]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        overlay = root / ".SynologyDrive/SynologyDrive.app/icon-overlay"
        library = overlay / "15/lib"
        library.mkdir(parents=True)
        shutil.copy2(helper, library / "plugin-cb-4.so")
        (overlay / "14").mkdir()
        (overlay / "current").symlink_to("15")
        control = root / "status"
        pid_log = root / "pids"
        info = root / "INFO"
        write_info(info)

        env = os.environ.copy()
        env.update(
            HOME=str(root),
            SYNODRIVE_STATUS_TEST_INFO=str(info),
            SYNODRIVE_STATUS_TEST_NAUTILUS=str(helper),
            FAKE_SYNODRIVE_CONTROL=str(control),
            FAKE_SYNODRIVE_PID_LOG=str(pid_log),
        )

        for args in [(), ("relative",), ("/tmp", "extra")]:
            result = run(binary, env, *args)
            assert result.returncode == 2, (args, result)
            assert not result.stdout

        control.write_text("1")
        assert_status(binary, env, b"synced")
        info.write_bytes(
            b"; installed client\r\n"
            b"[Package]\r\ninstaller = deb\r\n"
            b"[Version]\r\nmajor_version = 004\r\nminor_version = 99\r\n"
            b"mini_version = 7\r\nbuild_version = 99999\r\n"
            b"[Future]\r\nvalue = a=b\r\n"
        )
        assert_status(binary, env, b"synced")

        malformed = [
            None,
            "[Package]\ninstaller = deb\n",
            "[Version]\nminor_version = 0\n",
            "[Version]\nmajor_version = \n",
            "[Version]\nmajor_version = four\n",
            "[Version]\nmajor_version = 4x\n",
            "[Version]\nmajor_version = +4\n",
            "[Version]\nmajor_version = -4\n",
            "[Version]\nmajor_version = 999999999999999999999999\n",
            "[Version]\nmajor_version = 4\n[Version]\nminor_version = 1\n",
            "[Version]\nmajor_version = 4\nmajor_version = 4\n",
            "[Package]\nbroken\n[Version]\nmajor_version = 4\n",
            "[Package\ninstaller = deb\n[Version]\nmajor_version = 4\n",
            "[Version][Other]\nmajor_version = 4\n",
            "major_version = 4\n",
            "[]\nkey = value\n[Version]\nmajor_version = 4\n",
            "[Version]\nmajor_version = 3\n",
            "[Version]\nmajor_version = 5\n",
        ]
        for contents in malformed:
            if contents is None:
                info.unlink()
            else:
                info.write_text(contents)
            assert_failure(binary, env)
        write_info(info)

        names = [b"unknown", b"synced", b"syncing", b"unsupported",
                 b"read-only", b"no-permission"]
        strange_path = "/tmp/a path\nwith newline"
        for raw, expected in enumerate(names):
            control.write_text(str(raw))
            assert_status(binary, env, expected)
        result = run(binary, env, strange_path)
        assert (result.returncode, result.stdout) == (0, b"no-permission\n"), result

        control.write_text("6")
        assert_failure(binary, env)

        control.write_text("1")
        failing = env | {"FAKE_SYNODRIVE_PREPARE_FAIL": "1"}
        assert_failure(binary, failing)

        wrong_runtime = env | {"SYNODRIVE_STATUS_TEST_NAUTILUS": "/missing/library.so"}
        assert_failure(binary, wrong_runtime)

        (overlay / "current").unlink()
        (overlay / "current").symlink_to("14")
        assert_failure(binary, env)
        (overlay / "current").unlink()
        (overlay / "current").symlink_to("15")

        (library / "plugin-cb-4.so").unlink()
        assert_failure(binary, env)
        shutil.copy2(helper, library / "plugin-cb-4.so")

        shutil.copy2(missing_symbol_helper, library / "plugin-cb-4.so")
        assert_failure(binary, env)
        shutil.copy2(helper, library / "plugin-cb-4.so")

        control.write_text("1")
        frames = b"/tmp/a path\nwith newline\0/tmp/b\0"
        result = run(binary, env, "--stdio", data=frames)
        assert result.returncode == 0
        assert result.stdout == b"synced\0synced\0", result.stdout

        result = run(binary, env, "--stdio", data=b"\0" + b"x" * (1024 * 1024 + 1) + b"\0")
        assert result.stdout == b"error\0error\0"

        result = run(binary, env, "--stdio", data=b"/partial-without-nul")
        assert result.stdout == b""

        process = subprocess.Popen(
            [binary, "--stdio"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, env=env
        )
        process.stdin.write(b"/tmp/split")
        process.stdin.flush()
        assert not select.select([process.stdout], [], [], 0.05)[0]
        process.stdin.write(b" path\nname\0")
        process.stdin.flush()
        assert read_frame(process) == b"synced"

        process.stdin.write(b"/tmp/A\0/tmp/A\0/tmp/B\0")
        process.stdin.flush()
        assert [read_frame(process) for _ in range(3)] == [b"synced"] * 3

        control.write_text("error")
        process.stdin.write(b"/tmp/fail\0")
        process.stdin.flush()
        assert read_frame(process) == b"error"
        control.write_text("2")
        process.stdin.write(b"/tmp/recover\0")
        process.stdin.flush()
        assert read_frame(process) == b"syncing"
        process.stdin.close()
        assert process.wait(timeout=5) == 0

        pids = pid_log.read_text().splitlines()
        assert len(pids) >= 6
        assert len(set(pids[-6:])) == 6, pids[-6:]

        check_release_binary(release_binary, helper, env, control, root)


if __name__ == "__main__":
    main()
