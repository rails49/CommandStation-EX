"""Tests of check.py's transport and subcommand table, against a fake mirror.

    python3 -B -m unittest rails49/test_check.py
"""

from __future__ import annotations

import contextlib
import io
import os
import socket
import sys
import threading
import unittest
import unittest.mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check  # noqa: E402


class FakeMirror:
    """A line-based TCP server that answers <=> like the station does, and
    broadcasts a line a moment later, as another client's command would."""

    def __init__(self):
        self.server = socket.create_server(("127.0.0.1", 0))
        self.port = self.server.getsockname()[1]
        self.received = []
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        try:
            conn, _ = self.server.accept()
        except OSError:  # closed before anyone connected
            return
        with conn:
            buf = b""
            while True:
                data = conn.recv(4096)
                if not data:
                    return
                buf += data
                *done, buf = buf.split(b"\n")
                for line in done:
                    self.received.append(line.decode())
                    if line == b"<=>":
                        conn.sendall(b"<= A MAIN>\n<= D MAIN A>\n")
                        threading.Event().wait(0.05)
                        conn.sendall(b"<p1 A>\n")

    def close(self):
        self.server.close()


def run(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = check.main(argv)
    return code, out.getvalue()


def free_port():
    with socket.create_server(("127.0.0.1", 0)) as s:
        return s.getsockname()[1]


class SendTest(unittest.TestCase):
    def setUp(self):
        self.mirror = FakeMirror()
        self.addCleanup(self.mirror.close)

    def test_send_prints_the_reply_and_broadcasts(self):
        code, out = run(["--port", str(self.mirror.port), "--window", "0.3",
                         "send", "<=>"])
        self.assertEqual(code, 0)
        self.assertEqual(self.mirror.received, ["<=>"])
        self.assertEqual(out.splitlines(), [
            ">>> <=>", "    <= A MAIN>", "    <= D MAIN A>", "    <p1 A>"])

    def test_no_mirror_is_exit_2(self):
        code, out = run(["--port", str(free_port()), "send", "<=>"])
        self.assertEqual(code, 2)
        self.assertIn("cannot talk to", out)


class TableTest(unittest.TestCase):
    def test_a_check_and_its_arguments_come_from_the_table_alone(self):
        seen = {}

        def probe(st, args):
            """A check added by a later ticket."""
            seen["level"] = args.level

        mirror = FakeMirror()
        self.addCleanup(mirror.close)
        table = dict(check.CHECKS)
        table["probe"] = check.Check(
            probe, [("--level", dict(type=int, default=3))])
        with unittest.mock.patch.object(check, "CHECKS", table):
            code, out = run(["--port", str(mirror.port), "--window", "0.1",
                             "probe", "--level", "7"])
        self.assertEqual(code, 0)
        self.assertEqual(seen, {"level": 7})
        self.assertIn("probe: passed", out)

    def test_send_is_in_the_table(self):
        self.assertIn("send", check.CHECKS)


if __name__ == "__main__":
    unittest.main()
