#pragma once

#include <immintrin.h>

#ifdef YIN_YANG_USE_SIMD
inline constexpr const char* yin_yang_use_simd = "yes";
#else
inline constexpr const char* yin_yang_use_simd = "no";
#endif