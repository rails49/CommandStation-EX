# A release tag names the upstream base, and the release is the only output

Release tags read `v5.6.4-rails49.<n>`, and the release README said the
version part is the DCC-EX release the code is based on. The code is
`v5.6.3-Prod` plus the fork's commits. The fork had set `VERSION` to 5.6.4
itself, for `<JG track mA>`, while upstream's 5.6.4 is a different change the
fork does not carry (#63).

The release workflow also pushed `ghcr.io/rails49/commandstation-ex:<tag>`, an
image carrying the firmware and esptool. dccex, the USB mirror on the layout
server, flashes the station from the release asset `firmware.bin`, and nothing
in rails49 pulls the image.

Decided:

- The version part of a tag is the upstream release the code is on, now
  `5.6.3`. `VERSION` equals it. The fork's own changes do not take an upstream
  version number.
- The build counter continues across a change of version part. The first
  release under this rule is `v5.6.3-rails49.19`.
- The release is the only output. The workflow builds no image and pushes
  nothing to a registry. dccex flashes the station from the release's
  `firmware.bin`.

## Considered options

- Rebase onto upstream 5.6.4 or later. 5.6.4 adds a fix for decoding
  F13–F20; 5.6.5 and 5.6.6 fix only the Mega. Worth doing, as its own change,
  after which the version part follows the new base.
- Restart the counter at `.1`. `v5.6.3-rails49.1` would then be newer than
  `v5.6.4-rails49.18`.
- Keep pushing the image. It is a second copy of every release that nothing
  reads.

## Consequences

- The `<s>` banner of a release reads `V-5.6.3` and `G-<tag>`. dccex matches a
  release to the station on `G-`, which is unchanged.
- Existing `v5.6.4-rails49.*` tags, releases and pushed images stay as they
  are.
- The workflow no longer needs `packages: write`.
