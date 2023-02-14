@echo off

set ALL_DEPS=glew glfw3 entt openal-soft glm
set TRIPLET=x64-windows

vcpkg\vcpkg --version
vcpkg\vcpkg install %ALL_DEPS% --triplet=%TRIPLET%

@echo on