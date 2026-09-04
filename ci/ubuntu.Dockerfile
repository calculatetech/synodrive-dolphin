FROM ubuntu:26.04 AS runtime

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        dolphin file libnautilus-extension4 python3 \
    && rm -rf /var/lib/apt/lists/*
