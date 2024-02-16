#pragma once

#include <simd.hpp>

#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>

// indices from range [-2^31 + 1, to 2^31 - 2]
inline constexpr int sparse_cell_base = 1 << 30;
inline constexpr int sparse_cell_max = INT_MAX - 1;
inline constexpr int sparse_cell_min = INT_MIN + 1;

using sparse_cell_t = glm::ivec3;

struct sparse_cell_hasher_t {
#ifdef YIN_YANG_USE_SIMD
	static uint32_t hcell_simd(const sparse_cell_t& cell) {
		const __m128i load_mask = _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0);
		const __m128i mul1 = _mm_set_epi32(0x85ebca6b, 0x85ebca6b, 0x85ebca6b, 0x85ebca6b);
		const __m128i mul2 = _mm_set_epi32(0xc2b2ae35, 0xc2b2ae35, 0xc2b2ae35, 0xc2b2ae35);

		__m128i data = _mm_maskload_epi32(glm::value_ptr(cell), load_mask);
		data = _mm_xor_si128(data, _mm_slli_epi32(data, 16));
		data = _mm_mullo_epi32(data, mul1);
		data = _mm_xor_si128(data, _mm_slli_epi32(data, 13));
		data = _mm_mullo_epi32(data, mul2);
		data = _mm_xor_si128(data, _mm_slli_epi32(data, 16));

		data = _mm_mullo_epi32(data, _mm_set_epi32(0xc2b2ae35, 1, 1, 1));
		data = _mm_mullo_epi32(data, _mm_set_epi32(0x85ebca6b, 0x85ebca6b, 1, 1));

		uint32_t extracted[4]{};
		_mm_storeu_si128((__m128i*)extracted, data);
		return extracted[0] + extracted[1] + extracted[2];
	}

	static uint32_t hcell_crc32(const sparse_cell_t& cell) {
		uint32_t crc{};
		crc = _mm_crc32_u32(crc, cell.x);
		crc = _mm_crc32_u32(crc, cell.y);
		crc = _mm_crc32_u32(crc, cell.z);
		return crc;
	}

	static uint32_t hcell(const sparse_cell_t& cell) {
		return hcell_crc32(cell); // seems to be the best and the fastest
	}
#else
	static uint32_t h32(std::uint32_t h) {
		h ^= (h >> 16);
		h *= 0x85ebca6b;
		h ^= (h >> 13);
		h *= 0xc2b2ae35;
		h ^= (h >> 16);
		return h;
	}

	static uint32_t hcell(const sparse_cell_t& cell) {
		uint32_t x = h32(cell.x);
		uint32_t y = h32(cell.y);
		uint32_t z = h32(cell.z);
		return (x * 0xc2b2ae35 + y) * 0x85ebca6b + z;
	}
#endif

	uint32_t operator() (const sparse_cell_t& cell) const {
		return hcell(cell);
	}
};

struct sparse_cell_equals_t {
	bool operator() (const sparse_cell_t& cell1, const sparse_cell_t& cell2) const {
		return cell1 == cell2;
	}
};


#ifdef YIN_YANG_USE_SIMD
inline sparse_cell_t __get_sparse_cell(const glm::vec3& point, float cell_scale) {
	/*const __m128i load_store_mask = _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0);
	const __m128 pmin = _mm_set_ps((float)sparse_cell_min, (float)sparse_cell_min, (float)sparse_cell_min, (float)sparse_cell_min);
	const __m128 pmax = _mm_set_ps((float)sparse_cell_max, (float)sparse_cell_max, (float)sparse_cell_max, (float)sparse_cell_max);

	const __m128 p = _mm_maskload_ps(glm::value_ptr(point), load_store_mask);
	const __m128 s = _mm_set_ps(cell_scale, cell_scale, cell_scale, cell_scale);
	const __m128i c = _mm_cvtps_epi32(_mm_max_ps(pmin, _mm_min_ps(pmax, _mm_mul_ps(p, s))));

	sparse_cell_t cell{};
	_mm_maskstore_epi32(glm::value_ptr(cell), load_store_mask, c);
	return cell;*/

	// simple version seems to work better
	return glm::clamp(glm::floor(point * cell_scale), glm::vec3(sparse_cell_min), glm::vec3(sparse_cell_max));
}
#else
inline sparse_cell_t __get_sparse_cell(const glm::vec3& point, float cell_scale) {
	return glm::clamp(glm::floor(point * cell_scale), glm::vec3(sparse_cell_min), glm::vec3(sparse_cell_max));
}
#endif

// cell_scale = 1.0f / cell_size
inline sparse_cell_t get_sparse_cell(const glm::vec3& point, float cell_scale) {
	return __get_sparse_cell(point, cell_scale);
}