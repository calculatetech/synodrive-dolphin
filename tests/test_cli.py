#!/usr/bin/env python3

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import select


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


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    helper = pathlib.Path(sys.argv[2]).resolve()
    missing_symbol_helper = pathlib.Path(sys.argv[3]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        overlay = root / ".SynologyDrive/SynologyDrive.app/icon-overlay"
        library = overlay / "15/lib"
        library.mkdir(parents=True)
        shutil.copy2(helper, library / "plugin-cb-4.so")
        (overlay / "current").symlink_to("15")
        control = root / "status"
        pid_log = root / "pids"

        env = os.environ.copy()
        env.update(
            HOME=str(root),
            SYNODRIVE_STATUS_TEST_VERSION="8.0.2-17889",
            SYNODRIVE_STATUS_TEST_NAUTILUS=helper,
            FAKE_SYNODRIVE_CONTROL=str(control),
            FAKE_SYNODRIVE_PID_LOG=str(pid_log),
        )

        for args in [(), ("relative",), ("/tmp", "extra")]:
            result = run(binary, env, *args)
            assert result.returncode == 2, (args, result)
            assert not result.stdout

        names = [b"unknown\n", b"synced\n", b"syncing\n", b"unsupported\n",
                 b"read-only\n", b"no-permission\n"]
        strange_path = "/tmp/a path\nwith newline"
        for raw, expected in enumerate(names):
            control.write_text(str(raw))
            result = run(binary, env, strange_path)
            assert (result.returncode, result.stdout) == (0, expected), result

        control.write_text("6")
        result = run(binary, env, "/tmp")
        assert result.returncode == 1 and not result.stdout

        failing = env | {"FAKE_SYNODRIVE_PREPARE_FAIL": "1"}
        result = run(binary, failing, "/tmp")
        assert result.returncode == 1 and not result.stdout

        wrong_version = env | {"SYNODRIVE_STATUS_TEST_VERSION": "8.0.3"}
        assert run(binary, wrong_version, "/tmp").returncode == 1

        wrong_runtime = env | {"SYNODRIVE_STATUS_TEST_NAUTILUS": "/missing/library.so"}
        assert run(binary, wrong_runtime, "/tmp").returncode == 1

        (overlay / "current").unlink()
        (overlay / "current").symlink_to("14")
        assert run(binary, env, "/tmp").returncode == 1
        (overlay / "current").unlink()
        (overlay / "current").symlink_to("15")

        shutil.copy2(missing_symbol_helper, library / "plugin-cb-4.so")
        assert run(binary, env, "/tmp").returncode == 1
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
        control.write_text("1")
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


if __name__ == "__main__":
    main()
