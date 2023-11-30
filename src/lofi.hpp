#pragma once

#include "utils.hpp"

using lofi_hasher_t = callback_t<std::uint32_t(int)>;
using lofi_equals_t = callback_t<bool(int, int)>;

struct lofi_hashtable_t {
	static constexpr int inflation_coef = 2;

	struct bucket_t {
		void reset() {
			head.store(-1, std::memory_order_relaxed);
			count.store(0, std::memory_order_relaxed);
		}

		std::atomic<int> head{}; // default : -1
		std::atomic<int> count{}; // default : 0
	};

	struct list_entry_t {
		int next{};
	};

	lofi_hashtable_t(const lofi_hashtable_t&) = delete;
	lofi_hashtable_t& operator= (const lofi_hashtable_t&) = delete;
	lofi_hashtable_t(lofi_hashtable_t&&) noexcept = delete;
	lofi_hashtable_t& operator= (lofi_hashtable_t&&) noexcept = delete;

	lofi_hashtable_t(int _max_size, lofi_hasher_t _hasher, lofi_equals_t _equals)
		: hasher{_hasher}
		, equals{_equals}
		, max_capacity{nextpow2(_max_size) * inflation_coef}
	{
		buckets = std::make_unique<bucket_t[]>(max_capacity);
		list_entries = std::make_unique<list_entry_t[]>(max_capacity / inflation_coef);
	}

	// master
	void reset_size(int new_object_count) {
		object_count = std::min(max_capacity / inflation_coef, new_object_count);
		capacity_m1 = std::min(max_capacity, nextpow2(object_count) * inflation_coef) - 1;
		capacity_log2 = std::countr_zero<unsigned>(capacity_m1 + 1);
	}

	// worker
	// used by worker to reset the range of buckets
	bucket_t* get_buckets_data(int start = 0) {
		return buckets.get() + start;
	}

	// worker
	// returns either index of a bucket that was just used or -1 if it was already used
	struct put_result_t {
		int bucket_index{};
		int scans{};
	};

	put_result_t put(int item) {
		int bucket_index = hash_to_index(hasher(item));
		for (int i = 0; i <= capacity_m1; i++) {
			auto& bucket = buckets[bucket_index];

			int head_old = bucket.head.load(std::memory_order_relaxed);
			if (head_old == -1 && bucket.head.compare_exchange_strong(head_old, item, std::memory_order_relaxed)) {
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = item;
				return {bucket_index, i + 1};
			}
			if (equals(head_old, item)) { // totaly legit to use possibly invalid head here
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = bucket.head.exchange(item, std::memory_order_relaxed); // head can be invalid
				return {-1, i + 1};
			}

			bucket_index = (bucket_index + 1) & capacity_m1;
		}
		return {-1, -1};
	}


	// worker & master
	const bucket_t& get_bucket(int index) const {
		return buckets[index];
	}

	const bucket_t* get(int item) const {
		int bucket_index = hash_to_index(hasher(item));
		for (int i = 0; i <= capacity_m1; i++) {
			auto& bucket = buckets[bucket_index];

			int head = bucket.head.load(std::memory_order_relaxed);
			if (head == -1) {
				return nullptr; // must never happen if all elements were inserted successfully
			}
			if (equals(head, item)) {
				return &bucket;
			}

			bucket_index = (bucket_index + 1) & capacity_m1;
		}
		return nullptr;
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


	int capacity_m1{};
	int capacity_log2{};

	lofi_hasher_t hasher;
	lofi_equals_t equals;

	std::unique_ptr<bucket_t[]> buckets; // resized to capacity (increased capacity to decrease contention)
	std::unique_ptr<list_entry_t[]> list_entries; // size = object_count

	const int max_capacity;
	int object_count{};
};

struct lofi_hashtable2_t {
	static constexpr int inflation_coef = 2; // power of 2
	static constexpr int capacity_split_coef_log2 = 2;
	static constexpr int capacity_split_coef = 1 << capacity_split_coef_log2; // power of 2
	static constexpr int max_scans_per_split = 8;

	static_assert(std::has_single_bit<unsigned>(inflation_coef), "must be power of 2");
	static_assert(std::has_single_bit<unsigned>(capacity_split_coef), "must be power of 2");

	static int hash_to_index(std::uint32_t hash, std::uint32_t size_log2, std::uint32_t size_m1) {
		return ((hash >> size_log2) + hash) & size_m1; // add upper bits to lower
	}

	static std::uint32_t hash_repeat(std::uint32_t hash) {
		return (hash * 0x85ebca6b + (hash >> 17));
	}

	struct bucket_t {
		void reset() {
			head.store(-1, std::memory_order_relaxed);
			count.store(0, std::memory_order_relaxed);
		}

		std::atomic<int> head{}; // default : -1
		std::atomic<int> count{}; // default : 0
	};

	struct list_entry_t {
		int next{};
	};

	lofi_hashtable2_t(const lofi_hashtable2_t&) = delete;
	lofi_hashtable2_t& operator= (const lofi_hashtable2_t&) = delete;
	lofi_hashtable2_t(lofi_hashtable2_t&&) noexcept = delete;
	lofi_hashtable2_t& operator= (lofi_hashtable2_t&&) noexcept = delete;

	lofi_hashtable2_t(int _max_size, lofi_hasher_t _hasher, lofi_equals_t _equals)
		: hasher{_hasher}
		, equals{_equals}
		, max_capacity{std::max(nextpow2(_max_size), capacity_split_coef) * inflation_coef} // so it is not that small and divisible by capacity_split_coef
	{
		buckets = std::make_unique<bucket_t[]>(max_capacity);
		list_entries = std::make_unique<list_entry_t[]>(max_capacity / inflation_coef);
	}

	// master
	void reset_size(int new_object_count) {
		object_count = std::min(max_capacity / inflation_coef, new_object_count);
		// split_capacity_m1 + 1 == 1 << split_capacity_log2
		split_capacity_m1 = std::min(max_capacity, nextpow2(object_count) * inflation_coef / capacity_split_coef) - 1;
		split_capacity_log2 = std::countr_zero<unsigned>(split_capacity_m1 + 1);
	}

	// worker
	// used by worker to reset the range of buckets
	bucket_t* get_buckets_data(int start = 0) {
		return buckets.get() + start;
	}

	// worker
	// returns either index of a bucket that was just used or -1 if it was already used
	struct put_result_t {
		int bucket_index{};
		int scans{};
	};

	put_result_t _put(bucket_t* buckets_split, int split_size_m1, int base_index, int item, int max_scans) {
		int bucket_index = base_index;
		for (int i = 0; i < max_scans; i++) {
			auto& bucket = buckets_split[bucket_index];

			int head_old = bucket.head.load(std::memory_order_relaxed);
			if (head_old == -1 && bucket.head.compare_exchange_strong(head_old, item, std::memory_order_relaxed)) {
				bucket.count.fetch_add(1, std::memory_order_relaxed);
				list_entries[item].next = item;
				return {bucket_index, i + 1};
			}
			if (equals(head_old, item)) { // totaly legit to use possibly invalid head here
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
		std::uint32_t hash = hasher(item);
		for (int i = 0; i < capacity_split_coef; i++) {
			const int offset = i << split_capacity_log2;
			const int base_index = hash_to_index(hash, split_capacity_log2, split_capacity_m1);

			auto [bucket_index, scans] = _put(buckets.get() + offset, split_capacity_m1, base_index, item, max_scans_per_split);
			if (scans != -1) {
				return {bucket_index, total_scans + scans};
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
	const bucket_t& get_bucket(int index) const {
		return buckets[index];
	}

	const bucket_t* _get(bucket_t* buckets_split, int split_size_m1, int base_index, int item, int max_scans) const {
		int bucket_index = base_index;
		for (int i = 0; i < max_scans; i++) {
			auto& bucket = buckets_split[bucket_index];
			int head = bucket.head.load(std::memory_order_relaxed);
			if (head == -1) {
				return nullptr; // must never happen if all items were succsessfully inserted
			}
			if (equals(head, item)) {
				return &bucket;
			}
			bucket_index = (bucket_index + 1) & split_size_m1;
		}
		return nullptr;
	}

	const bucket_t* get(int item) const {
		// search in splits first
		std::uint32_t hash = hasher(item);
		for (int i = 0; i < max_scans_per_split; i++) {
			const int offset = i << split_capacity_log2;
			const int base_index = hash_to_index(hash, split_capacity_log2, split_capacity_m1);

			auto* bucket = _get(buckets.get() + offset, split_capacity_m1, base_index, item, max_scans_per_split);
			if (bucket) {
				return bucket;
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


	int split_capacity_m1{};
	int split_capacity_log2{};

	lofi_hasher_t hasher;
	lofi_equals_t equals;

	std::unique_ptr<bucket_t[]> buckets; // nextpow2(object_count) * inflation_coef
	std::unique_ptr<list_entry_t[]> list_entries; // always size = object_count

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

	int get_size() const {
		return size.load(std::memory_order_relaxed);
	}

	const int max_size;
	std::unique_ptr<data_t[]> data;
	std::atomic<int> size{};
};
