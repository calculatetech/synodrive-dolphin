FROM fedora:44

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN dnf install -y --setopt=install_weak_deps=False \
        cmake gcc-c++ kf6-kio-devel ninja-build python3 qt6-qtbase-devel rpm-build \
    && dnf clean all
