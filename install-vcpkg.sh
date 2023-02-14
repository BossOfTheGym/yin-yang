#!/usr/bin/env bash

GIT_REPO="https://github.com/microsoft/vcpkg"
GIT_TAG="2022.08.15"

git clone --depth 1 --branch $GIT_TAG $GIT_REPO

vcpkg/bootstrap-vcpkg.sh