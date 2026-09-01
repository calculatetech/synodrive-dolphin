#!/usr/bin/env python3

import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


def run(script, root, fail_image=None, argument=None, overrides=None):
    log = root / "docker.log"
    environment = os.environ.copy()
    environment["PATH"] = f"{root / 'bin'}:{environment['PATH']}"
    environment["FAKE_DOCKER_LOG"] = str(log)
    if fail_image:
        environment["FAKE_DOCKER_FAIL_IMAGE"] = fail_image
    if overrides:
        environment.update(overrides)
    command = [str(script)]
    if argument:
        command.append(argument)
    result = subprocess.run(command, text=True, capture_output=True, env=environment)
    calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
    return result, calls


def assert_run_boundaries(calls, checkout):
    runs = [call for call in calls if call[0] == "run"]
    assert len(runs) == 2, calls
    legs = [
        (runs[0], "26.04", "DEB", "ubuntu-26.04", "synodrive-dolphin_0.3.0-1_amd64.deb"),
        (runs[1], "44", "RPM", "fedora-44", "synodrive-dolphin-0.3.0-1.x86_64.rpm"),
    ]
    for call, version, package_format, directory, filename in legs:
        image = f"synodrive-dolphin-ci:{directory}"
        expected = [
            "run", "--rm", "--init", "--network", "none",
            "--mount", f"type=bind,source={checkout},target=/src,readonly",
            "--mount", f"type=bind,source={checkout}/build/packages/{directory},target=/packages",
            "--workdir", "/src",
            "--env", f"EXPECTED_VERSION={version}",
            "--env", f"PACKAGE_FORMAT={package_format}",
            "--env", f"PACKAGE_FILENAME={filename}",
            "--env", f"PACKAGE_UID={os.getuid()}",
            "--env", f"PACKAGE_GID={os.getgid()}",
            image, "sh", "-eu", "-c",
        ]
        assert call[:-1] == expected, call
        command = call[-1]
        assert "cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF" in command, command
        assert "cmake --build /build" in command, command
        assert 'cpack --config /build/CPackConfig.cmake -G "$PACKAGE_FORMAT" -B /packages' in command, command
        assert '/src/ci/validate-package "$PACKAGE_FORMAT" "$artifact"' in command, command


def assert_validator(validator, root):
    environment = os.environ.copy()
    environment["PATH"] = f"{root / 'bin'}:{environment['PATH']}"
    for package_format, plugin_root, suffix, fields, dependencies in [
        (
            "DEB", "/usr/lib/x86_64-linux-gnu/qt6/plugins", "deb",
            ["Package", "Version", "Architecture", "Maintainer", "Homepage", "Section", "Priority", "Description"],
            ["dolphin", "libnautilus-extension4", "libc6", "libgcc-s1", "libkf6kiocore6", "libqt6core6t64", "libstdc++6"],
        ),
        (
            "RPM", "/usr/lib64/qt6/plugins", "rpm",
            ["%{NAME}", "%{VERSION}", "%{RELEASE}", "%{ARCH}", "%{SUMMARY}", "%{LICENSE}", "%{URL}", "%{VENDOR}", "%{PACKAGER}"],
            ["dolphin", "libnautilus-extension.so.4()(64bit)", "libc.so.6()(64bit)", "libgcc_s.so.1()(64bit)", "libKF6KIOCore.so.6()(64bit)", "libQt6Core.so.6()(64bit)", "libstdc++.so.6()(64bit)"],
        ),
    ]:
        artifact = root / f"candidate.{suffix}"
        artifact.touch()
        environment["FAKE_FORMAT"] = package_format
        environment["FAKE_PLUGIN_ROOT"] = plugin_root
        for variable in ["FAKE_BAD_FIELD", "FAKE_DUPLICATE", "FAKE_DUPLICATE_DEPENDENCY", "FAKE_EMPTY_DEPENDENCIES", "FAKE_EMPTY_PAYLOAD", "FAKE_EXTRA", "FAKE_MISSING", "FAKE_MISSING_DEPENDENCY", "FAKE_MISSING_PLUGIN", "FAKE_ONE_DEPENDENCY", "FAKE_ONE_RECORD", "FAKE_SYNOLOGY_DEPENDENCY"]:
            environment.pop(variable, None)

        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode == 0, (package_format, result)

        for field in fields:
            environment["FAKE_BAD_FIELD"] = field
            result = subprocess.run([validator, package_format, artifact], env=environment)
            assert result.returncode != 0, (package_format, "bad identity", field)
        environment.pop("FAKE_BAD_FIELD")

        environment["FAKE_EXTRA"] = "/usr/share/synodrive-dolphin/unexpected"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "extra payload")
        environment.pop("FAKE_EXTRA")

        environment["FAKE_DUPLICATE"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "duplicate payload")
        environment.pop("FAKE_DUPLICATE")

        environment["FAKE_MISSING"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "missing payload")
        environment.pop("FAKE_MISSING")

        environment["FAKE_ONE_RECORD"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "one-record payload")
        environment.pop("FAKE_ONE_RECORD")

        environment["FAKE_EMPTY_PAYLOAD"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "empty payload")
        environment.pop("FAKE_EMPTY_PAYLOAD")

        environment["FAKE_MISSING_PLUGIN"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "missing plugin")
        environment.pop("FAKE_MISSING_PLUGIN")

        for dependency in dependencies:
            environment["FAKE_MISSING_DEPENDENCY"] = dependency
            result = subprocess.run([validator, package_format, artifact], env=environment)
            assert result.returncode != 0, (package_format, "missing dependency", dependency)
        environment.pop("FAKE_MISSING_DEPENDENCY")

        environment["FAKE_EMPTY_DEPENDENCIES"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "empty dependencies")
        environment.pop("FAKE_EMPTY_DEPENDENCIES")

        environment["FAKE_ONE_DEPENDENCY"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "one dependency")
        environment.pop("FAKE_ONE_DEPENDENCY")

        environment["FAKE_DUPLICATE_DEPENDENCY"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode == 0, (package_format, "harmless duplicate dependency")
        environment.pop("FAKE_DUPLICATE_DEPENDENCY")

        environment["FAKE_SYNOLOGY_DEPENDENCY"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "Synology dependency")


def main():
    source_script = pathlib.Path(sys.argv[1]).resolve()
    cmake = source_script.parent.parent / "CMakeLists.txt"
    cmake_text = cmake.read_text()
    assert "set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)" in cmake_text, cmake
    assert "set(CPACK_RPM_PACKAGE_AUTOREQ ON)" in cmake_text, cmake
    with tempfile.TemporaryDirectory() as temporary:
        root = pathlib.Path(temporary)
        checkout = root / "checkout"
        ci = checkout / "ci"
        ci.mkdir(parents=True)
        script = ci / "package"
        validator = ci / "validate-package"
        shutil.copy2(source_script, script)
        shutil.copy2(source_script.with_name("validate-package"), validator)
        shutil.copy2(source_script.with_name("ubuntu.Dockerfile"), ci)
        shutil.copy2(source_script.with_name("fedora.Dockerfile"), ci)

        fake_bin = root / "bin"
        fake_bin.mkdir()
        docker = fake_bin / "docker"
        docker.write_text(
            """#!/usr/bin/env python3
import json, os, pathlib, subprocess, sys
args = sys.argv[1:]
with open(os.environ['FAKE_DOCKER_LOG'], 'a') as log:
    print(json.dumps(args), file=log)
image = next((arg for arg in args if arg.startswith('synodrive-dolphin-ci:')), '')
if args[0] == 'run' and image == os.environ.get('FAKE_DOCKER_FAIL_IMAGE'):
    sys.exit(7)
if args[0] == 'run':
    source_mount = next(arg for arg in args if 'target=/src' in arg)
    source = pathlib.Path(source_mount.split('source=', 1)[1].split(',target=', 1)[0])
    output_mount = next(arg for arg in args if 'target=/packages' in arg)
    output = pathlib.Path(output_mount.split('source=', 1)[1].split(',target=', 1)[0])
    package_format = 'DEB' if image.endswith('ubuntu-26.04') else 'RPM'
    environment = os.environ.copy()
    for index, value in enumerate(args):
        if value == '--env':
            key, setting = args[index + 1].split('=', 1)
            environment[key] = setting
    environment['FAKE_FORMAT'] = package_format
    environment['FAKE_PLUGIN_ROOT'] = ('/usr/lib/x86_64-linux-gnu/qt6/plugins'
                                       if package_format == 'DEB'
                                       else '/usr/lib64/qt6/plugins')
    if environment.get('FAKE_TARGET_FORMAT') not in (None, package_format):
        for key in list(environment):
            if key.startswith('FAKE_') and key not in {'FAKE_DOCKER_LOG', 'FAKE_TARGET_FORMAT'}:
                environment.pop(key)
        environment['FAKE_FORMAT'] = package_format
        environment['FAKE_PLUGIN_ROOT'] = ('/usr/lib/x86_64-linux-gnu/qt6/plugins'
                                           if package_format == 'DEB'
                                           else '/usr/lib64/qt6/plugins')
    build = source.parent / f'container-build-{package_format.lower()}'
    build.mkdir(exist_ok=True)
    command = args[-1]
    command = command.replace('. /etc/os-release', 'VERSION_ID=$EXPECTED_VERSION')
    command = command.replace('/src', str(source))
    command = command.replace('/build', str(build))
    command = command.replace('/packages', str(output))
    result = subprocess.run(['sh', '-eu', '-c', command], env=environment)
    sys.exit(result.returncode)
"""
        )
        docker.chmod(0o755)

        cmake = fake_bin / "cmake"
        cmake.write_text("#!/bin/sh\nexit 0\n")
        cmake.chmod(0o755)
        cpack = fake_bin / "cpack"
        cpack.write_text(
            """#!/usr/bin/env python3
import os, pathlib, sys
args = sys.argv[1:]
output = pathlib.Path(args[args.index('-B') + 1])
output.mkdir(parents=True, exist_ok=True)
(output / os.environ['PACKAGE_FILENAME']).touch()
"""
        )
        cpack.chmod(0o755)
        chown = fake_bin / "chown"
        chown.write_text("#!/bin/sh\nexit 0\n")
        chown.chmod(0o755)

        package_tool = fake_bin / "package-tool"
        package_tool.write_text(
            """#!/usr/bin/env python3
import os, pathlib, sys

tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
plugin_root = os.environ['FAKE_PLUGIN_ROOT']
payload = [
    '/usr/bin/synodrive-status',
    f'{plugin_root}/kf6/overlayicon/synodrive-overlay.so',
    '/usr/share/doc/synodrive-dolphin/copyright',
]
if os.environ.get('FAKE_DUPLICATE'):
    payload.append(payload[0])
if os.environ.get('FAKE_EXTRA'):
    payload.append(os.environ['FAKE_EXTRA'])
if os.environ.get('FAKE_MISSING'):
    payload.pop()
if os.environ.get('FAKE_ONE_RECORD'):
    payload = payload[:1]
if os.environ.get('FAKE_EMPTY_PAYLOAD'):
    payload = []
if os.environ.get('FAKE_MISSING_PLUGIN'):
    payload = [path for path in payload if not path.endswith('/synodrive-overlay.so')]

if tool == 'qtpaths6':
    print(plugin_root)
elif tool == 'dpkg-deb':
    dependencies = ('dolphin, libnautilus-extension4, libc6 (>= 2.34), '
                    'libgcc-s1, libkf6kiocore6, libqt6core6t64, libstdc++6')
    if os.environ.get('FAKE_SYNOLOGY_DEPENDENCY'):
        dependencies += ', synology-drive'
    if os.environ.get('FAKE_DUPLICATE_DEPENDENCY'):
        dependencies = 'dolphin, ' + dependencies
    if os.environ.get('FAKE_EMPTY_DEPENDENCIES'):
        dependencies = ''
    if os.environ.get('FAKE_ONE_DEPENDENCY'):
        dependencies = 'dolphin'
    fields = {
        'Package': 'synodrive-dolphin',
        'Version': '0.3.0-1',
        'Architecture': 'amd64',
        'Maintainer': 'Michael Beutler <mikebeutler84@gmail.com>',
        'Homepage': 'https://github.com/calculatetech/synodrive-dolphin',
        'Section': 'utils',
        'Priority': 'optional',
        'Description': 'Synology Drive Dolphin Extension (Unofficial)',
        'Depends': dependencies,
    }
    if os.environ.get('FAKE_BAD_FIELD') in fields:
        fields[os.environ['FAKE_BAD_FIELD']] = 'wrong'
    missing_dependency = os.environ.get('FAKE_MISSING_DEPENDENCY')
    if missing_dependency:
        dependencies = ', '.join(
            item for item in dependencies.split(', ')
            if item.split(' ', 1)[0] != missing_dependency
        )
        fields['Depends'] = dependencies
    if args[0] == '-f':
        print(fields[args[2]])
    elif args[0] == '-c':
        for path in payload:
            print(f'-rw-r--r-- root/root 1 2026-09-01 00:00 .{path}')
elif tool == 'rpm':
    dependencies = [
        'dolphin', 'libnautilus-extension.so.4()(64bit)', 'libc.so.6()(64bit)',
        'libgcc_s.so.1()(64bit)', 'libKF6KIOCore.so.6()(64bit)',
        'libQt6Core.so.6()(64bit)', 'libstdc++.so.6()(64bit)',
    ]
    if os.environ.get('FAKE_SYNOLOGY_DEPENDENCY'):
        dependencies.append('synology-drive')
    if os.environ.get('FAKE_DUPLICATE_DEPENDENCY'):
        dependencies.insert(0, dependencies[0])
    if os.environ.get('FAKE_EMPTY_DEPENDENCIES'):
        dependencies = []
    if os.environ.get('FAKE_ONE_DEPENDENCY'):
        dependencies = dependencies[:1]
    fields = {
        '%{NAME}': 'synodrive-dolphin', '%{VERSION}': '0.3.0', '%{RELEASE}': '1',
        '%{ARCH}': 'x86_64', '%{SUMMARY}': 'Synology Drive Dolphin Extension (Unofficial)',
        '%{LICENSE}': 'MIT', '%{URL}': 'https://github.com/calculatetech/synodrive-dolphin',
        '%{VENDOR}': 'calculatetech',
        '%{PACKAGER}': 'Michael Beutler <mikebeutler84@gmail.com>',
    }
    if os.environ.get('FAKE_BAD_FIELD') in fields:
        fields[os.environ['FAKE_BAD_FIELD']] = 'wrong'
    missing_dependency = os.environ.get('FAKE_MISSING_DEPENDENCY')
    if missing_dependency:
        dependencies.remove(missing_dependency)
    if args[:2] == ['-qp', '--qf']:
        print(fields[args[2]], end='')
    elif args[:2] == ['-qpl', '--dump']:
        for path in payload:
            print(f'{path} 1 0 digest 0100644 root root 0 0 0 -')
    elif args[:2] == ['-qp', '--requires']:
        print('\\n'.join(dependencies))
"""
        )
        package_tool.chmod(0o755)
        for name in ["qtpaths6", "dpkg-deb", "rpm"]:
            (fake_bin / name).symlink_to(package_tool)

        assert_validator(validator, root)

        rejected, calls = run(script, root, argument="unexpected")
        assert rejected.returncode == 2, rejected
        assert rejected.stderr == "usage: ./ci/package\n", rejected
        assert calls == [], calls

        success, calls = run(script, root)
        assert success.returncode == 0, success
        assert [call[0] for call in calls] == ["build", "run", "build", "run"], calls
        assert all("ubuntu-26.04" in " ".join(call) for call in calls[:2]), calls
        assert all("fedora-44" in " ".join(call) for call in calls[2:]), calls
        assert_run_boundaries(calls, checkout)
        expected_paths = [
            str(checkout / "build/packages/ubuntu-26.04/synodrive-dolphin_0.3.0-1_amd64.deb"),
            str(checkout / "build/packages/fedora-44/synodrive-dolphin-0.3.0-1.x86_64.rpm"),
        ]
        lines = success.stdout.rstrip().splitlines()
        assert lines[-2:] == expected_paths, success.stdout
        assert all(lines.count(path) == 1 for path in expected_paths), success.stdout

        for package_format, bad_field in [("DEB", "Version"), ("RPM", "%{VERSION}")]:
            (root / "docker.log").unlink()
            rejected, calls = run(
                script, root,
                overrides={"FAKE_TARGET_FORMAT": package_format, "FAKE_BAD_FIELD": bad_field},
            )
            assert rejected.returncode != 0, (package_format, "composed identity")
            assert expected_paths[0] not in rejected.stdout, rejected.stdout

        for package_format in ["DEB", "RPM"]:
            (root / "docker.log").unlink()
            rejected, calls = run(
                script, root,
                overrides={
                    "FAKE_TARGET_FORMAT": package_format,
                    "FAKE_DUPLICATE": "1",
                    "FAKE_EXTRA": "/usr/share/synodrive-dolphin/later-unexpected",
                },
            )
            assert rejected.returncode != 0, (package_format, "composed payload full set")
            assert expected_paths[0] not in rejected.stdout, rejected.stdout

            (root / "docker.log").unlink()
            rejected, calls = run(
                script, root,
                overrides={
                    "FAKE_TARGET_FORMAT": package_format,
                    "FAKE_DUPLICATE_DEPENDENCY": "1",
                    "FAKE_SYNOLOGY_DEPENDENCY": "1",
                },
            )
            assert rejected.returncode != 0, (package_format, "composed dependency full set")
            assert expected_paths[0] not in rejected.stdout, rejected.stdout

        (root / "docker.log").unlink()
        first_failure, calls = run(script, root, fail_image="synodrive-dolphin-ci:ubuntu-26.04")
        assert first_failure.returncode == 7, first_failure
        assert len(calls) == 2 and all("fedora-44" not in " ".join(call) for call in calls), calls
        assert "build/packages/ubuntu-26.04/" not in first_failure.stdout, first_failure.stdout

        (root / "docker.log").unlink()
        later_failure, calls = run(script, root, fail_image="synodrive-dolphin-ci:fedora-44")
        assert later_failure.returncode == 7, later_failure
        assert [call[0] for call in calls] == ["build", "run", "build", "run"], calls
        assert sum("fedora-44" in " ".join(call) and call[0] == "run" for call in calls) == 1, calls
        assert "build/packages/ubuntu-26.04/" not in later_failure.stdout, later_failure.stdout


if __name__ == "__main__":
    main()
