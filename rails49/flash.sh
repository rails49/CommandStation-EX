#!/bin/sh
#
# Write the firmware this image carries to the command station.
#
# The device is passed in, because which one it is belongs to the box the
# station is plugged into and not to this image. Whatever holds that device
# has to let go of it first: on the layout server the mirror holds it open,
# so stop that before running this and start it again afterwards.
set -eu

DEVICE="${DEVICE:-/dev/dccex}"
BAUD="${BAUD:-460800}"

if [ ! -e "$DEVICE" ]; then
  echo "No such device: $DEVICE" >&2
  exit 1
fi

echo "Writing $(cat /firmware/VERSION) to $DEVICE"

# One image, offset zero. esptool reads the flash back and compares checksums,
# so a write that does not land fails here rather than on the next power on.
exec esptool.py --chip esp32 --port "$DEVICE" --baud "$BAUD" \
  --before default_reset --after hard_reset \
  write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 /firmware/firmware.bin
