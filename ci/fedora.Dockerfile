FROM fedora:44 AS common

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

FROM common AS build

RUN dnf install -y --setopt=install_weak_deps=False \
        cmake gcc-c++ kf6-kio-devel ninja-build python3 qt6-qtbase-devel rpm-build \
    && dnf clean all

FROM common AS runtime

RUN dnf install -y --setopt=install_weak_deps=False \
        --disablerepo=fedora-cisco-openh264 \
        dolphin file nautilus-extensions \
    && dnf clean all
