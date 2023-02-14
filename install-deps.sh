#!/usr/bin/env bash

ALL_DEPS="glew glfw3 entt openal-soft glm"
TRIPLET=x64-linux

echo ${ALL_DEPS}

vcpkg/vcpkg --version
vcpkg/vcpkg install $ALL_DEPS --triplet=$TRIPLET