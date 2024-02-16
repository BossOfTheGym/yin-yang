#pragma once

#include <bit>
#include <array>
#include <random>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <type_traits>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/mat3x3.hpp>
#include <glm/common.hpp>

#include <glm/gtc/type_ptr.hpp>

inline int test_var = 666;

template<class ... tag_t>
struct type_id_t {
private:
	inline static std::size_t counter = 0;

public:
	template<class ... type_t>
	static std::size_t get() {
		static const std::size_t id = counter++;
		return id;
	}

	static std::size_t count() {
		return counter;
	}
};


template<class if_t>
class if_placeholder_t {
	template<class object_t>
	static std::size_t object_id() { return type_id_t<if_t>::template get<object_t>(); }

	template<class object_t>
	if_placeholder_t(std::shared_ptr<if_t> _if_ptr, object_t* _true_ptr)
		: if_ptr{std::move(_if_ptr)}, true_ptr{_true_ptr}, id{object_id<object_t>()} {}

public:
	template<class object_t, class ... args_t>
	static if_placeholder_t create(args_t&& ... args) {
		auto entry_ptr = std::make_shared<object_t>(std::forward<args_t>(args)...);
		return if_placeholder_t{entry_ptr, entry_ptr.get()};
	}

	template<class object_t>
	static if_placeholder_t create(object_t* object) {
		return if_placeholder_t{std::shared_ptr<if_t>(object), object};
	}

	template<class object_t>
	static if_placeholder_t create(std::shared_ptr<object_t> object_ptr) {
		return if_placeholder_t{object_ptr, object_ptr.get()};
	}

	if_placeholder_t() = default;

	if_placeholder_t(if_placeholder_t&& another) noexcept {
		*this = std::move(another);
	}

	if_placeholder_t& operator = (if_placeholder_t&& another) noexcept {
		std::swap(if_ptr, another.if_ptr);
		std::swap(true_ptr, another.true_ptr);
		std::swap(id, another.id);
		return *this;
	}

	template<class object_t>
	auto* get() const {
		assert(id == object_id<object_t>());
		return (object_t*)true_ptr;
	}

	if_t* get_if() const {
		return if_ptr.get();
	}

private:
	std::shared_ptr<if_t> if_ptr{}; // to simplify things up
	void* true_ptr{}; // true pointer to entry to avoid dynamic_cast (can be directly cast to object using static_cast)
	std::size_t id{};
};


using seed_t = std::uint64_t;

inline seed_t shuffle(seed_t value) {
	return std::rotl(value, 17) * 0x123456789ABCDEF1 + std::rotr(value, 17);
}

class basic_int_gen_t {
public:
	basic_int_gen_t(seed_t seed) : base_gen(seed) {}

	int gen() {
		return base_gen();
	}

private:
	std::minstd_rand base_gen;
};

class int_gen_t {
public:
	int_gen_t(seed_t seed, int a, int b) : base_gen(seed), distr(a, b) {}

	int gen() {
		return distr(base_gen);
	}

private:
	std::minstd_rand base_gen;
	std::uniform_int_distribution<> distr;
};

class float_gen_t {
public:
	float_gen_t(seed_t seed, float a, float b) : base_gen(shuffle(seed)), distr(a, b) {}

	float gen() {
		return distr(base_gen);
	}

private:
	std::minstd_rand base_gen;
	std::uniform_real_distribution<> distr;
};

class rgb_color_gen_t {
public:
	rgb_color_gen_t(seed_t seed)
		: r_gen(shuffle(seed), 0.0f, 1.0f)
		, g_gen(shuffle(seed + 1), 0.0f, 1.0f)
		, b_gen(shuffle(seed + 2), 0.0f, 1.0f
		) {}

	glm::vec3 gen() {
		float r = r_gen.gen(), g = g_gen.gen(), b = b_gen.gen();
		return glm::vec3(r, g, b);
	}

private:
	float_gen_t r_gen, g_gen, b_gen;
};

class hsl_color_gen_t {
public:
	hsl_color_gen_t(seed_t seed) : h_gen(shuffle(seed), 0.0f, 360.0f) {}

	glm::vec3 gen() {
		float h = h_gen.gen();
		return glm::vec3(h, 1.0f, 1.0f);
	}

private:
	float_gen_t h_gen;
};

inline glm::vec3 hsl_to_rgb(const glm::vec3& hsl) {
	float h = hsl.x, s = hsl.y, l = hsl.z;

	float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
	float ht = h / 60.0f;
	float ht_q, ht_r;
	ht_r = std::modf(ht, &ht_q);
	float x = c * (1.0f - std::abs(ht_r - 1.0f));

	glm::vec3 rgb{};
	switch (int(ht)) {
		case 0: rgb = glm::vec3(c, x, 0); break;
		case 1: rgb = glm::vec3(x, c, 0); break;
		case 2: rgb = glm::vec3(0, c, x); break;
		case 3: rgb = glm::vec3(0, x, c); break;
		case 4: rgb = glm::vec3(x, 0, c); break;
		case 5: rgb = glm::vec3(c, 0, x); break;
	}

	return rgb + glm::vec3(l - c * 0.5f);
}

class hsl_to_rgb_color_gen_t : public hsl_color_gen_t {
public:
	using base_t = hsl_color_gen_t;

	hsl_to_rgb_color_gen_t(seed_t seed) : base_t(seed) {}

	glm::vec3 gen() {
		return hsl_to_rgb(base_t::gen());
	}
};

using hsv_color_gen_t = hsl_color_gen_t;

inline glm::vec3 hsv_to_rgb(const glm::vec3& hsv) {
	float h = hsv.x, s = hsv.y, v = hsv.z;

	float c = v * s;
	float ht = h / 60.0f;
	float ht_q, ht_r;
	ht_r = std::modf(ht, &ht_q);
	float x = c * (1.0f - std::abs(ht_r - 1.0f));

	glm::vec3 rgb{};
	switch (int(ht)) {
		case 0: rgb = glm::vec3(c, x, 0); break;
		case 1: rgb = glm::vec3(x, c, 0); break;
		case 2: rgb = glm::vec3(0, c, x); break;
		case 3: rgb = glm::vec3(0, x, c); break;
		case 4: rgb = glm::vec3(x, 0, c); break;
		case 5: rgb = glm::vec3(c, 0, x); break;
	}

	return rgb + glm::vec3(v - c);
}

class hsv_to_rgb_color_gen_t : public hsv_color_gen_t {
public:
	using base_t = hsv_color_gen_t;

	hsv_to_rgb_color_gen_t(seed_t seed) : base_t(seed) {}

	glm::vec3 gen() {
		return hsv_to_rgb(base_t::gen());
	}
};


template<class type_t>
void shuffle_vec(type_t* data, int size, seed_t seed = 0) {
	if (size <= 0) {
		return;
	}

	basic_int_gen_t gen(seed);
	for (int i = size - 1; i > 0; i--) {
		std::swap(data[i], data[gen.gen() % i]);
	}
}

template<class type_t>
void shuffle_vec(std::vector<type_t>& vec, seed_t seed = 0) {
	shuffle_vec(vec.data(), vec.size(), seed);
}


inline int nextlog2(int n) {
	int power = 0;
	int next = 1;
	while (next < n) {
		next <<= 1;
		power++;
	}
	return power;
}

inline int nextpow2(int n) {
	int next = 1;
	while (next < n) {
		next <<= 1;
	}
	return next;
}

inline int log2size(int n) {
	return 1 << n;
}


template<class>
struct callback_t;

template<class ret_t, class ... args_t>
struct callback_t<ret_t(args_t...)>{
	template<class ctx_t>
	using ctx_func_t = ret_t(ctx_t*, args_t...);

	callback_t() = default;

	template<class ctx_t>
	callback_t(ctx_t& _ctx, ctx_func_t<ctx_t>* _func)
		: ctx{&_ctx}
		, func{(ctx_func_t<void>*)_func}
	{}

	template<class ctx_t>
	callback_t(ctx_t* _ctx, ctx_func_t<ctx_t>* _func)
		: ctx{_ctx}
		, func{(ctx_func_t<void>*)_func}
	{}

	ret_t operator() (args_t ... args) const {
		return func(ctx, std::forward<args_t>(args)...);
	}

	void* ctx{};
	ctx_func_t<void>* func{};
};


template<class iter_t>
struct iter_helper_t {
	iter_t begin() {
		return iter_begin;
	}

	iter_t end() {
		return iter_end;
	}

	iter_t iter_begin, iter_end;
};

struct int_iter_t {
	int operator* () const {
		return value;
	}

	bool operator== (int_iter_t iter) const {
		return value == iter.value;
	}

	int_iter_t& operator++ () {
		return value++, * this;
	}

	int_iter_t operator++ (int) {
		return int_iter_t{value++};
	}

	int_iter_t& operator-- () {
		return value--, * this;
	}

	int_iter_t operator-- (int) {
		return int_iter_t{value--};
	}

	int value{};
};

struct int_iter_range_t {
	int_iter_t begin() const { return {range_start}; }
	int_iter_t end() const { return {range_end}; }

	int range_start{};
	int range_end{};
};


struct job_range_t {
	int start{};
	int stop{};
};

inline job_range_t compute_job_range(int job_size, int job_count, int job_id) {
	int job_part = (job_size + job_count - 1) / job_count;
	int job_start = std::min(job_id * job_part, job_size);
	int job_stop = std::min(job_start + job_part, job_size);
	return {job_start, job_stop};
}

template<class type_t>
struct data_range_t {
	type_t* start{};
	type_t* stop{};
};

template<class type_t>
data_range_t<type_t> compute_data_range(type_t* data, int job_size, int job_count, int job_id) {
	auto [start, stop] = compute_job_range(job_size, job_count, job_id);
	return {data + start, data + stop};
}


inline void assert_check(bool value, std::string_view error) {
	if (!value) {
		std::cerr << error << "\n";
		std::abort();
	}
}

inline void assert_false(bool value, std::string_view error) {
	assert_check(!value, error);
}

inline void assert_true(bool value, std::string_view error) {
	assert_check(value, error);
}

template<class type_t, int capacity>
struct static_vector_t {
	bool can_push() const {
		return count < capacity;
	}

	void push_back(type_t value) {
		assert(can_push());
		_data[count++] = value;
	}

	void pop_back() {
		count--;
	}

	void reset() {
		count = 0;
	}

	type_t& operator[] (int i) {
		return _data[i];
	}

	const type_t& operator[] (int i) const {
		return _data[i];
	}

	auto begin() {
		return _data.begin();
	}

	auto end() {
		return _data.end();
	}

	auto begin() const {
		return _data.begin();
	}

	auto end() const {
		return _data.end();
	}

	int size() const {
		return count;
	}

	bool empty() const {
		return count == 0;
	}

	type_t* data() {
		return _data.data();
	}

	const type_t* data() const {
		return _data.data();
	}

	std::array<type_t, capacity> _data;
	int count{};
};