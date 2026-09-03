FROM ubuntu:26.04 AS common

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

FROM common AS build

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake dpkg-dev file g++ libkf6kio-dev libnautilus-extension4 ninja-build python3 qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*

FROM common AS runtime

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        dolphin file libnautilus-extension4 python3 \
    && rm -rf /var/lib/apt/lists/*
