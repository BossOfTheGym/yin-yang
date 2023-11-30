#pragma once

// indices from range [-2^31 + 1, to 2^31 - 2]
inline constexpr int sparse_cell_base = 1 << 30;

using sparse_cell_t = glm::ivec3;

struct sparse_cell_hasher_t {
	static std::uint32_t h32(std::uint32_t h) {
		h ^= (h >> 16);
		h *= 0x85ebca6b;
		h ^= (h >> 13);
		h *= 0xc2b2ae35;
		h ^= (h >> 16);
		return h;
	}

	std::uint32_t operator() (const sparse_cell_t& cell) const {
		std::uint32_t x = h32(cell.x);
		std::uint32_t y = h32(cell.y);
		std::uint32_t z = h32(cell.z);
		return (x * 0xc2b2ae35 + y) * 0x85ebca6b + z;
	}
};

struct sparse_cell_equals_t {
	bool operator() (const sparse_cell_t& cell1, const sparse_cell_t& cell2) const {
		return cell1 == cell2;
	}
};

// cell_scale = 1.0f / cell_size
inline sparse_cell_t get_sparse_cell(const glm::vec3& point, float cell_scale, float cell_min, float cell_max) {
	return glm::clamp(glm::floor(point * cell_scale), glm::vec3{cell_min}, glm::vec3{cell_max});
}