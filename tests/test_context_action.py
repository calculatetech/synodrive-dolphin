#!/usr/bin/env python3

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


def run(binary, env, *args):
    return subprocess.run([binary, *args], capture_output=True, env=env, check=False)


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    release_binary = pathlib.Path(sys.argv[2]).resolve()
    provider = pathlib.Path(sys.argv[3]).resolve()
    status_binary = pathlib.Path(sys.argv[4]).resolve()
    status_provider = pathlib.Path(sys.argv[5]).resolve()
    plugin_test = pathlib.Path(sys.argv[6]).resolve()
    plugin_root = pathlib.Path(sys.argv[7]).resolve()
    missing_provider = pathlib.Path(sys.argv[8]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = pathlib.Path(directory)
        info = root / "INFO"
        info.write_text("[Version]\nmajor_version = 4\n")
        overlay = root / "overlay"
        library = overlay / "15/lib"
        library.mkdir(parents=True)
        shutil.copy2(provider, library / "plugin-cb-4.so")
        (overlay / "current").symlink_to("15")
        selected = root / "a file.txt"
        selected.write_text("test")
        log = root / "activation.log"
        env = os.environ.copy()
        env.update(
            HOME=str(root),
            SYNODRIVE_ACTION_TEST_INFO=str(info),
            SYNODRIVE_ACTION_TEST_OVERLAY=str(overlay / "current"),
            SYNODRIVE_ACTION_TEST_NAUTILUS="libnautilus-extension.so.4",
            FAKE_CONTEXT_LOG=str(log),
        )
        status_overlay = root / ".SynologyDrive/SynologyDrive.app/icon-overlay"
        (status_overlay / "15/lib").mkdir(parents=True)
        shutil.copy2(status_provider, status_overlay / "15/lib/plugin-cb-4.so")
        (status_overlay / "14").mkdir()
        (status_overlay / "current").symlink_to("15")
        control = root / "status"
        control.write_text("1")
        status_env = env | {
            "SYNODRIVE_STATUS_TEST_INFO": str(info),
            "SYNODRIVE_STATUS_TEST_NAUTILUS": str(status_provider),
            "FAKE_SYNODRIVE_CONTROL": str(control),
        }

        for args in [(), ("--list", "relative"), ("--activate", "unknown", str(selected))]:
            result = run(binary, env, *args)
            assert result.returncode == 2 and not result.stdout, result

        result = run(binary, env, "--list", str(selected))
        assert (result.returncode, result.stdout) == (
            0, b"get-link\nbrowse-versions\n"
        ), result
        assert run(status_binary, status_env, str(selected)).returncode == 0

        for mode in ["empty", "disabled", "unrelated", "prefix"]:
            result = run(binary, env | {"FAKE_CONTEXT_MODE": mode}, "--list", str(selected))
            assert (result.returncode, result.stdout) == (0, b""), (mode, result)

        result = run(binary, env | {"FAKE_CONTEXT_MODE": "single"}, "--list", str(selected))
        assert (result.returncode, result.stdout) == (0, b"get-link\n"), result
        result = run(binary, env | {"FAKE_CONTEXT_MODE": "prefix-exact"}, "--list", str(selected))
        assert (result.returncode, result.stdout) == (0, b"get-link\n"), result
        result = run(binary, env | {"FAKE_CONTEXT_MODE": "duplicate"}, "--list", str(selected))
        assert result.returncode == 1 and not result.stdout, result

        for action in ["get-link", "browse-versions"]:
            result = run(binary, env, "--activate", action, str(selected))
            assert result.returncode == 0, result
        lines = log.read_text().splitlines()
        assert lines[0].startswith("NautilusCloudStation::ShareLink file://")
        assert lines[1].startswith("NautilusCloudStation::VersionBrowse file://")
        assert "%20" in lines[0]

        result = run(binary, env | {"FAKE_CONTEXT_MODE": "single"},
                     "--activate", "browse-versions", str(selected))
        assert result.returncode == 1, result
        info.write_text("[Version]\nmajor_version = 5\n")
        assert run(binary, env, "--list", str(selected)).returncode == 1
        assert run(status_binary, status_env, str(selected)).returncode == 1

        (overlay / "current").unlink()
        (overlay / "current").symlink_to("15")
        (status_overlay / "current").unlink()
        (status_overlay / "current").symlink_to("15")
        shutil.copy2(missing_provider, library / "plugin-cb-4.so")
        shutil.copy2(missing_provider, status_overlay / "15/lib/plugin-cb-4.so")
        assert run(binary, env, "--list", str(selected)).returncode == 1
        assert run(status_binary, status_env, str(selected)).returncode == 1
        info.write_text("[Version\nmajor_version = 4\n")
        assert run(binary, env, "--list", str(selected)).returncode == 1
        assert run(status_binary, status_env, str(selected)).returncode == 1
        info.write_text("[Version]\nmajor_version = 4\n")

        (overlay / "current").unlink()
        (overlay / "14").mkdir()
        (overlay / "current").symlink_to("14")
        (status_overlay / "current").unlink()
        (status_overlay / "current").symlink_to("14")
        assert run(binary, env, "--list", str(selected)).returncode == 1
        assert run(status_binary, status_env, str(selected)).returncode == 1

        image = release_binary.read_bytes()
        for seam in [b"SYNODRIVE_ACTION_TEST_INFO", b"SYNODRIVE_ACTION_TEST_OVERLAY",
                     b"SYNODRIVE_ACTION_TEST_NAUTILUS", b"SYNODRIVE_ACTION_TEST_PID_LOG"]:
            assert seam not in image

    plugin_env = os.environ | {
        "QT_QPA_PLATFORM": "offscreen",
        "QT_PLUGIN_PATH": str(plugin_root),
        "SYNODRIVE_ACTION_TEST_MODULE": str(
            plugin_root / "kf6/kfileitemaction/synodrive-fileitemaction.so"
        ),
    }
    result = subprocess.run([plugin_test, provider, binary], env=plugin_env, check=False)
    assert result.returncode == 0, result


if __name__ == "__main__":
    main()
