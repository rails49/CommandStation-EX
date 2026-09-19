# Firmware for the rails49 layout

This fork owns the station's code and builds it. The layout server flashes what
comes out. Nothing in this directory is upstream DCC-EX.

## What a release is

A tag of the form `v<upstream version>-rails49.<n>`, for example
`v5.6.4-rails49.1`. The upstream part says which DCC-EX release the code is
based on, the suffix counts builds of this fork on top of it.

Pushing such a tag runs `.github/workflows/rails49-release.yml`, which:

1. builds `config.rails49.h` against ESP32 core 2.0.17, appending the access
   point's password from the `WIFI_PASSWORD` repository secret;
2. writes the tag into `GITHUB_SHA.h`, so the station's `<s>` banner names the
   build that is on it;
3. merges the bootloader, partition table, boot_app0 and application into one
   image, flashable at offset `0x0`;
4. attaches that image to a GitHub release;
5. pushes `ghcr.io/rails49/commandstation-ex:<tag>`, which carries the image and
   `esptool`.

Running the workflow by hand from the Actions tab builds the same image and
keeps it as a workflow artifact, without releasing or pushing anything.

## Before the first release

Set the secret. It is the access point's password, and the master copy belongs
in 1Password, not in GitHub:

    op read "op://Private/rails49 station AP/password" |
      gh secret set WIFI_PASSWORD --repo rails49/CommandStation-EX

`config.rails49.h` holds everything else, because the rest of the file describes
hardware rather than being a secret.

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
    printf '#define WIFI_PASSWORD "%s"\n' "$(op read 'op://Private/rails49 station AP/password')" >> config.h
    arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries .

Then merge with the offsets the workflow uses. `config.h` is gitignored, so it
will not be committed, but delete it when you are done anyway.

## Why the image is merged

Four files at four offsets is four chances for an offset to go stale. The
offsets are not ours: they come from the core's own upload recipe in
`platform.txt`, and they change with the core version, which is why the workflow
pins it. One file at offset zero has nothing to get wrong.
