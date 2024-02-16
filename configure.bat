cmake -S . ^
-B build ^
-DUSE_VCPKG=TRUE ^
-DVCPKG_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake ^
-DYIN_YANG_USE_SIMD=TRUE