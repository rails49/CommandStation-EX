# Firmware for the rails49 layout

This fork owns the station's code and builds it. The layout server flashes what
comes out. Nothing in this directory is upstream DCC-EX.

## What a release is

A tag of the form `v<upstream version>-rails49.<n>`, for example
`v5.6.3-rails49.19`. The upstream part is the DCC-EX release the code is on,
the suffix counts builds of this fork (ADR-0007).

Pushing such a tag runs `.github/workflows/rails49-release.yml`, which:

1. runs `rails49/test-guard.sh`, the native test of the Guard's per-tick
   decision, and stops if it fails;
2. builds `config.rails49.h` against ESP32 core 2.0.17;
3. writes the tag into `GITHUB_SHA.h`, so the station's `<s>` banner names the
   build that is on it;
4. merges the bootloader, partition table, boot_app0 and application into one
   image, flashable at offset `0x0`;
5. attaches that image to a GitHub release.

Running the workflow by hand from the Actions tab builds the same image and
keeps it as a workflow artifact, without releasing anything.

## No wifi

`config.rails49.h` leaves wifi out, so the station runs no access point and
accepts no network connections. The layout server reaches it over USB only.

## Flashing

dccex, the USB mirror on the layout server, flashes the station from a
release's `firmware.bin`. Two things to know:

- **By hand, stop the mirror first.** It holds the device open, exclusively,
  and esptool cannot share the port. The release notes give the esptool
  command; start the mirror again afterwards.
- **Flashing resets the station**, which drops the rails and every throttle
  connected through the mirror. It is not a live operation.

## Talking to the station: check.py

`check.py` talks to the mirror port, by default `127.0.0.1:2560` on the layout
box (`--host`, `--port`). It sends a command and prints every line that comes
back for half a second (`--window`), broadcasts to all clients included:

    uv run rails49/check.py send '<=>'

`guard` sends `<D GUARD>` and prints what the Guard and the hardware hold: one
line per track (mode, invert flag, invert bit read back from the GPIO register,
whether the Guard watches it, accumulator, time since its last Reversal, trip
value), then the Guard's ticks in the last second and its longest tick, reset
by the read. It changes nothing on the layout:

    uv run rails49/check.py guard

`trip-follows-jg` reads every track's trip value from `<D GUARD>`, best run
first after a boot, then sets `<JG D 300>` (`--ma`) and expects D's trip value
to read it at once, and restores D's previous limit.

`join-keeps-jg` finds the PROG track from `<=>`, sets its limit to 600 mA
(`--ma`), joins with `<1 JOIN>`, and expects the trip value to read it while
joined and again after the ack of an `<R>`. It expects no `<p1 JOIN>` in the
power lines Broadcast as the ack ends Join, and `<p1 JOIN>` in those as it
restores Join. If those read `<p0 X>` for the PROG track, it expects a
`<p1 X>` to follow; if they read `<p1 X>`, it says no ALERT was seen. It ends
with `<0 PROG>` and restores the previous limit.

Every other subcommand is one check of #9 or #10; `--help` lists them. A check
changes track modes and power, so run one only with the user's OK. A new check
is one entry in the `CHECKS` table. `python3 -B -m unittest rails49/test_check.py`
tests the client against a fake mirror.

## Building by hand

    cp config.rails49.h config.h
    arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries .

Then merge with the offsets the workflow uses. `config.h` is gitignored, so it
will not be committed.

## Why the image is merged

Four files at four offsets is four chances for an offset to go stale. The
offsets are not ours: they come from the core's own upload recipe in
`platform.txt`, and they change with the core version, which is why the workflow
pins it. One file at offset zero has nothing to get wrong.
