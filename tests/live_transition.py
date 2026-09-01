#!/usr/bin/env python3

import os
import pathlib
import subprocess
import sys
import threading
import time


def response(process, path):
    process.stdin.write(os.fsencode(path) + b"\0")
    process.stdin.flush()
    data = bytearray()
    while True:
        byte = process.stdout.read(1)
        if byte == b"\0":
            return data.decode()
        if not byte:
            raise RuntimeError("synodrive-status ended before its response")
        data.extend(byte)


def wait_for(process, path, wanted, timeout):
    seen = []
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status = response(process, path)
        if not seen or seen[-1] != status:
            seen.append(status)
        if status == wanted:
            return seen
        time.sleep(0.01)
    raise RuntimeError(f"did not observe {wanted}; saw {seen}")


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    sync_root = pathlib.Path(sys.argv[2]).resolve()
    # Synology ignores hidden files, so the acceptance file must not start with a dot.
    name = f"synodrive-overlay-test-{time.time_ns()}"
    test_file = sync_root / name
    if test_file.exists():
        raise RuntimeError("unique acceptance file already exists")

    process = subprocess.Popen(
        [binary, "--stdio"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, env=os.environ
    )
    sequence = []
    try:
        test_file.write_bytes(b"initial\n")
        sequence.extend(wait_for(process, test_file, "synced", 180))

        def modify():
            with test_file.open("ab", buffering=0) as output:
                block = b"x" * (1024 * 1024)
                for _ in range(32):
                    output.write(block)
                    os.fsync(output.fileno())
                    time.sleep(0.02)

        writer = threading.Thread(target=modify)
        writer.start()
        sequence.extend(wait_for(process, test_file, "syncing", 10))
        writer.join()
        sequence.extend(wait_for(process, test_file, "synced", 120))
        if not all(value in sequence for value in ("synced", "syncing")):
            raise RuntimeError(f"incomplete transition: {sequence}")
        print("sequence=" + "->".join(sequence))
        print("wrapper_alive=yes" if process.poll() is None else "wrapper_alive=no")
        print("test_file=" + name)
    finally:
        test_file.unlink(missing_ok=True)
        process.stdin.close()
        process.wait(timeout=10)
        if test_file.exists():
            raise RuntimeError("acceptance file cleanup failed")
        print("cleanup=absent")


if __name__ == "__main__":
    main()
