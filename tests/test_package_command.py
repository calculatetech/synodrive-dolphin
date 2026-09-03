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
    assert len(calls) == 8, calls
    builds = calls[0::2]
    runs = calls[1::2]
    assert [call[0] for call in builds] == ["build"] * 4, calls
    assert [call[0] for call in runs] == ["run"] * 4, calls
    legs = [
        (calls[0:4], "26.04", "DEB", "ubuntu-26.04", "ubuntu.Dockerfile", "synodrive-dolphin_0.4.0-1_amd64.deb"),
        (calls[4:8], "44", "RPM", "fedora-44", "fedora.Dockerfile", "synodrive-dolphin-0.4.0-1.x86_64.rpm"),
    ]
    for leg, version, package_format, directory, dockerfile, filename in legs:
        image = f"synodrive-dolphin-ci:{directory}"
        runtime_image = f"{image}-runtime"
        build_call, build_run, runtime_call, runtime_run = leg
        assert build_call == [
            "build", "--file", f"{checkout}/ci/{dockerfile}", "--target", "build",
            "--tag", image, f"{checkout}/ci",
        ], build_call
        assert runtime_call == [
            "build", "--file", f"{checkout}/ci/{dockerfile}", "--target", "runtime",
            "--tag", runtime_image, f"{checkout}/ci",
        ], runtime_call
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
        assert build_run[:-1] == expected, build_run
        command = build_run[-1]
        assert "cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX=/usr" in command, command
        assert "cmake --build /build" in command, command
        assert 'cpack --config /build/CPackConfig.cmake -G "$PACKAGE_FORMAT" -B /packages' in command, command
        assert '/src/ci/validate-package "$PACKAGE_FORMAT" "$artifact"' in command, command
        assert runtime_run == [
            "run", "--rm", "--init", "--network", "none",
            "--mount", f"type=bind,source={checkout},target=/src,readonly",
            "--mount", f"type=bind,source={checkout}/build/packages/{directory},target=/packages,readonly",
            "--workdir", "/src",
            "--env", "SYNODRIVE_PACKAGE_CONTAINER=1",
            runtime_image, "/src/ci/validate-package-lifecycle", package_format,
            f"/packages/{filename}",
        ], runtime_run


def assert_validator(validator, root):
    environment = os.environ.copy()
    environment["PATH"] = f"{root / 'bin'}:{environment['PATH']}"
    for package_format, plugin_root, suffix, fields, dependencies in [
        (
            "DEB", "/usr/lib/x86_64-linux-gnu/qt6/plugins", "deb",
            ["Package", "Version", "Architecture", "Maintainer", "Homepage", "Section", "Priority", "Description"],
            ["dolphin", "libnautilus-extension4", "libc6", "libgcc-s1", "libkf6coreaddons6", "libkf6kiocore6", "libkf6kiowidgets6", "libqt6core6t64", "libqt6widgets6", "libstdc++6"],
        ),
        (
            "RPM", "/usr/lib64/qt6/plugins", "rpm",
            ["%{NAME}", "%{VERSION}", "%{RELEASE}", "%{ARCH}", "%{SUMMARY}", "%{LICENSE}", "%{URL}", "%{VENDOR}", "%{PACKAGER}"],
            ["dolphin", "libnautilus-extension.so.4()(64bit)", "libc.so.6()(64bit)", "libgcc_s.so.1()(64bit)", "libKF6CoreAddons.so.6()(64bit)", "libKF6KIOCore.so.6()(64bit)", "libKF6KIOWidgets.so.6()(64bit)", "libQt6Core.so.6()(64bit)", "libQt6Widgets.so.6()(64bit)", "libstdc++.so.6()(64bit)"],
        ),
    ]:
        artifact = root / f"candidate.{suffix}"
        artifact.touch()
        environment["FAKE_FORMAT"] = package_format
        environment["FAKE_PLUGIN_ROOT"] = plugin_root
        for variable in ["FAKE_BAD_FIELD", "FAKE_DEB_SCRIPTLET", "FAKE_DUPLICATE", "FAKE_DUPLICATE_DEPENDENCY", "FAKE_EMPTY_DEPENDENCIES", "FAKE_EMPTY_PAYLOAD", "FAKE_EXTRA", "FAKE_MISSING", "FAKE_MISSING_DEPENDENCY", "FAKE_MISSING_PLUGIN", "FAKE_ONE_DEPENDENCY", "FAKE_ONE_RECORD", "FAKE_RPM_SCRIPTLET", "FAKE_RPM_TRIGGER", "FAKE_SYNOLOGY_DEPENDENCY"]:
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
        environment.pop("FAKE_SYNOLOGY_DEPENDENCY")

        environment[f"FAKE_{package_format}_SCRIPTLET"] = "1"
        result = subprocess.run([validator, package_format, artifact], env=environment)
        assert result.returncode != 0, (package_format, "package scriptlet")
        environment.pop(f"FAKE_{package_format}_SCRIPTLET")
        if package_format == "RPM":
            environment["FAKE_RPM_TRIGGER"] = "1"
            result = subprocess.run([validator, package_format, artifact], env=environment)
            assert result.returncode != 0, (package_format, "package trigger")
            environment.pop("FAKE_RPM_TRIGGER")


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
        lifecycle = ci / "validate-package-lifecycle"
        shutil.copy2(source_script, script)
        shutil.copy2(source_script.with_name("validate-package"), validator)
        shutil.copy2(source_script.with_name("validate-package-lifecycle"), lifecycle)
        shutil.copy2(source_script.with_name("ubuntu.Dockerfile"), ci)
        shutil.copy2(source_script.with_name("fedora.Dockerfile"), ci)
        shutil.copy2(source_script.parent.parent / "LICENSE", checkout)
        tests = checkout / "tests"
        tests.mkdir()
        shutil.copy2(source_script.parent.parent / "tests/tray_patch_fixture.py", tests)

        fake_bin = root / "bin"
        fake_bin.mkdir()
        docker = fake_bin / "docker"
        docker.write_text(
            """#!/usr/bin/env python3
import json, os, pathlib, shutil, subprocess, sys
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
    package_format = 'DEB' if 'ubuntu-26.04' in image else 'RPM'
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
            if key.startswith('FAKE_') and key not in {
                'FAKE_DOCKER_FAIL_IMAGE', 'FAKE_DOCKER_LOG', 'FAKE_TARGET_FORMAT'
            }:
                environment.pop(key)
        environment['FAKE_FORMAT'] = package_format
        environment['FAKE_PLUGIN_ROOT'] = ('/usr/lib/x86_64-linux-gnu/qt6/plugins'
                                           if package_format == 'DEB'
                                           else '/usr/lib64/qt6/plugins')
    if image.endswith('-runtime'):
        lifecycle_root = source.parent / f'lifecycle-root-{package_format.lower()}'
        shutil.rmtree(lifecycle_root, ignore_errors=True)
        lifecycle_root.mkdir()
        environment['SYNODRIVE_LIFECYCLE_TEST_ROOT'] = str(lifecycle_root)
        environment['FAKE_SOURCE'] = str(source)
        if environment.get('FAKE_PREINSTALLED'):
            (lifecycle_root / '.installed').touch()
        preexisting = environment.get('FAKE_PREEXISTING_PATH')
        if preexisting:
            path = lifecycle_root / preexisting.lstrip('/')
            if environment.get('FAKE_PREEXISTING_DIRECTORY'):
                path.mkdir(parents=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
        if environment.get('FAKE_SYNOLOGY_PATH'):
            (lifecycle_root / 'opt/Synology/SynologyDrive').mkdir(parents=True)
        image_index = args.index(image)
        command = [
            value.replace('/src', str(source)).replace('/packages', str(output))
            for value in args[image_index + 1:]
        ]
        result = subprocess.run(command, env=environment)
        sys.exit(result.returncode)
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
if not os.environ.get('FAKE_CPACK_NO_OUTPUT'):
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
import os, pathlib, shutil, sys

tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
plugin_root = os.environ['FAKE_PLUGIN_ROOT']
payload = [
    '/usr/bin/synodrive-status',
    '/usr/bin/synodrive-tray-patch',
    '/usr/libexec/synodrive-dolphin/synodrive-action',
    f'{plugin_root}/kf6/overlayicon/synodrive-overlay.so',
    f'{plugin_root}/kf6/kfileitemaction/synodrive-fileitemaction.so',
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

root = pathlib.Path(os.environ.get('SYNODRIVE_LIFECYCLE_TEST_ROOT', '/'))
state = root / '.installed'
command_path = '/usr/bin/synodrive-status'
patch_command_path = '/usr/bin/synodrive-tray-patch'
action_helper_path = '/usr/libexec/synodrive-dolphin/synodrive-action'
plugin_path = f'{plugin_root}/kf6/overlayicon/synodrive-overlay.so'
action_plugin_path = f'{plugin_root}/kf6/kfileitemaction/synodrive-fileitemaction.so'
copyright_path = '/usr/share/doc/synodrive-dolphin/copyright'
lifecycle_files = [command_path, patch_command_path, action_helper_path, plugin_path, action_plugin_path, copyright_path]

def installed_path(path):
    return root / path.lstrip('/')

def install():
    for path in lifecycle_files:
        target = installed_path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        if path in [command_path, patch_command_path]:
            usage = ('usage: synodrive-status <absolute-path>' if path == command_path
                     else 'usage: synodrive-tray-patch status|apply|restore')
            target.write_text('''#!/bin/sh
[ -n "${FAKE_CLI_STDOUT-}" ] && printf '%s' "$FAKE_CLI_STDOUT"
if [ -n "${FAKE_CLI_STDERR+x}" ]; then
    printf '%s' "$FAKE_CLI_STDERR" >&2
else
    printf '%s\\n' 'usage: synodrive-status <absolute-path>' >&2
fi
exit "${FAKE_CLI_STATUS-2}"
''')
            target.write_text(target.read_text().replace(
                'usage: synodrive-status <absolute-path>', usage,
            ))
            target.chmod(0o755)
        elif path == copyright_path:
            shutil.copyfile(pathlib.Path(os.environ['FAKE_SOURCE']) / 'LICENSE', target)
        else:
            target.write_bytes(b'ELF fixture')
    if os.environ.get('FAKE_BAD_LICENSE'):
        installed_path(copyright_path).write_text('wrong license')
    symlink = os.environ.get('FAKE_LIFECYCLE_SYMLINK')
    if symlink:
        target = installed_path(symlink)
        target.unlink()
        target.symlink_to('/dev/null')
    state.touch()

def inventory():
    files = lifecycle_files.copy()
    kind = os.environ.get('FAKE_LIFECYCLE_INVENTORY')
    if kind == 'empty':
        files = []
    elif kind == 'one':
        files = files[:1]
    elif kind == 'missing':
        files.pop()
    elif kind == 'duplicate':
        files.append(files[0])
    elif kind == 'extra':
        files.append('/usr/share/synodrive-dolphin/unexpected')
    elif kind == 'duplicate-extra':
        files.extend([files[0], '/usr/share/synodrive-dolphin/later-unexpected'])
    directories = sorted({str(pathlib.PurePosixPath(path).parent) for path in lifecycle_files})
    return directories + files

def remove():
    retained = os.environ.get('FAKE_RETAIN_PATH')
    for path in lifecycle_files:
        target = installed_path(path)
        if path != retained and (target.exists() or target.is_symlink()):
            target.unlink()
    doc = installed_path('/usr/share/doc/synodrive-dolphin')
    if not os.environ.get('FAKE_RETAIN_DOC') and doc.exists():
        try:
            doc.rmdir()
        except OSError:
            pass
    if not os.environ.get('FAKE_RETAIN_DB'):
        state.unlink(missing_ok=True)
    if os.environ.get('FAKE_RETAIN_SYNOLOGY'):
        installed_path('/opt/Synology/SynologyDrive').mkdir(parents=True)

if tool == 'qtpaths6':
    print(plugin_root)
elif tool == 'dpkg-deb':
    dependencies = ('dolphin, libnautilus-extension4, libc6 (>= 2.34), '
                    'libgcc-s1, libkf6coreaddons6, libkf6kiocore6, '
                    'libkf6kiowidgets6, libqt6core6t64, libqt6widgets6, libstdc++6')
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
        'Version': '0.4.0-1',
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
    elif args[0] == '-e':
        control = pathlib.Path(args[2])
        control.mkdir(parents=True, exist_ok=True)
        (control / 'control').touch()
        if os.environ.get('FAKE_DEB_SCRIPTLET'):
            (control / 'postinst').touch()
elif tool == 'dpkg-query':
    if args[:1] == ['-L']:
        if not state.exists():
            sys.exit(1)
        print('\\n'.join(inventory()))
    elif args[:1] == ['-W']:
        if len(args) == 2 and args[1] == 'synodrive-dolphin':
            sys.exit(0 if state.exists() else 1)
        if not state.exists() and args[-1:] == ['synodrive-dolphin']:
            sys.exit(1)
        fields = {
            '${binary:Package}': 'synodrive-dolphin', '${Version}': '0.4.0-1',
            '${Architecture}': 'amd64',
            '${Maintainer}': 'Michael Beutler <mikebeutler84@gmail.com>',
            '${Homepage}': 'https://github.com/calculatetech/synodrive-dolphin',
            '${Section}': 'utils', '${Priority}': 'optional',
            '${binary:Summary}': 'Synology Drive Dolphin Extension (Unofficial)',
        }
        field = next((value[3:] for value in args if value.startswith('-f=')), None)
        if field.startswith('${binary:Package}') and args[-1] != 'synodrive-dolphin':
            print('base-files')
            if os.environ.get('FAKE_SYNOLOGY_PACKAGE'):
                print('synology-drive')
        else:
            value = fields[field]
            if os.environ.get('FAKE_LIFECYCLE_BAD_FIELD') == field:
                value = 'wrong'
            print(value, end='')
elif tool == 'dpkg':
    if args[:1] == ['--install']:
        install()
    elif args[:1] == ['--verify']:
        if os.environ.get('FAKE_VERIFY_FAIL'):
            print('??5?????? /usr/bin/synodrive-status')
            sys.exit(1)
    elif args[:1] == ['--purge']:
        remove()
elif tool == 'rpm':
    dependencies = [
        'dolphin', 'libnautilus-extension.so.4()(64bit)', 'libc.so.6()(64bit)',
        'libgcc_s.so.1()(64bit)', 'libKF6CoreAddons.so.6()(64bit)',
        'libKF6KIOCore.so.6()(64bit)', 'libKF6KIOWidgets.so.6()(64bit)',
        'libQt6Core.so.6()(64bit)', 'libQt6Widgets.so.6()(64bit)',
        'libstdc++.so.6()(64bit)',
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
        '%{NAME}': 'synodrive-dolphin', '%{VERSION}': '0.4.0', '%{RELEASE}': '1',
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
    elif args[:2] == ['-qp', '--scripts']:
        if os.environ.get('FAKE_RPM_SCRIPTLET'):
            print('postinstall scriptlet')
    elif args[:2] == ['-qp', '--triggers']:
        if os.environ.get('FAKE_RPM_TRIGGER'):
            print('trigger scriptlet')
    elif args[:2] == ['-q', '--qf']:
        if not state.exists():
            sys.exit(1)
        value = fields[args[2]]
        if os.environ.get('FAKE_LIFECYCLE_BAD_FIELD') == args[2]:
            value = 'wrong'
        print(value, end='')
    elif args[:1] == ['-ql']:
        if not state.exists():
            sys.exit(1)
        print('\\n'.join(inventory()))
    elif args[:1] == ['-q']:
        sys.exit(0 if state.exists() else 1)
    elif args[:1] == ['-qa']:
        print('filesystem')
        if os.environ.get('FAKE_SYNOLOGY_PACKAGE'):
            print('synology-drive')
    elif args[:1] == ['--install']:
        install()
    elif args[:1] == ['--verify']:
        if os.environ.get('FAKE_VERIFY_FAIL'):
            print('S.5....T.  /usr/bin/synodrive-status')
            sys.exit(1)
    elif args[:1] == ['--erase']:
        remove()
elif tool == 'file':
    path = args[-1]
    bad = os.environ.get('FAKE_BAD_FILE_PATH')
    print('ASCII text' if bad and path.endswith(bad) else 'ELF 64-bit LSB shared object')
elif tool == 'stat':
    path = args[-1]
    logical = '/' + str(pathlib.Path(path).relative_to(root))
    plugin_mode = '644' if os.environ['FAKE_FORMAT'] == 'DEB' else '755'
    modes = {command_path: '755', patch_command_path: '755', action_helper_path: '755', plugin_path: plugin_mode,
             action_plugin_path: plugin_mode, copyright_path: '644'}
    result = f'root:root {modes[logical]}'
    if os.environ.get('FAKE_BAD_STAT_PATH') == logical:
        result = os.environ['FAKE_BAD_STAT_VALUE']
    print(result)
elif tool == 'ldd':
    path = args[-1]
    bad = os.environ.get('FAKE_LDD_BAD_PATH')
    if bad and path.endswith(bad):
        print(os.environ.get('FAKE_LDD_OUTPUT', 'libmissing.so => not found'))
        sys.exit(int(os.environ.get('FAKE_LDD_STATUS', '0')))
    if os.environ.get('FAKE_LDD_SYNOLOGY'):
        print('libsynology.so => /opt/Synology/libsynology.so')
    else:
        print('libc.so.6 => /lib/libc.so.6')
elif tool == 'ldconfig':
    if not os.environ.get('FAKE_MISSING_NAUTILUS'):
        print('libnautilus-extension.so.4 (libc6,x86-64) => /lib/libnautilus-extension.so.4 ')
"""
        )
        package_tool.chmod(0o755)
        for name in ["qtpaths6", "dpkg-deb", "dpkg-query", "dpkg", "rpm", "file", "stat", "ldd", "ldconfig"]:
            (fake_bin / name).symlink_to(package_tool)

        assert_validator(validator, root)

        rejected, calls = run(script, root, argument="unexpected")
        assert rejected.returncode == 2, rejected
        assert rejected.stderr == "usage: ./ci/package\n", rejected
        assert calls == [], calls

        success, calls = run(script, root)
        assert success.returncode == 0, success
        assert [call[0] for call in calls] == ["build", "run"] * 4, calls
        assert all("ubuntu-26.04" in " ".join(call) for call in calls[:4]), calls
        assert all("fedora-44" in " ".join(call) for call in calls[4:]), calls
        assert_run_boundaries(calls, checkout)
        expected_paths = [
            str(checkout / "build/packages/ubuntu-26.04/synodrive-dolphin_0.4.0-1_amd64.deb"),
            str(checkout / "build/packages/fedora-44/synodrive-dolphin-0.4.0-1.x86_64.rpm"),
        ]
        lines = success.stdout.rstrip().splitlines()
        assert lines[-2:] == expected_paths, success.stdout
        assert all(lines.count(path) == 1 for path in expected_paths), success.stdout

        unguarded = subprocess.run(
            [lifecycle, "DEB", root / "candidate.deb"], text=True, capture_output=True,
        )
        assert unguarded.returncode == 1, unguarded

        def assert_composed_reject(package_format, label, **overrides):
            (root / "docker.log").unlink(missing_ok=True)
            overrides["FAKE_TARGET_FORMAT"] = package_format
            result, result_calls = run(script, root, overrides=overrides)
            assert result.returncode != 0, (package_format, label, result)
            assert all(path not in result.stdout for path in expected_paths), result.stdout
            if package_format == "DEB":
                assert all("fedora-44" not in " ".join(call) for call in result_calls), result_calls
            return result_calls

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

        metadata_fields = {
            "DEB": [
                "${binary:Package}", "${Version}", "${Architecture}", "${Maintainer}",
                "${Homepage}", "${Section}", "${Priority}", "${binary:Summary}",
            ],
            "RPM": [
                "%{NAME}", "%{VERSION}", "%{RELEASE}", "%{ARCH}", "%{SUMMARY}",
                "%{LICENSE}", "%{URL}", "%{VENDOR}", "%{PACKAGER}",
            ],
        }
        plugin_paths = {
            "DEB": "/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/overlayicon/synodrive-overlay.so",
            "RPM": "/usr/lib64/qt6/plugins/kf6/overlayicon/synodrive-overlay.so",
        }
        archive_fields = {
            "DEB": ["Package", "Version", "Architecture", "Maintainer", "Homepage", "Section", "Priority", "Description"],
            "RPM": ["%{NAME}", "%{VERSION}", "%{RELEASE}", "%{ARCH}", "%{SUMMARY}", "%{LICENSE}", "%{URL}", "%{VENDOR}", "%{PACKAGER}"],
        }
        archive_dependencies = {
            "DEB": ["dolphin", "libnautilus-extension4", "libc6", "libgcc-s1", "libkf6coreaddons6", "libkf6kiocore6", "libkf6kiowidgets6", "libqt6core6t64", "libqt6widgets6", "libstdc++6"],
            "RPM": ["dolphin", "libnautilus-extension.so.4()(64bit)", "libc.so.6()(64bit)", "libgcc_s.so.1()(64bit)", "libKF6CoreAddons.so.6()(64bit)", "libKF6KIOCore.so.6()(64bit)", "libKF6KIOWidgets.so.6()(64bit)", "libQt6Core.so.6()(64bit)", "libQt6Widgets.so.6()(64bit)", "libstdc++.so.6()(64bit)"],
        }
        for package_format in ["DEB", "RPM"]:
            for field in archive_fields[package_format]:
                assert_composed_reject(
                    package_format, f"archive metadata {field}", FAKE_BAD_FIELD=field,
                )
            for variable in [
                "FAKE_EMPTY_PAYLOAD", "FAKE_ONE_RECORD", "FAKE_MISSING",
                "FAKE_MISSING_PLUGIN", "FAKE_DUPLICATE",
            ]:
                assert_composed_reject(
                    package_format, f"archive payload {variable}", **{variable: "1"},
                )
            assert_composed_reject(
                package_format, "archive extra payload",
                FAKE_EXTRA="/usr/share/synodrive-dolphin/unexpected",
            )
            for dependency in archive_dependencies[package_format]:
                assert_composed_reject(
                    package_format, f"archive missing dependency {dependency}",
                    FAKE_MISSING_DEPENDENCY=dependency,
                )
            assert_composed_reject(
                package_format, "archive empty dependencies", FAKE_EMPTY_DEPENDENCIES="1",
            )
            assert_composed_reject(
                package_format, "archive one dependency", FAKE_ONE_DEPENDENCY="1",
            )
            assert_composed_reject(
                package_format, "archive package scriptlet",
                **{f"FAKE_{package_format}_SCRIPTLET": "1"},
            )
            if package_format == "RPM":
                assert_composed_reject(
                    package_format, "archive package trigger", FAKE_RPM_TRIGGER="1",
                )
            (root / "docker.log").unlink(missing_ok=True)
            duplicate, duplicate_calls = run(
                script, root,
                overrides={
                    "FAKE_TARGET_FORMAT": package_format,
                    "FAKE_DUPLICATE_DEPENDENCY": "1",
                },
            )
            assert duplicate.returncode == 0, (package_format, "archive duplicate dependency")
            assert len(duplicate_calls) == 8, duplicate_calls

            stale = pathlib.Path(expected_paths[0 if package_format == "DEB" else 1])
            stale.parent.mkdir(parents=True, exist_ok=True)
            stale.write_text("stale")
            assert_composed_reject(
                package_format, "stale artifact cannot satisfy current run",
                FAKE_CPACK_NO_OUTPUT="1",
            )
            assert not stale.exists(), stale

        for package_format in ["DEB", "RPM"]:
            plugin_path = plugin_paths[package_format]
            paths = [
                "/usr/bin/synodrive-status",
                "/usr/bin/synodrive-tray-patch",
                "/usr/libexec/synodrive-dolphin/synodrive-action",
                plugin_path,
                plugin_path.replace("/overlayicon/synodrive-overlay.so", "/kfileitemaction/synodrive-fileitemaction.so"),
                "/usr/share/doc/synodrive-dolphin/copyright",
            ]
            for field in metadata_fields[package_format]:
                assert_composed_reject(
                    package_format, f"installed metadata {field}",
                    FAKE_LIFECYCLE_BAD_FIELD=field,
                )
            for kind in ["empty", "one", "missing", "duplicate", "extra", "duplicate-extra"]:
                assert_composed_reject(
                    package_format, f"installed inventory {kind}",
                    FAKE_LIFECYCLE_INVENTORY=kind,
                )
            assert_composed_reject(
                package_format, "installed symlink", FAKE_LIFECYCLE_SYMLINK=plugin_path,
            )
            for path in paths:
                mode = "755" if path in ["/usr/bin/synodrive-status", "/usr/bin/synodrive-tray-patch", "/usr/libexec/synodrive-dolphin/synodrive-action"] else "644"
                if package_format == "RPM" and "/kf6/" in path:
                    mode = "755"
                for value in [f"user:root {mode}", f"root:group {mode}", "root:root 600"]:
                    assert_composed_reject(
                        package_format, f"installed stat {path} {value}",
                        FAKE_BAD_STAT_PATH=path, FAKE_BAD_STAT_VALUE=value,
                    )
            for path in paths[:5]:
                assert_composed_reject(
                    package_format, f"installed file type {path}", FAKE_BAD_FILE_PATH=path,
                )
            assert_composed_reject(package_format, "license bytes", FAKE_BAD_LICENSE="1")
            assert_composed_reject(package_format, "command exit", FAKE_CLI_STATUS="1")
            assert_composed_reject(package_format, "command stdout", FAKE_CLI_STDOUT="unexpected")
            assert_composed_reject(package_format, "command stderr", FAKE_CLI_STDERR="wrong\n")
            loader_calls = assert_composed_reject(
                package_format, "command loader nonzero",
                FAKE_LDD_BAD_PATH="/usr/bin/synodrive-status",
                FAKE_LDD_OUTPUT="libc.so.6 => /lib/libc.so.6", FAKE_LDD_STATUS="1",
            )
            assert len(loader_calls) == (4 if package_format == "DEB" else 8), loader_calls
            assert_composed_reject(
                package_format, "command loader", FAKE_LDD_BAD_PATH="/usr/bin/synodrive-status",
            )
            assert_composed_reject(
                package_format, "later plugin undefined symbol", FAKE_LDD_BAD_PATH=plugin_path,
                FAKE_LDD_OUTPUT="undefined symbol: later_symbol",
            )
            assert_composed_reject(package_format, "Nautilus runtime", FAKE_MISSING_NAUTILUS="1")
            assert_composed_reject(package_format, "Synology path", FAKE_SYNOLOGY_PATH="1")
            assert_composed_reject(
                package_format, "post-removal Synology path", FAKE_RETAIN_SYNOLOGY="1",
            )
            assert_composed_reject(package_format, "Synology package", FAKE_SYNOLOGY_PACKAGE="1")
            assert_composed_reject(package_format, "Synology loader", FAKE_LDD_SYNOLOGY="1")
            integrity_calls = assert_composed_reject(
                package_format, "native integrity", FAKE_VERIFY_FAIL="1",
            )
            assert len(integrity_calls) == (4 if package_format == "DEB" else 8), integrity_calls
            assert sum(
                call[0] == "run" and "-runtime" in " ".join(call)
                for call in integrity_calls
            ) == 1 + (1 if package_format == "RPM" else 0), integrity_calls
            assert_composed_reject(package_format, "preinstalled package", FAKE_PREINSTALLED="1")
            for path in paths:
                assert_composed_reject(
                    package_format, f"preexisting path {path}", FAKE_PREEXISTING_PATH=path,
                )
            assert_composed_reject(
                package_format, "preexisting documentation directory",
                FAKE_PREEXISTING_PATH="/usr/share/doc/synodrive-dolphin",
                FAKE_PREEXISTING_DIRECTORY="1",
            )
            for path in paths:
                assert_composed_reject(
                    package_format, f"retained path {path}", FAKE_RETAIN_PATH=path,
                )
            assert_composed_reject(package_format, "retained package database", FAKE_RETAIN_DB="1")
            assert_composed_reject(package_format, "retained documentation directory", FAKE_RETAIN_DOC="1")

        (root / "docker.log").unlink()
        first_failure, calls = run(script, root, fail_image="synodrive-dolphin-ci:ubuntu-26.04")
        assert first_failure.returncode == 7, first_failure
        assert len(calls) == 2 and all("fedora-44" not in " ".join(call) for call in calls), calls
        assert "build/packages/ubuntu-26.04/" not in first_failure.stdout, first_failure.stdout

        (root / "docker.log").unlink()
        later_failure, calls = run(script, root, fail_image="synodrive-dolphin-ci:fedora-44")
        assert later_failure.returncode == 7, later_failure
        assert [call[0] for call in calls] == ["build", "run"] * 3, calls
        assert sum("fedora-44" in " ".join(call) and call[0] == "run" for call in calls) == 1, calls
        assert "build/packages/ubuntu-26.04/" not in later_failure.stdout, later_failure.stdout

        (root / "docker.log").unlink()
        first_lifecycle_failure, calls = run(
            script, root, fail_image="synodrive-dolphin-ci:ubuntu-26.04-runtime",
        )
        assert first_lifecycle_failure.returncode == 7, first_lifecycle_failure
        assert len(calls) == 4 and all("fedora-44" not in " ".join(call) for call in calls), calls

        (root / "docker.log").unlink()
        later_lifecycle_failure, calls = run(
            script, root, fail_image="synodrive-dolphin-ci:fedora-44-runtime",
        )
        assert later_lifecycle_failure.returncode == 7, later_lifecycle_failure
        assert len(calls) == 8, calls
        assert all(path not in later_lifecycle_failure.stdout for path in expected_paths)


if __name__ == "__main__":
    main()
