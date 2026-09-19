# Carries one build of the layout's firmware and the tool that writes it.
# Built by .github/workflows/rails49-release.yml, which puts firmware.bin at the
# root of the build context first. Nothing is compiled here.
FROM python:3.12-slim

RUN pip install --no-cache-dir esptool==4.5.1

ARG VERSION=unknown
RUN mkdir -p /firmware && echo "$VERSION" > /firmware/VERSION

COPY firmware.bin /firmware/firmware.bin
COPY rails49/flash.sh /usr/local/bin/flash

LABEL org.opencontainers.image.source=https://github.com/rails49/CommandStation-EX
LABEL org.opencontainers.image.description="DCC-EX firmware for the rails49 layout, with esptool to write it"

ENTRYPOINT ["/usr/local/bin/flash"]
