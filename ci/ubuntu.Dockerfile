FROM ubuntu:26.04

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake dpkg-dev file g++ libkf6kio-dev ninja-build python3 qt6-base-dev \
    && rm -rf /var/lib/apt/lists/*
