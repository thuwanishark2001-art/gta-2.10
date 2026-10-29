# syntax=docker/dockerfile:1
# Kingmakers RP game server (open.mp, 32-bit Linux).
# Run scripts/fetch-deps.sh first: it downloads the pinned open.mp release,
# compiler and plugins into .deps/, which this file copies in.

FROM mirror.gcr.io/library/debian:bookworm-slim AS base
# APT_MIRROR and the optional extra_ca secret are only for networks behind a
# TLS-intercepting proxy; the defaults work everywhere else.
ARG APT_MIRROR=http://deb.debian.org
RUN --mount=type=secret,id=extra_ca,required=false \
    sed -i "s#http://deb.debian.org#${APT_MIRROR}#g" /etc/apt/sources.list.d/debian.sources && \
    if [ -s /run/secrets/extra_ca ]; then \
        cp /run/secrets/extra_ca /tmp/extra_ca.crt && chmod 644 /tmp/extra_ca.crt && \
        echo 'Acquire::https::CaInfo "/tmp/extra_ca.crt";' > /etc/apt/apt.conf.d/99ca; \
    fi && \
    dpkg --add-architecture i386 && apt-get update && \
    apt-get install -y --no-install-recommends libc6:i386 libstdc++6:i386 && \
    rm -rf /var/lib/apt/lists/* /etc/apt/apt.conf.d/99ca /tmp/extra_ca.crt

FROM base AS build
COPY .deps/pawnc /opt/pawnc
COPY .deps/include /src/include
COPY gamemodes /src/gamemodes
COPY tests /src/tests
WORKDIR /src
ENV LD_LIBRARY_PATH=/opt/pawnc/lib
RUN mkdir -p out && \
    /opt/pawnc/bin/pawncc gamemodes/kingmakers.pwn -iinclude -igamemodes -oout/kingmakers.amx -d1 -O1 '-;+' '-(+' -Z+ && \
    /opt/pawnc/bin/pawncc tests/km_tests.pwn -iinclude -igamemodes -oout/km_tests.amx -d1 -O1 '-;+' '-(+' -Z+

FROM base AS server
RUN useradd --system --home /srv/omp omp
WORKDIR /srv/omp
COPY .deps/omp/ ./
COPY --from=build /src/out/ gamemodes/
COPY scripts/entrypoint.sh /usr/local/bin/km-entrypoint
RUN mkdir -p scriptfiles filterscripts models logs && chown -R omp:omp /srv/omp
# The entrypoint starts as root only to fix volume ownership, then runs as omp.
EXPOSE 7777/udp
ENTRYPOINT ["km-entrypoint"]
