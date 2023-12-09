#pragma once

#include "utils.hpp"

using lofi_hasher_t = callback_t<std::uint32_t(int)>;
using lofi_equals_t = callback_t<bool(int, int)>;

struct lofi_bucket_t {
	void reset() {
		head.store(-1, std::memory_order_relaxed);
		count.store(0, std::memory_order_relaxed);
	}

	int get_head() const {
		return head.load(std::memory_order_relaxed);
	}

	int get_count() const {
		return count.load(std::memory_order_relaxed);
	}

	std::atomic<int> head{}; // default : -1
	std::atomic<int> count{}; // default : 0
};

struct lofi_list_entry_t {
	int next{};
};

struct lofi_item_iter_t {
	int get() {
		return curr;
	}

	void next() {
		int next = list_entries[curr].next;
		curr = curr != next ? next : -1;
	}

	bool valid() const {
		return curr != -1;
	}

	const lofi_list_entry_t* list_entries{};
	int curr{};
};

// ops must have the following ops:
// - std::uint32_t hash(int)
// - std::uint32_t hash(const item_t&)
// - bool equals(int, int)
// - bool equals(int, const item_t&)

template<class ops_t>
struct lofi_hashtable1_t {
	static constexpr int inflation_coef = 2;

	static_assert(std::has_single_bit<unsigned>(inflation_coef), "must be power of 2");

	lofi_hashtable1_t(const lofi_hashtable1_t&) = delete;
	lofi_hashtable1_t& operator= (const lofi_hashtable1_t&) = delete;
	lofi_hashtable1_t(lofi_hashtable1_t&&) noexcept = delete;
	lofi_hashtable1_t& operator= (lofi_hashtable1_t&&) noexcept = delete;

	lofi_hashtable1_t(int _max_size, ops_t _ops)
		: ops{std::move(_ops)}
		, max_capacity{nextpow2(_max_size) * inflation_coef}
	{
		buckets = std::make_unique<lofi_bucket_t[]>(max_capacity);
		list_entries = std::make_unique<lofi_list_entry_t[]>(max_capacity / inflation_coef);
	}

	// master
	void reset_size(int bucket_count_hint) {
		object_count = std::min(max_capacity / inflation_coef, bucket_count_hint);
		capacity_m1 = std::min(max_capacity, nextpow2(object_count) * inflation_coef) - 1;
		capacity_log2 = std::countr_zero<unsigned>(capacity_m1 + 1);
	}

	// worker
	// used by worker to reset the range of buckets
	lofi_bucket_t* get_buckets_data(int start = 0) {
		return buckets.get() + start;
	}

	// worker
	// returns either index of a bucket that was just used or -1 if it was already used
	struct put_result_t {
		int bucket_index{};
		int scans{}; // well'p mostly used for testing, on practical usage
	};

	put_result_t put(int item) {
		int bucket_index = hash_to_index(ops.hash(item));
		for (int i = 0; i <= capacity_m1; i++) {
			auto& bucket = buckets[bucket_index];

			int head_old = bucket.head.load(std::memory_order_relaxed);
			if (head_old == -1 && bucket.head.compare_exchange_strong(head_old, item, std::memory_order_relaxed)) {
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = item;
				return {bucket_index, i + 1};
			}
			if (ops.equals(head_old, item)) { // totaly legit to use possibly invalid head here
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = bucket.head.exchange(item, std::memory_order_relaxed); // head can be invalid
				return {-1, i + 1};
			}

			bucket_index = (bucket_index + 1) & capacity_m1;
		}
		return {-1, -1};
	}


	// worker & master
	lofi_bucket_t& get_bucket(int index) {
		return buckets[index];
	}

	const lofi_bucket_t& get_bucket(int index) const {
		return buckets[index];
	}

	template<class item_or_handle_t>
	int get(const item_or_handle_t& item) const {
		int bucket_index = hash_to_index(ops.hash(item));
		for (int i = 0; i <= capacity_m1; i++) {
			auto& bucket = buckets[bucket_index];

			int head = bucket.head.load(std::memory_order_relaxed);
			if (head == -1) {
				return -1;
			}
			if (ops.equals(head, item)) {
				return bucket_index;
			}

			bucket_index = (bucket_index + 1) & capacity_m1;
		}
		return -1;
	}

	int hash_to_index(std::uint32_t hash) const {
		return ((hash >> capacity_log2) + hash) & capacity_m1; // add upper bits to lower
	}

	int get_buckets_count() const {
		return capacity_m1 + 1;
	}

	int get_object_count() const {
		return object_count;
	}

	lofi_item_iter_t iter(int bucket_index) const {
		return lofi_item_iter_t{list_entries.get(), buckets[bucket_index].head.load(std::memory_order_relaxed)};
	}


	int capacity_m1{};
	int capacity_log2{};

	ops_t ops;

	std::unique_ptr<lofi_bucket_t[]> buckets; // resized to capacity (increased capacity to decrease contention)
	std::unique_ptr<lofi_list_entry_t[]> list_entries; // size = object_count

	const int max_capacity;
	int object_count{};
};

template<class ops_t>
struct lofi_hashtable2_t {
	static constexpr int inflation_coef = 2; // power of 2
	static constexpr int capacity_split_coef_log2 = 2;
	static constexpr int capacity_split_coef = 1 << capacity_split_coef_log2; // power of 2
	static constexpr int max_scans_per_split = 3;

	static_assert(std::has_single_bit<unsigned>(inflation_coef), "must be power of 2");
	static_assert(std::has_single_bit<unsigned>(capacity_split_coef), "must be power of 2");

	static int hash_to_index(std::uint32_t hash, std::uint32_t size_log2, std::uint32_t size_m1) {
		return ((hash >> size_log2) + hash) & size_m1; // add upper bits to lower
	}

	static std::uint32_t hash_repeat(std::uint32_t hash) {
		return (hash * 0x85ebca6b + (hash >> 17));
	}

	lofi_hashtable2_t(const lofi_hashtable2_t&) = delete;
	lofi_hashtable2_t& operator= (const lofi_hashtable2_t&) = delete;
	lofi_hashtable2_t(lofi_hashtable2_t&&) noexcept = delete;
	lofi_hashtable2_t& operator= (lofi_hashtable2_t&&) noexcept = delete;

	lofi_hashtable2_t(int _max_size, ops_t _ops)
		: ops{std::move(_ops)}
		, max_capacity{std::max(nextpow2(_max_size), capacity_split_coef) * inflation_coef} // so it is not that small and divisible by capacity_split_coef
	{
		buckets = std::make_unique<lofi_bucket_t[]>(max_capacity);
		list_entries = std::make_unique<lofi_list_entry_t[]>(max_capacity / inflation_coef);
	}

	// master
	void reset_size(int bucket_count_hint) {
		// split_capacity_m1 + 1 == 1 << split_capacity_log2
		split_capacity_m1 = std::min(max_capacity, nextpow2(bucket_count_hint) * inflation_coef) / capacity_split_coef - 1;
		split_capacity_log2 = std::countr_zero<unsigned>(split_capacity_m1 + 1);
	}

	// worker
	// used by worker to reset the range of buckets
	lofi_bucket_t* get_buckets_data(int start = 0) {
		return buckets.get() + start;
	}

	// worker
	// returns either index of a bucket that was just used or -1 if it was already used
	struct put_result_t {
		int bucket_index{};
		int scans{}; // well'p mostly used for testing, on practical usage
	};

	put_result_t _put(lofi_bucket_t* buckets_split, int split_size_m1, int base_index, int item, int max_scans) {
		int bucket_index = base_index;
		for (int i = 0; i < max_scans; i++) {
			auto& bucket = buckets_split[bucket_index];

			int old_head = bucket.head.load(std::memory_order_relaxed);
			if (old_head == -1 && bucket.head.compare_exchange_strong(old_head, item, std::memory_order_relaxed)) {
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = item;
				return {bucket_index, i + 1};
			}
			if (ops.equals(old_head, item)) { // totaly legit to use possibly invalid head here
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = bucket.head.exchange(item, std::memory_order_relaxed); // head can be invalid
				return {-1, i + 1};
			}

			bucket_index = (bucket_index + 1) & split_size_m1;
		}

		// we failed to find empty bucket within scan limit
		return {-1, -1};
	}

	put_result_t put(int item) {
		// try to insert to splits first
		int total_scans = 0;
		std::uint32_t hash = ops.hash(item);
		for (int i = 0; i < capacity_split_coef; i++) {
			const int offset = i << split_capacity_log2;
			const int base_index = hash_to_index(hash, split_capacity_log2, split_capacity_m1);

			auto [bucket_index, scans] = _put(buckets.get() + offset, split_capacity_m1, base_index, item, max_scans_per_split);
			if (scans != -1) {
				return {bucket_index != -1 ? bucket_index + offset : -1, total_scans + scans};
			}

			hash = hash_repeat(hash);
			total_scans += max_scans_per_split;
		}

		// force insertion into the whole hashtable, no scan limit
		const int size_m1 = (capacity_split_coef << split_capacity_log2) - 1;
		const int size_log2 = capacity_split_coef_log2 + split_capacity_log2;

		auto [bucket_index, scans] = _put(buckets.get(), size_m1, hash_to_index(hash, size_log2, size_m1), item, size_m1 + 1);
		if (scans != -1) {
			return {bucket_index, total_scans + scans};
		}

		// we failed to insert, must never happen if we do not go out of count limit
		return {-1, -1};
	}

	// worker & master
	lofi_bucket_t& get_bucket(int index) {
		return buckets[index];
	}

	const lofi_bucket_t& get_bucket(int index) const {
		return buckets[index];
	}

	template<class item_or_handle_t>
	int _get(const lofi_bucket_t* buckets_split, int split_size_m1, int base_index, const item_or_handle_t& item, int max_scans) const {
		int bucket_index = base_index;
		for (int i = 0; i < max_scans; i++) {
			auto& bucket = buckets_split[bucket_index];
			int head = bucket.head.load(std::memory_order_relaxed);
			if (head == -1) {
				return -1;
			}
			if (ops.equals(head, item)) {
				return bucket_index;
			}
			bucket_index = (bucket_index + 1) & split_size_m1;
		}
		return -1;
	}

	template<class item_or_handle_t>
	int get(const item_or_handle_t& item) const {
		// search in splits first
		std::uint32_t hash = ops.hash(item);
		for (int i = 0; i < max_scans_per_split; i++) {
			const int offset = i << split_capacity_log2;
			const int base_index = hash_to_index(hash, split_capacity_log2, split_capacity_m1);

			int bucket = _get(buckets.get() + offset, split_capacity_m1, base_index, item, max_scans_per_split);
			if (bucket != -1) {
				return bucket + offset;
			}

			hash = hash_repeat(hash);
		}

		// search in whole hashtable
		const int size_m1 = (capacity_split_coef << split_capacity_log2) - 1;
		const int size_log2 = capacity_split_coef_log2 + split_capacity_log2;

		return _get(buckets.get(), size_m1, hash_to_index(hash, size_log2, size_m1), item, size_m1 + 1);
	}

	int get_buckets_count() const {
		return capacity_split_coef << split_capacity_log2;
	}

	int get_object_count() const {
		return object_count;
	}

	lofi_item_iter_t iter(int bucket_index) const {
		return lofi_item_iter_t{list_entries.get(), buckets[bucket_index].head.load(std::memory_order_relaxed)};
	}


	int split_capacity_m1{};
	int split_capacity_log2{};

	ops_t ops;

	std::unique_ptr<lofi_bucket_t[]> buckets; // nextpow2(object_count) * inflation_coef
	std::unique_ptr<lofi_list_entry_t[]> list_entries; // always size = object_count

	const int max_capacity;
	int object_count{};
};

template<class data_t>
struct lofi_stack_t {
	lofi_stack_t(int _max_size)
		: max_size{_max_size} {
		data = std::make_unique<data_t[]>(max_size);
	}

	// master
	void reset() {
		size.store(0, std::memory_order_relaxed);
	}

	// worker
	data_t* push(int count) {
		int start = size.fetch_add(count, std::memory_order_relaxed);
		assert(start + count <= max_size);
		return data.get() + start;
	}

	// worker
	data_t* get_data(int start) {
		return data.get() + start;
	}

	// worker & master
	int get_size() const {
		return size.load(std::memory_order_relaxed);
	}

	data_t& operator[] (int i) {
		return data[i];
	}

	const data_t& operator[] (int i) const {
		return data[i];
	}

	const int max_size;
	std::unique_ptr<data_t[]> data;
	std::atomic<int> size{};
};