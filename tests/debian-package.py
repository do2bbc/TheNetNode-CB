import errno
import fcntl
import os
from pathlib import Path
import pty
import re
import select
import shutil
import struct
import subprocess
import sys
import tempfile
import termios
import time


def main(package, arch):
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        extracted = root / "extracted"
        subprocess.run(["dpkg-deb", "-x", package, extracted], check=True)
        subprocess.run(["dpkg-deb", "-e", package, root / "control"], check=True)
        subprocess.run(["md5sum", "-c", root / "control/md5sums"],
                       cwd=extracted, check=True, stdout=subprocess.DEVNULL)
        assert not any((root / "control" / name).exists()
                       for name in ("preinst", "postinst", "prerm", "postrm"))
        assert not (extracted / "etc").exists()
        assert not (extracted / "var").exists()
        assert not (extracted / "usr/local").exists()
        assert not list(extracted.rglob("*.service"))
        doc = extracted / "usr/share/doc/tnn-cb"
        assert "Source commit:" in (doc / "BUILD.txt").read_text()
        run = root / "run"
        shutil.copytree(doc / "examples", run)
        for folder in ("textcmd", "pacsat", "httpd", "speech", "msg"):
            (run / folder).mkdir()
        for filename in ("ohu.txt", "ohs.txt", "config.top"):
            shutil.copy(extracted / "usr/share/tnn-cb" / filename, run)
        node = extracted / "usr/bin/tnn-cb"
        emulator = {
            "amd64": [],
            "arm64": ["qemu-aarch64", "-L", "/usr/aarch64-linux-gnu"],
            "armhf": ["qemu-arm", "-L", "/usr/arm-linux-gnueabihf"],
        }[arch]
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
        process = subprocess.Popen(
            [*emulator, str(node), "-v"], cwd=run,
            stdin=slave, stdout=slave, stderr=slave,
            env={**os.environ, "SHELL": "/bin/sh", "TERM": "xterm"},
            start_new_session=True)
        os.close(slave)
        captured = bytearray()

        def wait_for(text, timeout=30):
            start = len(captured)
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]:
                    try:
                        chunk = os.read(master, 16384)
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                        break
                    if not chunk:
                        break
                    captured.extend(chunk)
                    if text in captured[start:]:
                        return
                if process.poll() is not None:
                    break
            raise AssertionError(f"Missing {text!r} in node output")

        try:
            wait_for(b"password:")
            assert b"(Linux) (CB)" in captured
            assert re.search(rb"1\.79cb\d+", captured)
            os.write(master, b"TESTONLY\r")
            wait_for(b"Welcome to")
            os.write(master, b"\x1bQUIT\r")
            wait_for(b"Programm wird beendet")
            assert process.wait(timeout=15) == 0
        except BaseException:
            print(captured.decode("latin1"), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            os.close(master)
        if os.geteuid() == 0:
            install_root = root / "installed"
            sentinels = [
                install_root / "usr/local/tnn/tnn179.pas",
                install_root / "var/lib/tnn-cb/tnn179.pas",
            ]
            for sentinel in sentinels:
                sentinel.parent.mkdir(parents=True, exist_ok=True)
                sentinel.write_text("existing configuration must survive\n")
            for action, target in [
                ("--install", package), ("--install", package),
                ("--remove", "tnn-cb"), ("--install", package),
                ("--purge", "tnn-cb"),
            ]:
                subprocess.run([
                    "dpkg", f"--root={install_root}", "--force-architecture",
                    "--force-depends", action, target,
                ], check=True, stdout=subprocess.DEVNULL)
                for sentinel in sentinels:
                    assert sentinel.read_text() == "existing configuration must survive\n"
                assert (install_root / "usr/bin/tnn-cb").exists() == (action == "--install")
            print(f"{arch}: isolated install/reinstall/remove/purge preserved existing configuration")
        print(f"{arch}: package checksums, offline startup, login and clean exit passed")


if __name__ == "__main__":
    main(str(Path(sys.argv[1]).resolve()), sys.argv[2])
