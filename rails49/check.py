# /// script
# requires-python = ">=3.9"
# dependencies = []
# ///
"""Checks of the station's command interface, run against the mirror port.

Each check is one subcommand. It sends commands, collects every line the
station sends for a short window after each, so broadcasts are seen too, and
asserts on what a host sees. Run it on the layout box, where the mirror port
is local:

    uv run rails49/check.py polarity-survives
    uv run rails49/check.py trip-follows-jg
    uv run rails49/check.py guard
    uv run rails49/check.py --host 127.0.0.1 --port 2560 send "<D GUARD>"

The checks change track modes and power on the layout. Running one is the
OK for that; nothing happens until a subcommand is given.
"""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from dataclasses import dataclass, field
from typing import Callable

TRACK = "D"  # the AUTO track under test on rails49
OTHER = "A"  # a plain MAIN track


class CheckFailed(Exception):
    pass


@dataclass
class GuardLine:
    """One track's line of <D GUARD>."""

    track: str
    mode: str
    inv: int
    gpio: int
    active: int
    acc_us: int
    last: str
    trip: int | None     # raw counts, None from a build before #10
    trip_ma: int | None


GUARD_RE = re.compile(
    r"GUARD (?P<track>[A-H]) (?P<mode>.+?) inv=(?P<inv>\d) gpio=(?P<gpio>\d) "
    r"active=(?P<active>\d) acc=(?P<acc>-?\d+)us last=(?P<last>\S+)"
    r"(?: trip=(?P<trip>\d+)/(?P<tripma>\d+)mA)?"
)


def parse_guard(lines: list[str]) -> dict[str, GuardLine]:
    result = {}
    for line in lines:
        m = GUARD_RE.search(line)
        if m:
            result[m["track"]] = GuardLine(
                m["track"], m["mode"], int(m["inv"]), int(m["gpio"]),
                int(m["active"]), int(m["acc"]), m["last"],
                int(m["trip"]) if m["trip"] else None,
                int(m["tripma"]) if m["tripma"] else None)
    return result


@dataclass
class GuardCounters:
    """The last line of <D GUARD>: the Guard's ticks in the last second and
    its longest tick, both reset by the read."""

    ticks_per_s: int
    max_us: int


GUARD_COUNTERS_RE = re.compile(r"GUARD ticks=(?P<ticks>\d+)/s max=(?P<max>\d+)us")


def parse_guard_counters(lines: list[str]) -> GuardCounters | None:
    for line in lines:
        m = GUARD_COUNTERS_RE.search(line)
        if m:
            return GuardCounters(int(m["ticks"]), int(m["max"]))
    return None


def gauges(lines: list[str]) -> list[int]:
    """The limits in mA from the <jG ...> reply, one per track from A."""
    for line in lines:
        if line.startswith("<jG ") and line.endswith(">"):
            return [int(v) for v in line[4:-1].split()]
    return []


def track_states(lines: list[str], track: str) -> list[str]:
    """Mode names from every <= x ...> line for the track, in order."""
    prefix = f"<= {track} "
    return [line[len(prefix):-1] for line in lines
            if line.startswith(prefix) and line.endswith(">")]


def power_lines(lines: list[str], track: str) -> list[str]:
    """<p0 x> and <p1 x> lines for the track, in order."""
    return [line for line in lines if line in (f"<p0 {track}>", f"<p1 {track}>")]


class Station:
    def __init__(self, host: str, port: int, window: float):
        self.window = window
        self.sock = socket.create_connection((host, port), timeout=5)
        self.buf = b""
        self.collect(self.window)  # drop what was in flight

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.sock.close()

    def collect(self, seconds: float) -> list[str]:
        lines = []
        end = time.monotonic() + seconds
        while (left := end - time.monotonic()) > 0:
            self.sock.settimeout(left)
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                break
            if not data:
                raise CheckFailed("the mirror closed the connection")
            self.buf += data
            *done, self.buf = self.buf.split(b"\n")
            lines += [d.decode(errors="replace").strip() for d in done]
        return [line for line in lines if line]

    def send(self, command: str, seconds: float | None = None) -> list[str]:
        self.sock.sendall(command.encode() + b"\n")
        lines = self.collect(self.window if seconds is None else seconds)
        print(f">>> {command}")
        for line in lines:
            print(f"    {line}")
        return lines


def expect(ok: bool, what: str):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        raise CheckFailed(what)


def polarity_survives(st: Station, args):
    """A mode command on one track leaves the Polarity of the others alone."""
    st.send(f"<= {TRACK} MAIN_AUTO>")
    st.send(f"<= {TRACK} INV>")
    st.send(f"<= {OTHER} MAIN>")
    guard = parse_guard(st.send("<D GUARD>"))
    expect(TRACK in guard and OTHER in guard, "<D GUARD> reports both tracks")
    d, a = guard[TRACK], guard[OTHER]
    expect(d.inv == 1, f"{TRACK} invert flag set")
    expect(d.gpio == 1, f"{TRACK} gpio invert bit set")
    expect(a.inv == 0, f"{OTHER} invert flag clear")
    expect(a.gpio == 0, f"{OTHER} gpio invert bit clear")


def reports(st: Station, args):
    """<=> shows the Polarity the hardware has."""
    st.send(f"<= {TRACK} MAIN_AUTO_INV>")
    states = track_states(st.send("<=>"), TRACK)
    expect(states == ["MAIN A I"], f"<=> reports <= {TRACK} MAIN A I>")
    st.send(f"<= {TRACK} MAIN_AUTO>")
    states = track_states(st.send("<=>"), TRACK)
    expect(states == ["MAIN A"], f"<=> reports <= {TRACK} MAIN A>")


def no_cut(st: Station, args):
    """A mode command cuts power only when the base mode changes."""
    st.send(f"<= {TRACK} MAIN_AUTO>")
    st.send(f"<1 {TRACK}>")
    lines = st.send(f"<= {TRACK} MAIN_AUTO_INV>")
    expect(f"<p0 {TRACK}>" not in lines, f"no <p0 {TRACK}> after MAIN_AUTO_INV")
    expect(track_states(lines, TRACK) == ["MAIN A I"], f"broadcast <= {TRACK} MAIN A I>")
    lines = st.send("<=>") + st.send("<p>")
    expect(track_states(lines, TRACK) == ["MAIN A I"], f"<=> still reports {TRACK} MAIN A I")
    expect(f"<p1 {TRACK}>" in lines, f"<p> reports {TRACK} on")
    lines = st.send(f"<= {TRACK} NONE>")
    expect(f"<p0 {TRACK}>" in lines, f"<p0 {TRACK}> after NONE")
    print(f"restoring {TRACK} to MAIN_AUTO and on")
    st.send(f"<= {TRACK} MAIN_AUTO>")
    st.send(f"<1 {TRACK}>")


def overload_broadcast(st: Station, args):
    """An Overload is broadcast as a power state, entering and leaving."""
    lines = st.send(f"<1 {TRACK}>")
    print(f"Short track {TRACK} now, a wire across its rails for a second,")
    print(f"then remove it. Collecting for {args.wait:.0f} s.")
    lines += st.collect(args.wait)
    for line in lines:
        print(f"    {line}")
    # Each retry ends a period that began with an entry to OVERLOAD, so each
    # period before a POWER RESTORE has its <p0 x> (ADR-0005).
    periods = [[]]
    for line in lines:
        if f"TRACK {TRACK} POWER RESTORE" in line:
            periods.append([])
        else:
            periods[-1].append(line)
    seen = power_lines(lines, TRACK)
    expect(f"<p0 {TRACK}>" in seen, f"<p0 {TRACK}> on the Overload")
    for n, period in enumerate(periods[:-1]):
        expect(f"<p0 {TRACK}>" in power_lines(period, TRACK),
               f"<p0 {TRACK}> before retry {n + 1}")
    expect(seen[-1] == f"<p1 {TRACK}>",
           f"<p1 {TRACK}> last, when the track is back on")


def reversal_broadcast(st: Station, args):
    """A Reversal broadcasts the track state line."""
    st.send(f"<= {TRACK} MAIN_AUTO>")
    st.send(f"<1 {TRACK}>")
    print(f"Run a train across a gap of track {TRACK} with the wrong Polarity.")
    print(f"Collecting for {args.wait:.0f} s.")
    lines = st.collect(args.wait)
    for line in lines:
        print(f"    {line}")
    at = next((i for i, line in enumerate(lines)
               if f"TRACK {TRACK} INVERT" in line), None)
    expect(at is not None, f"a Reversal of {TRACK} was seen")
    after = track_states(lines[at + 1:], TRACK)
    expect(any(s in ("MAIN A", "MAIN A I") for s in after),
           f"<= {TRACK} MAIN A[ I]> broadcast after the Reversal")


def trip_follows_jg(st: Station, args):
    """A limit set with <JG> is the Guard's trip value at once."""
    before = gauges(st.send("<JG>"))
    index = ord(TRACK) - ord("A")
    expect(len(before) > index, f"<JG> reports a limit for {TRACK}")
    try:
        st.send(f"<JG {TRACK} {args.ma}>")
        guard = parse_guard(st.send("<D GUARD>"))
        expect(TRACK in guard, f"<D GUARD> reports {TRACK}")
        d = guard[TRACK]
        expect(d.trip is not None, f"<D GUARD> reports {TRACK}'s trip value")
        # the mA come back through the ADC's raw counts, so may be a little low
        expect(args.ma - 20 <= d.trip_ma <= args.ma,
               f"{TRACK} trips at {d.trip_ma}mA ({d.trip} raw), set {args.ma}mA")
    finally:
        print(f"restoring {TRACK}'s limit to {before[index]}mA")
        st.send(f"<JG {TRACK} {before[index]}>")


def guard(st: Station, args):
    """Send <D GUARD> and print what the Guard and the hardware hold."""
    lines = st.send("<D GUARD>")
    expect(bool(parse_guard(lines)), "<D GUARD> reports the tracks")
    expect(parse_guard_counters(lines) is not None,
           "<D GUARD> reports the Guard's ticks and longest tick")


def send(st: Station, args):
    """Send one command and print every line for the window."""
    st.send(args.command)


@dataclass
class Check:
    """A subcommand: the function it runs and its own arguments, each as
    (name or flags, add_argument keywords)."""

    fn: Callable[[Station, argparse.Namespace], None]
    args: list[tuple[str, dict]] = field(default_factory=list)
    asserts: bool = True  # False: only prints, so no "passed"


WAIT = ("--wait", dict(type=float, default=30.0,
                       help="seconds to collect while the user acts"))

CHECKS = {
    "send": Check(send, [("command", dict(help="for example '<=>'"))],
                  asserts=False),
    "guard": Check(guard),
    "polarity-survives": Check(polarity_survives),
    "reports": Check(reports),
    "no-cut": Check(no_cut),
    "overload-broadcast": Check(overload_broadcast, [WAIT]),
    "reversal-broadcast": Check(reversal_broadcast, [WAIT]),
    "trip-follows-jg": Check(trip_follows_jg, [
        ("--ma", dict(type=int, default=300, help="limit to set, mA"))]),
}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1", help="mirror host")
    ap.add_argument("--port", type=int, default=2560, help="mirror port")
    ap.add_argument("--window", type=float, default=0.5,
                    help="seconds to collect lines after each command")
    sub = ap.add_subparsers(dest="check", required=True)
    for name, check in CHECKS.items():
        p = sub.add_parser(name, help=check.fn.__doc__)
        p.set_defaults(check_entry=check)
        for flag, kw in check.args:
            p.add_argument(flag, **kw)
    args = ap.parse_args(argv)

    check = args.check_entry
    try:
        with Station(args.host, args.port, args.window) as st:
            check.fn(st, args)
    except CheckFailed as e:
        print(f"{args.check}: FAILED: {e}")
        return 1
    except OSError as e:
        print(f"{args.check}: cannot talk to {args.host}:{args.port}: {e}")
        return 2
    if check.asserts:
        print(f"{args.check}: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
