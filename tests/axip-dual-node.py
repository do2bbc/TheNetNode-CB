import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import shutil
import socket
import struct
import subprocess
import tempfile
import termios
import time


ROOT = Path(__file__).resolve().parent.parent


def crc(data):
    fcs = 0xFFFF
    for byte in data:
        fcs ^= byte
        for _ in range(8):
            fcs = (fcs >> 1) ^ (0x8408 if fcs & 1 else 0)
    return struct.pack("<H", fcs ^ 0xFFFF)


def frame(source):
    def address(call, last, command=False):
        return bytes(ord(c) << 1 for c in call.ljust(6)) + bytes(
            [0x60 | last | (0x80 if command else 0)]
        )

    data = address("XX0XX", 0, True) + address(source, 1) + b"\x3f"
    return data + crc(data)


def receive_reply(peer, call, port):
    deadline = time.monotonic() + 10
    expected = bytes(ord(c) << 1 for c in call)
    while time.monotonic() < deadline:
        peer.settimeout(deadline - time.monotonic())
        response, origin = peer.recvfrom(2048)
        if len(response) >= 17 and response[:6] == expected and response[14] & 0xEF == 0x63:
            assert origin[1] == port
            assert response[-2:] == crc(response[:-2])
            return
    raise AssertionError(f"No AX.25 reply to {call}")


def main():
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        shutil.copy(ROOT / "packaging/debian/tnn179.pas", work)
        shutil.copy(ROOT / "packaging/debian/tnn.ini", work)
        for folder in ("textcmd", "pacsat", "httpd", "speech", "msg"):
            (work / folder).mkdir()
        with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as probe:
            probe.bind(("::1", 0))
            port = probe.getsockname()[1]
        (work / "tnn179.tnb").write_text(
            f"ATTACH PORT=12 KTYP=AX25IP DEVICE={port}\n"
            "PORT 12 NAME=AX25IP ON\n"
            f"AXIPR U {port}\n"
            "AXIPR R + STATIC 127.0.0.1 U 10093\n"
            "AXIPR R + STATIC ::1 U 10093\n"
            "AXIPR R + SCOPED fe80::1%lo U 10093\n"
            "AXIPR D + ::1 U 10093\n"
            "AXIPR R + SEED 127.0.0.1 U 10093\n"
        )
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
        process = subprocess.Popen(
            [str(ROOT / "bin/tnn"), "-v"], cwd=work,
            stdin=slave, stdout=slave, stderr=slave,
            env={**os.environ, "SHELL": "/bin/sh", "TERM": "xterm"},
            start_new_session=True,
        )
        os.close(slave)
        captured = bytearray()

        def wait_for(text, timeout=30):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if text in captured:
                    return
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
                if process.poll() is not None:
                    break
            raise AssertionError(f"Missing {text!r} in node output")

        try:
            wait_for(b"password:")
            os.write(master, b"TESTONLY\r")
            wait_for(b"Welcome to")
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                if (work / "startup.log").exists():
                    startup = (work / "startup.log").read_text(errors="replace")
                    if "Call is registerd." in startup.partition("AXIPR R + SEED")[2]:
                        break
                time.sleep(0.1)
            else:
                raise AssertionError("Node startup configuration did not finish")
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as peer4, \
                 socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as peer6:
                peer4.bind(("127.0.0.1", 0))
                peer6.bind(("::1", 0))
                peer4.sendto(frame("P4TEST"), ("127.0.0.1", port))
                peer6.sendto(frame("P6TEST"), ("::1", port))
                for peer, call in ((peer4, "P4TEST"), (peer6, "P6TEST")):
                    receive_reply(peer, call, port)
                peer6.sendto(frame("P4TEST"), ("::1", port))
                receive_reply(peer6, "P4TEST", port)
                peer4.sendto(frame("P4TEST"), ("127.0.0.1", port))
                receive_reply(peer4, "P4TEST", port)
                port4 = peer4.getsockname()[1]
                port6 = peer6.getsockname()[1]
            os.write(master, b"\x1bc\r")
            wait_for(b"CONNECTED to")
            os.write(master, b"AXIPR\r")
            wait_for(b"AXIP-Routes:")
            wait_for(str(port6).encode())
            assert b"P4TEST     127.0.0.1" in captured
            assert f"UDP   {port4}".encode() in captured
            assert b"P6TEST     ::1" in captured
            assert b"STATIC     ::1" in captured
            assert b"SCOPED     fe80::1%lo" in captured
            os.write(master, b"SPARAM\r")
            wait_for(b".TNB saved...")
            saved = (work / "tnn179.tnb").read_text(errors="replace")
            assert "AXIPR R + STATIC ::1 U 10093" in saved
            assert "AXIPR R + SCOPED fe80::1%lo U 10093" in saved
            assert "AXIPR D + ::1 U 10093" in saved
            assert "P4TEST" not in saved and "P6TEST" not in saved
            os.write(master, b"\x1bQUIT\r")
            wait_for(b"Programm wird beendet")
            assert process.wait(timeout=15) == 0
        except BaseException:
            print(captured.decode("latin1"))
            if (work / "startup.log").exists():
                print((work / "startup.log").read_text(errors="replace"))
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
        for new_parser in (False, True):
            master, slave = pty.openpty()
            fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
            process = subprocess.Popen(
                [str(ROOT / "bin/tnn"), "-v"], cwd=work,
                stdin=slave, stdout=slave, stderr=slave,
                env={**os.environ, "SHELL": "/bin/sh", "TERM": "xterm"},
                start_new_session=True,
            )
            os.close(slave)
            captured = bytearray()
            try:
                wait_for(b"password:")
                os.write(master, b"TESTONLY\r")
                wait_for(b"Welcome to")
                marker = "AXIPR ADD DEFAULT ::1" if new_parser else "AXIPR D + ::1"
                deadline = time.monotonic() + 30
                while time.monotonic() < deadline:
                    startup = (work / "startup.log").read_text(errors="replace")
                    if "Call is registerd." in startup.partition(marker)[2]:
                        break
                    time.sleep(0.1)
                else:
                    raise AssertionError("Saved IPv6 routes did not reload")
                os.write(master, b"\x1bc\r")
                wait_for(b"CONNECTED to")
                os.write(master, b"AXIPR\r")
                wait_for(b"default    ::1")
                assert b"STATIC     ::1" in captured
                if not new_parser:
                    os.write(master, b"AXIPR P\r")
                    wait_for(b"Switched to new axipr-parser")
                    os.write(master, b"AXIPR ADD NEWV6 ::1 UDP 10093\r")
                    wait_for(b"Call is registerd.")
                    os.write(master, b"SPARAM\r")
                    wait_for(b".TNB saved...")
                    saved = (work / "tnn179.tnb").read_text(errors="replace")
                    assert "AXIPR ADD NEWV6 ::1 UDP 10093" in saved
                    assert "AXIPR ADD DEFAULT ::1 UDP 10093" in saved
                    assert "AXIPR LOOKUP 180" in saved
                    assert "AXIPR U " not in saved
                else:
                    assert b"NEWV6      ::1" in captured
                    assert "Invalid Mode" not in startup
                    assert "out of range" not in startup
                os.write(master, b"\x1bQUIT\r")
                wait_for(b"Programm wird beendet")
                assert process.wait(timeout=15) == 0
            except BaseException:
                print(captured.decode("latin1"))
                if (work / "startup.log").exists():
                    print((work / "startup.log").read_text(errors="replace"))
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
    print("AXIP IPv4 and IPv6 node integration passed")


if __name__ == "__main__":
    main()
