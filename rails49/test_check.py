# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
"""Tests of check.py's transport and subcommand table, against a fake mirror.

    PYTHONDONTWRITEBYTECODE=1 uv run rails49/test_check.py
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


GUARD_REPLY = (
    b"<* GUARD A MAIN inv=0 gpio=0 active=1 acc=0us last=- trip=1200/1999mA *>\n"
    b"<* GUARD D MAIN A inv=1 gpio=1 active=1 acc=0us last=5321ms trip=1200/1999mA *>\n"
    b"<* GUARD ticks=9998/s max=41us *>\n")

# Every reply must arrive within the window; the margin is for a busy CI
# runner.
WINDOW = "0.3"


def setUpModule():
    try:
        socket.create_server(("127.0.0.1", 0)).close()
    except PermissionError:
        raise unittest.SkipTest("no local TCP port can be opened here")


class FakeMirror:
    """A line-based TCP server that answers <=> and <D GUARD> like the
    station does, and after <=> broadcasts a line a moment later, as another
    client's command would."""

    def __init__(self, guard_reply=GUARD_REPLY, replies=None):
        self.guard_reply = guard_reply
        # command -> lines, for a check's script; a list of them is played
        # one per time the command comes, the last one repeated
        self.replies = {command: list(reply) if isinstance(reply, list)
                        else reply for command, reply in (replies or {}).items()}
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
                    if line in self.replies:
                        conn.sendall(self.reply(line))
                    elif line == b"<=>":
                        conn.sendall(b"<= A MAIN>\n<= D MAIN A>\n")
                        threading.Event().wait(0.05)
                        conn.sendall(b"<p1 A>\n")
                    elif line == b"<D GUARD>":
                        conn.sendall(self.guard_reply)

    def reply(self, line):
        reply = self.replies[line]
        if isinstance(reply, list):
            return reply.pop(0) if len(reply) > 1 else reply[0]
        return reply

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
        code, out = run(["--port", str(self.mirror.port), "--window", WINDOW,
                         "send", "<=>"])
        self.assertEqual(code, 0)
        self.assertEqual(self.mirror.received, ["<=>"])
        self.assertEqual(out.splitlines(), [
            ">>> <=>", "    <= A MAIN>", "    <= D MAIN A>", "    <p1 A>"])

    def test_no_mirror_is_exit_2(self):
        code, out = run(["--port", str(free_port()), "send", "<=>"])
        self.assertEqual(code, 2)
        self.assertIn("cannot talk to", out)


class GuardTest(unittest.TestCase):
    def mirror(self, reply=GUARD_REPLY):
        mirror = FakeMirror(reply)
        self.addCleanup(mirror.close)
        return mirror

    def test_guard_sends_d_guard_and_prints_the_lines(self):
        mirror = self.mirror()
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "guard"])
        self.assertEqual(code, 0)
        self.assertEqual(mirror.received, ["<D GUARD>"])
        for line in GUARD_REPLY.decode().splitlines():
            self.assertIn("    " + line, out.splitlines())
        self.assertIn("guard: passed", out)

    def test_parse_guard_counters(self):
        self.assertEqual(
            check.parse_guard_counters(GUARD_REPLY.decode().splitlines()),
            check.GuardCounters(9998, 41))
        self.assertIsNone(check.parse_guard_counters(["<* GUARD A x *>"]))

    def test_a_build_without_d_guard_fails(self):
        mirror = self.mirror(b"")
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "guard"])
        self.assertEqual(code, 1)
        self.assertIn("guard: FAILED", out)


# What the mirror shows for a wire across D's rails held through one retry:
# the Guard cuts, the retry is cut again, then the wire is gone.
SHORT = [
    b"<* TRACK D INVERT (fast) *>",
    b"<= D MAIN A I>",
    b"<* TRACK D FAST OVERLOAD 3100mA. Pause 40ms *>",
    b"<p0 D>",
    b"<* TRACK D POWER RESTORE (after 40ms) *>",
    b"<* TRACK D FAST OVERLOAD 3100mA. Pause 80ms *>",
    b"<p0 D>",
    b"<* TRACK D POWER RESTORE (after 80ms) *>",
    b"<* TRACK D NORMAL (after 20ms/21ms) 12mA *>",
    b"<p1 D>",
]


class OverloadBroadcastTest(unittest.TestCase):
    def check(self, short):
        reply = b"".join(line + b"\n" for line in short)
        mirror = FakeMirror(replies={b"<1 D>": reply})
        self.addCleanup(mirror.close)
        return run(["--port", str(mirror.port), "--window", WINDOW,
                    "overload-broadcast", "--wait", "0.2"])

    def test_p0_on_every_entry_and_p1_when_back_on(self):
        code, out = self.check(SHORT)
        self.assertEqual(code, 0, out)
        self.assertIn("overload-broadcast: passed", out)

    def test_an_entry_without_p0_fails(self):
        code, out = self.check(SHORT[:6] + SHORT[7:])  # the retry's <p0 D>
        self.assertEqual(code, 1)
        self.assertIn("overload-broadcast: FAILED", out)

    def test_no_p1_after_the_short_fails(self):
        code, out = self.check(SHORT[:-1])
        self.assertEqual(code, 1)
        self.assertIn("overload-broadcast: FAILED", out)

    def test_p0_late_fails(self):
        # <p0 D> only after the first retry: the host read D as on meanwhile
        code, out = self.check(SHORT[:3] + SHORT[4:])
        self.assertEqual(code, 1)


# <D GUARD> after <= D INV> and then <= A MAIN> on a build whose waveform
# setup attaches every MAIN pin un-inverted (finding 1 of the review): D's
# flag still says inverted, its GPIO bit does not.
GUARD_REPLY_D_RESET = GUARD_REPLY.replace(b"inv=1 gpio=1", b"inv=1 gpio=0")


class PolaritySurvivesTest(unittest.TestCase):
    def check(self, reply):
        mirror = FakeMirror(reply)
        self.addCleanup(mirror.close)
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "polarity-survives"])
        return mirror, code, out

    def test_d_inverted_and_a_plain_after_a_mode_command_on_a(self):
        mirror, code, out = self.check(GUARD_REPLY)
        self.assertEqual(code, 0, out)
        self.assertEqual(mirror.received, [
            "<= D MAIN_AUTO>", "<= D INV>", "<= A MAIN>", "<D GUARD>"])
        self.assertIn("polarity-survives: passed", out)

    def test_d_reset_in_hardware_by_the_command_on_a_fails(self):
        mirror, code, out = self.check(GUARD_REPLY_D_RESET)
        self.assertEqual(code, 1)
        self.assertIn("FAIL D gpio invert bit set", out)
        self.assertIn("polarity-survives: FAILED", out)


# What the station answers to no-cut's commands when only a base mode change
# cuts power (ADR-0005). Each <= broadcasts D's state; <s> its power.
NO_CUT = {
    b"<1 D>": b"<p1 D>\n",
    b"<= D MAIN_AUTO_INV>": b"<= D MAIN A I>\n",
    b"<= D MAIN_AUTO>": b"<= D MAIN A>\n",
    b"<= D MAIN>": b"<= D MAIN>\n",
    b"<= D INV>": b"<= D MAIN I>\n",
    b"<s>": b"<iDCC-EX V-5.4.0 / ESP32 / x / y>\n<p1 A>\n<p1 D>\n",
    b"<= D NONE>": b"<p0 D>\n<= D NONE>\n",
}


class NoCutTest(unittest.TestCase):
    def check(self, replies):
        mirror = FakeMirror(replies=replies)
        self.addCleanup(mirror.close)
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "no-cut"])
        return mirror, code, out

    def test_modifier_changes_and_repeats_keep_d_on(self):
        mirror, code, out = self.check(NO_CUT)
        self.assertEqual(code, 0, out)
        self.assertEqual(mirror.received[:9], [
            "<= D MAIN_AUTO>", "<1 D>", "<= D MAIN_AUTO_INV>",
            "<= D MAIN_AUTO>", "<= D MAIN_AUTO>", "<= D MAIN>", "<= D INV>",
            "<= D MAIN_AUTO>", "<s>"])
        self.assertEqual(mirror.received[9], "<= D NONE>")
        self.assertIn("no-cut: passed", out)

    def test_a_cut_on_an_inv_change_fails(self):
        replies = dict(NO_CUT)
        replies[b"<= D MAIN_AUTO>"] = b"<p0 D>\n<= D MAIN A>\n"
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("no-cut: FAILED", out)

    def test_d_off_afterwards_fails(self):
        replies = dict(NO_CUT)
        replies[b"<s>"] = b"<p1 A>\n<p0 D>\n"
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("FAIL <s> reports D on", out)

    def test_no_cut_on_a_base_mode_change_fails(self):
        replies = dict(NO_CUT)
        replies[b"<= D NONE>"] = b"<= D NONE>\n"
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("FAIL <p0 D> after NONE", out)


# What <=> answers after reports' commands when it names D from the hardware
# invert flag: inverted after <= D INV, normal after <= D MAIN_AUTO.
REPORTS = {
    b"<= D MAIN_AUTO>": b"<= D MAIN A>\n",
    b"<= D INV>": b"<= D MAIN A I>\n",
    b"<=>": [b"<= A MAIN>\n<= D MAIN A I>\n", b"<= A MAIN>\n<= D MAIN A>\n"],
}


class ReportsTest(unittest.TestCase):
    def check(self, replies):
        mirror = FakeMirror(replies=replies)
        self.addCleanup(mirror.close)
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "reports"])
        return mirror, code, out

    def test_d_reads_main_a_i_inverted_and_main_a_after_main_auto(self):
        mirror, code, out = self.check(REPORTS)
        self.assertEqual(code, 0, out)
        self.assertEqual(mirror.received, [
            "<= D MAIN_AUTO>", "<= D INV>", "<=>", "<= D MAIN_AUTO>", "<=>"])
        self.assertIn("reports: passed", out)

    def test_a_report_from_the_mode_word_fails(self):
        # INV set on the mode, but the name taken from AUTO alone
        replies = dict(REPORTS)
        replies[b"<=>"] = b"<= A MAIN>\n<= D MAIN A>\n"
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("FAIL <=> reports <= D MAIN A I>", out)


# What the mirror shows for a loco crossing D's gap with the wrong Polarity:
# the Guard reverses D, and the main loop broadcasts D's state line and then
# the diagnostic (ADR-0005).
TRAIN = [
    b"<p1 D>",
    b"<= D MAIN A I>",
    b"<p1 A>",
    b"<p1 D>",
    b"<* TRACK D INVERT (fast) *>",
]


class ReversalBroadcastTest(unittest.TestCase):
    def check(self, train):
        reply = b"".join(line + b"\n" for line in train)
        mirror = FakeMirror(replies={b"<= D MAIN_AUTO>": b"<= D MAIN A>\n",
                                     b"<1 D>": reply})
        self.addCleanup(mirror.close)
        return run(["--port", str(mirror.port), "--window", WINDOW,
                    "reversal-broadcast", "--wait", "0.2"])

    def test_state_line_next_to_the_reversal(self):
        code, out = self.check(TRAIN)
        self.assertEqual(code, 0, out)
        self.assertIn("reversal-broadcast: passed", out)

    def test_state_line_only_after_the_diagnostic_fails(self):
        code, out = self.check([TRAIN[0], TRAIN[4], TRAIN[1]])
        self.assertEqual(code, 1)
        self.assertIn("reversal-broadcast: FAILED", out)

    def test_diagnostic_alone_fails(self):
        code, out = self.check([TRAIN[0], TRAIN[4]])
        self.assertEqual(code, 1)
        self.assertIn("reversal-broadcast: FAILED", out)

    def test_state_line_of_an_earlier_event_fails(self):
        code, out = self.check([TRAIN[1], b"<* TRACK D ALERT 900mA *>",
                                TRAIN[4]])
        self.assertEqual(code, 1)

    def test_no_reversal_fails(self):
        code, out = self.check(TRAIN[:4])
        self.assertEqual(code, 1)
        self.assertIn("FAIL a Reversal of D was seen", out)


# <D GUARD> on a fresh boot, then after <JG D 300>: D's trip value follows
# at once, read back through the ADC's raw counts.
GUARD_REPLY_D_300 = GUARD_REPLY.replace(
    b"last=5321ms trip=1200/1999mA", b"last=5321ms trip=180/299mA")
TRIP = {
    b"<JG>": b"<jG 2000 2000 2000 2000>\n",
    b"<JG D 300>": b"<jG 2000 2000 2000 300>\n",
    b"<JG D 2000>": b"<jG 2000 2000 2000 2000>\n",
    b"<D GUARD>": [GUARD_REPLY, GUARD_REPLY_D_300],
}


class TripFollowsJgTest(unittest.TestCase):
    def check(self, guard_replies):
        replies = dict(TRIP)
        replies[b"<D GUARD>"] = guard_replies
        mirror = FakeMirror(replies=replies)
        self.addCleanup(mirror.close)
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "trip-follows-jg"])
        return mirror, code, out

    def test_every_track_has_a_trip_value_and_d_follows_jg(self):
        mirror, code, out = self.check(TRIP[b"<D GUARD>"])
        self.assertEqual(code, 0, out)
        self.assertEqual(mirror.received, [
            "<JG>", "<D GUARD>", "<JG D 300>", "<D GUARD>", "<JG D 2000>"])
        self.assertIn("trip-follows-jg: passed", out)

    def test_a_stale_trip_value_fails_and_the_limit_is_restored(self):
        mirror, code, out = self.check([GUARD_REPLY])
        self.assertEqual(code, 1)
        self.assertIn("FAIL D trips at 1999mA", out)
        self.assertEqual(mirror.received[-1], "<JG D 2000>")

    def test_a_track_without_a_trip_value_at_boot_fails(self):
        # the scratch value before the main loop's first pass over A
        unset = GUARD_REPLY.replace(b"last=- trip=1200/1999mA",
                                    b"last=- trip=0/0mA")
        mirror, code, out = self.check([unset, GUARD_REPLY_D_300])
        self.assertEqual(code, 1)
        self.assertIn("FAIL A has a trip value", out)
        self.assertNotIn("<JG D 300>", mirror.received)

    def test_a_build_without_trip_values_fails(self):
        old = GUARD_REPLY.replace(b" trip=1200/1999mA", b"")
        mirror, code, out = self.check([old])
        self.assertEqual(code, 1)
        self.assertIn("trip-follows-jg: FAILED", out)


# <D GUARD> while joined after <JG B 600>: B reads MAIN and trips at the set
# limit, read back through the ADC's raw counts.
GUARD_REPLY_JOINED = (
    b"<* GUARD A MAIN inv=0 gpio=0 active=1 acc=0us last=- trip=1200/1999mA *>\n"
    b"<* GUARD B MAIN inv=0 gpio=0 active=1 acc=0us last=- trip=360/599mA *>\n"
    b"<* GUARD ticks=9998/s max=41us *>\n")
# What the station answers to join-keeps-jg's commands when the limit holds
# through the join and the ack, and the power lines of each change of Join
# report the new state. B is the PROG track, never given a limit, and every
# track is on.
JOIN = {
    b"<=>": b"<= A MAIN>\n<= B PROG>\n<= C MAIN A>\n<= D MAIN A>\n",
    b"<JG>": b"<jG 2000 250 2000 2000>\n",
    b"<JG B 600>": b"<jG 2000 600 2000 2000>\n",
    b"<0 PROG>": b"<= B PROG>\n<p1 A>\n<p1 B>\n<p1>\n<p1 MAIN>\n<p1 PROG>\n"
                 b"<jG 2000 600 2000 2000>\n<p1 A>\n<p0 B>\n<p1 MAIN>\n",
    b"<1 JOIN>": b"<= B MAIN>\n<p1 A>\n<p1 B>\n<p1>\n<p1 JOIN>\n"
                 b"<jG 2000 600 2000 2000>\n",
    b"<D GUARD>": GUARD_REPLY_JOINED,
    # Join ends, the translator's <s> comes in between, Join is restored
    b"<R>": b"<= B PROG>\n<p1 A>\n<p1 B>\n<p1>\n<p1 MAIN>\n<p1 PROG>\n"
            b"<jG 2000 600 2000 2000>\n<iDCC-EX V-5.6.3 / ESP32 / x / y>\n"
            b"<p1 A>\n<p1 B>\n<p1>\n<p1 MAIN>\n<p1 PROG>\n"
            b"<* TRACK B ALERT FAULT *>\n<= B MAIN>\n<p1 A>\n<p0 B>\n"
            b"<p1 JOIN>\n<jG 2000 600 2000 2000>\n<r -1>\n",
    b"<JG B 250>": b"<jG 2000 250 2000 2000>\n",
}


class JoinKeepsJgTest(unittest.TestCase):
    def check(self, **changes):
        replies = dict(JOIN)
        replies.update({k.encode(): v for k, v in changes.items()})
        mirror = FakeMirror(replies=replies)
        self.addCleanup(mirror.close)
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "join-keeps-jg", "--wait", "0.5"])
        return mirror, code, out

    def test_the_limit_holds_while_joined_and_after_an_ack(self):
        mirror, code, out = self.check()
        self.assertEqual(code, 0, out)
        self.assertEqual(mirror.received, [
            "<=>", "<JG>", "<JG B 600>", "<1 JOIN>", "<D GUARD>", "<R>",
            "<D GUARD>", "<0 PROG>", "<JG B 250>"])
        self.assertIn("join-keeps-jg: passed", out)

    def test_no_join_line_fails_and_b_is_restored(self):
        mirror, code, out = self.check(**{
            "<1 JOIN>": b"<p1 A>\n<p1 B>\n<p1 MAIN>\n<p1 PROG>\n"})
        self.assertEqual(code, 1)
        self.assertIn("FAIL <p1 JOIN> after <1 JOIN>", out)
        self.assertEqual(mirror.received[-2:], ["<0 PROG>", "<JG B 250>"])

    def test_the_board_limit_after_the_ack_fails(self):
        board = GUARD_REPLY_JOINED.replace(b"trip=360/599mA",
                                           b"trip=2999/4999mA")
        mirror, code, out = self.check(**{
            "<D GUARD>": [GUARD_REPLY_JOINED, board]})
        self.assertEqual(code, 1)
        self.assertIn("FAIL B trips at 4999mA", out)
        self.assertEqual(mirror.received[-2:], ["<0 PROG>", "<JG B 250>"])

    def test_join_lines_with_the_previous_state_fail(self):
        # before #75: each change's power lines report Join as it was
        mirror, code, out = self.check(**{
            "<R>": b"<= B PROG>\n<p1 A>\n<p1 B>\n<p1>\n<p1 JOIN>\n"
                   b"<jG 2000 600 2000 2000>\n<= B MAIN>\n<p1 A>\n<p0 B>\n"
                   b"<p1 MAIN>\n<jG 2000 600 2000 2000>\n<r -1>\n"})
        self.assertEqual(code, 1)
        self.assertIn("FAIL no <p1 JOIN> as Join ends", out)
        self.assertEqual(mirror.received[-2:], ["<0 PROG>", "<JG B 250>"])

    def test_no_join_line_as_join_is_restored_fails(self):
        mirror, code, out = self.check(**{
            "<R>": b"<= B PROG>\n<p1 A>\n<p1 B>\n<p1 MAIN>\n<p1 PROG>\n"
                   b"<jG 2000 600 2000 2000>\n<= B MAIN>\n<p1 A>\n<p1 B>\n"
                   b"<p1 MAIN>\n<jG 2000 600 2000 2000>\n<r -1>\n"})
        self.assertEqual(code, 1)
        self.assertIn("FAIL <p1 JOIN> as Join is restored", out)

    def test_no_join_line_on_join_with_every_track_on_fails(self):
        mirror, code, out = self.check(**{
            "<1 JOIN>": b"<= B MAIN>\n<p1 A>\n<p1 B>\n<p1>\n<p1 MAIN>\n"
                        b"<jG 2000 600 2000 2000>\n"})
        self.assertEqual(code, 1)
        self.assertIn("FAIL <p1 JOIN> after <1 JOIN>", out)

    def test_no_reply_to_r_fails(self):
        mirror, code, out = self.check(**{"<R>": b"<= B PROG>\n"})
        self.assertEqual(code, 1)
        self.assertIn("FAIL a reply to <R>", out)
        self.assertEqual(mirror.received[-2:], ["<0 PROG>", "<JG B 250>"])

    def test_no_prog_track_fails_before_any_change(self):
        mirror, code, out = self.check(**{"<=>": b"<= A MAIN>\n<= B NONE>\n"})
        self.assertEqual(code, 1)
        self.assertIn("FAIL <=> reports one PROG track", out)
        self.assertEqual(mirror.received, ["<=>"])


# What the station answers to ext-refuses-modifiers' commands when AUTO and
# INV on an EXT track are refused: <X>, and D still reads EXT. D starts as
# MAIN A and is put back.
EXT = {
    b"<=>": [b"<= A MAIN>\n<= D MAIN A>\n", b"<= A MAIN>\n<= D EXT>\n"],
    b"<= D EXT>": b"<= D EXT>\n",
    b"<= D AUTO>": b"<X>\n",
    b"<= D INV>": b"<X>\n",
    b"<= D MAIN_AUTO>": b"<= D MAIN A>\n",
}


class ExtRefusesModifiersTest(unittest.TestCase):
    def check(self, replies):
        mirror = FakeMirror(replies=replies)
        self.addCleanup(mirror.close)
        code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                         "ext-refuses-modifiers"])
        return mirror, code, out

    def test_auto_and_inv_refused_and_d_restored(self):
        mirror, code, out = self.check(EXT)
        self.assertEqual(code, 0, out)
        self.assertEqual(mirror.received, [
            "<=>", "<= D EXT>", "<= D AUTO>", "<= D INV>", "<=>",
            "<= D MAIN_AUTO>"])
        self.assertIn("ext-refuses-modifiers: passed", out)

    def test_auto_accepted_fails_and_d_is_restored(self):
        replies = dict(EXT)
        replies[b"<= D AUTO>"] = b"<= D EXT>\n"
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("FAIL <X> for <= D AUTO>", out)
        self.assertEqual(mirror.received[-1], "<= D MAIN_AUTO>")

    def test_inv_accepted_fails(self):
        replies = dict(EXT)
        replies[b"<= D INV>"] = b"<= D EXT>\n"
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("FAIL <X> for <= D INV>", out)

    def test_a_mode_other_than_ext_afterwards_fails(self):
        replies = dict(EXT)
        replies[b"<=>"] = [EXT[b"<=>"][0], b"<= A MAIN>\n<= D MAIN A>\n"]
        mirror, code, out = self.check(replies)
        self.assertEqual(code, 1)
        self.assertIn("FAIL <=> reports <= D EXT>", out)

    def test_mode_command_from_the_reported_name(self):
        for state, command in [
                ("MAIN", "MAIN"), ("MAIN I", "MAIN_INV"),
                ("MAIN A", "MAIN_AUTO"), ("MAIN A I", "MAIN_AUTO_INV"),
                ("BOOST A", "BOOST_AUTO"), ("BOOST I", "BOOST_INV"),
                ("PROG", "PROG"), ("NONE", "NONE"), ("EXT", "EXT"),
                ("DC 3", "DC 3"), ("DCX 3", "DCX 3")]:
            self.assertEqual(check.mode_command(state), command, state)


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
            code, out = run(["--port", str(mirror.port), "--window", WINDOW,
                             "probe", "--level", "7"])
        self.assertEqual(code, 0)
        self.assertEqual(seen, {"level": 7})
        self.assertIn("probe: passed", out)

    def test_send_is_in_the_table(self):
        self.assertIn("send", check.CHECKS)


if __name__ == "__main__":
    unittest.main()
