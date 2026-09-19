# Firmware for the rails49 layout

This fork owns the station's code and builds it. The layout server flashes what
comes out. Nothing in this directory is upstream DCC-EX.

## What a release is

A tag of the form `v<upstream version>-rails49.<n>`, for example
`v5.6.4-rails49.1`. The upstream part says which DCC-EX release the code is
based on, the suffix counts builds of this fork on top of it.

Pushing such a tag runs `.github/workflows/rails49-release.yml`, which:

1. builds `config.rails49.h` against ESP32 core 2.0.17;
2. writes the tag into `GITHUB_SHA.h`, so the station's `<s>` banner names the
   build that is on it;
3. merges the bootloader, partition table, boot_app0 and application into one
   image, flashable at offset `0x0`;
4. attaches that image to a GitHub release;
5. pushes `ghcr.io/rails49/commandstation-ex:<tag>`, which carries the image and
   `esptool`.

Running the workflow by hand from the Actions tab builds the same image and
keeps it as a workflow artifact, without releasing or pushing anything.

## The access point's password is published

The station is its own access point, and `config.rails49.h` names its password
in the clear. That is on purpose.

Left unset, the firmware generates `PASS_<mac>` and shows it on the station's
display. That works standing at the layout and nowhere else. Naming the password
in the file means it can be read from here instead, at the cost of it being a
password anyone can read.

It guards nothing that is otherwise guarded: everyone who can reach the layout
network can already drive every train, with no authentication anywhere. Radio
range is what is left. Anyone who wants their own password builds their own
firmware, which is what upstream DCC-EX expects of everybody anyway: it ships no
binaries, and every user compiles their own `config.h` on their own machine.

## Flashing

The layout server pulls the image and runs it against the serial device:

    docker run --rm --device /dev/dccex ghcr.io/rails49/commandstation-ex:v5.6.4-rails49.1

`DEVICE` and `BAUD` override the defaults. Two things to know:

- **Something else holds that device.** On the layout server the USB mirror has
  it open, exclusively. It has to be stopped before flashing and started again
  afterwards; esptool cannot share the port.
- **Flashing resets the station**, which drops the rails and every throttle
  connected through the mirror. It is not a live operation.

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
