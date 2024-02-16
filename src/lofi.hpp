#pragma once

#include "utils.hpp"

// ops must have the following ops:
// - uint32_t hash(uint32_t)
// - uint32_t hash(const item_t&)
// - bool equals(uint32_t, uint32_t)
// - bool equals(uint32_t, const item_t&)

// hashtable can store 2^20 entries at max
inline constexpr int lofi_max_buckets = 1 << 20;

// unsafe bitfield implementation (yes we are doing compilers job here)
// head - head of the list of items inserted in the current bucket
struct lofi_bucket_data_t {
	lofi_bucket_data_t() = default;

	lofi_bucket_data_t(uint32_t _data) : data{_data} {}

	lofi_bucket_data_t(uint32_t _hash, uint32_t _head) {
		pack(_hash, _head);
	}

	void pack(uint32_t _hash, uint32_t _head) {
		data = 0x00100000;
		data |= _head;
		data |= _hash & 0xFFE00000;
	}

	bool used() const {
		return data & 0x00100000;
	}

	uint32_t head() const {
		return data & 0xFFFFF;
	}
	
	bool hash_equals(uint32_t h) const {
		return (data & 0xFFE00000) == (h & 0xFFE00000);
	}

	uint32_t data{}; // (msb) hash(11) | used(1)| head(20) (lsb)
};

struct lofi_bucket_t {
	bool used() const { return data.used(); }
	uint32_t head() const { return data.head(); }

	lofi_bucket_data_t data{}; // atomic, managed via std::atomic_ref in hashtable
	uint32_t count{}; // atomic, managed via std::atomic_ref in hashtable
};

static_assert(sizeof(lofi_bucket_t) == sizeof(uint64_t), "sizeof lofi_bucket_t must be equal to uint64_t");

// TODO : remove next
// true scans value is scans + 1 (after have been extracted from bit field)
// bucket - index of the used bucket
// next - next item to the inserted item (used externally)
// scans - count of acans made to insert an item, true scans value is scans + 1 (after have been extracted from bit field)
struct lofi_insertion_t {
	enum {
		new_bucket_flag = 0x1,
		inserted_flag = 0x2,
	};

	lofi_insertion_t() = default;

	lofi_insertion_t(uint64_t _data) : data{_data} {}

	lofi_insertion_t(uint64_t _bucket, uint64_t _next, uint64_t _scans, uint64_t _flags) {
		pack(_bucket, _next, _scans, _flags);
	}

	void pack(uint64_t _bucket, uint64_t _next, uint64_t _scans, uint64_t _flags) {
		data = 0;
		data |= _bucket;
		data |= _next << 21;
		data |= _scans << 41;
		data |= _flags << 62;
	}

	uint64_t bucket() const {
		return data & 0x1FFFFF;
	}

	uint64_t next() const {
		return (data >> 21) & 0xFFFFF;
	} 

	uint64_t scans() const {
		return (data >> 41) & 0x1FFFFF;
	}

	bool new_bucket() const {
		return data & ((uint64_t)1 << 62);
	}

	bool inserted() const {
		return data & ((uint64_t)1 << 63);
	}

	// (msb) flags(inserted(1) | new_bucket(1)) | scans(21) | next(20) | bucket(21) (lsb)
	uint64_t data{}; 
};

inline constexpr lofi_insertion_t lofi_failed_insertion{};

// TODO : remove lofi_get_result_t
// scans - count of scans made to search for an element
// true scans value is scans + 1 (after have been extracted from bit field)
struct lofi_get_result_t {
	lofi_get_result_t() = default;

	lofi_get_result_t(uint64_t _data) : data{_data} {}

	lofi_get_result_t(uint64_t _bucket, uint64_t _head, uint64_t _scans, bool _found) {
		pack(_bucket, _head, _scans, _found);
	}

	void pack(uint64_t _bucket, uint64_t _head, uint64_t _scans, bool _found) {
		data = 0;
		data |= _bucket;
		data |= _head << 21;
		data |= _scans << 41;
		data |= (uint64_t)_found << 62;
	}

	uint64_t bucket() const {
		return data & 0x1FFFFF;
	}

	uint64_t head() const {
		return (data >> 21) & 0xFFFFF;
	}

	uint64_t scans() const {
		return (data >> 41) & 0x1FFFFF;
	}

	bool found() const {
		return data & ((uint64_t)1 << 62);
	}

	uint64_t data{}; // (msb) unused(1) | found(1) | scans(21) | head(20) | bucket(21) (lsb)
};

inline constexpr lofi_get_result_t lofi_failed_get{};

struct lofi_search_result_t {
	uint32_t head() const {
		return bucket.head();
	}

	uint32_t count() const {
		return bucket.count;
	}

	bool valid() const {
		return bucket.used();
	}

	lofi_bucket_t bucket{};
	uint32_t bucket_index{};
	uint32_t scans{};
};

// list stored in memory as array of 'pointers' list[curr_item] = next_item_after_curr_item
struct lofi_flat_list_walker_t {
	uint32_t get() const {
		assert(head < flat_size);
		return head;
	}

	bool valid() const {
		return !end_reached;
	}

	void next() {
		assert(head < flat_size);
		uint32_t old_head = head; 
		head = flat_list[head];
		end_reached = old_head == head;
	}

	void skip(int count) {
		for (int i = 0; i < count; i++) {
			next();
		}
	}

	const uint32_t* flat_list{};
	uint32_t flat_size{};
	uint32_t head{};
	bool end_reached{};
};

template<class func_t>
void lofi_walk_flat_list(const uint32_t* flat_list, uint32_t flat_size, uint32_t head, func_t&& func) {
	for (lofi_flat_list_walker_t walker{flat_list, flat_size, head}; walker.valid(); walker.next()) {
		func(walker.get());
	}
}

inline int lofi_flat_list_size(const uint32_t* flat_list, uint32_t flat_size, uint32_t head) {
	int count = 0;
	lofi_walk_flat_list(flat_list, flat_size, head, [&](uint32_t) { count++; });
	return count;
}

// very low-level hashtable interface, all memory management burden is external to this tiny utility
struct lofi_hashtable_t {
	lofi_hashtable_t() = default;
	lofi_hashtable_t(lofi_bucket_t* _buckets, uint32_t _bucket_count, uint32_t* _next_item, uint32_t _item_count) {
		reset(_buckets, _bucket_count, _next_item, _item_count);
	}

	// master
	void reset(lofi_bucket_t* _buckets, uint32_t _bucket_count, uint32_t* _next_item, uint32_t _item_count) {
		assert(std::has_single_bit(_bucket_count));	
		assert(_item_count <= _bucket_count);
		buckets = _buckets;
		capacity_m1 = _bucket_count - 1;
		capacity_log2 = std::countr_zero(_bucket_count);
		next_item = _next_item;
		item_count = _item_count;
	}

	// mt function
	// returned value 'next' is used to build list of values that belong to the same bucket
	// this list (lists) can be walked be lofi_list_walker_t utility
	// last element is an element that has next pointing to itself
	template<class hash_ops_t>
	[[nodiscard]] lofi_insertion_t put(uint32_t item, const hash_ops_t& ops) {
		assert(item < item_count);

		uint32_t hash = ops.hash(item);
		uint32_t bucket_index = hash_to_index(hash);
		for (uint32_t i = 0; i <= capacity_m1; i++) {
			lofi_bucket_t& bucket = buckets[bucket_index];

			lofi_bucket_data_t old_data = std::atomic_ref(bucket.data).load(std::memory_order_relaxed);
			lofi_bucket_data_t new_data{hash, item};

			if (!old_data.used() && std::atomic_ref(bucket.data).compare_exchange_strong(old_data, new_data, std::memory_order_relaxed)) {
				next_item[item] = item;
				std::atomic_ref(bucket.count).fetch_add(1, std::memory_order_relaxed);
				return lofi_insertion_t{bucket_index, 0, i, lofi_insertion_t::new_bucket_flag | lofi_insertion_t::inserted_flag};
			}

			if (old_data.hash_equals(hash) && ops.equals(old_data.head(), item)) {
				old_data = std::atomic_ref(bucket.data).exchange(new_data, std::memory_order_relaxed);
				next_item[item] = old_data.head();
				std::atomic_ref(bucket.count).fetch_add(1, std::memory_order_relaxed);
				return lofi_insertion_t{bucket_index, 0, i, lofi_insertion_t::inserted_flag};
			}

			bucket_index = (bucket_index + 1) & capacity_m1;
		}
		return lofi_failed_insertion;
	}

	// mt function, item_t can either be item itself or its integer handle
	template<class item_t, class hash_ops_t>
	[[nodiscard]] lofi_search_result_t get(const item_t& item, const hash_ops_t& ops) const {
		uint32_t hash = ops.hash(item);
		uint32_t bucket_index = hash_to_index(hash);
		for (uint32_t i = 0; i <= capacity_m1; i++) {
			lofi_bucket_t bucket = buckets[bucket_index]; // we can copy here

			if (bucket.data.used()) {
				uint32_t head = bucket.data.head();
				if (bucket.data.hash_equals(hash) && ops.equals(head, item)) {
					return lofi_search_result_t{bucket, bucket_index, i};
				}
			} else {
				return lofi_search_result_t{};
			}

			bucket_index = (bucket_index + 1) & capacity_m1;
		}
		return lofi_search_result_t{};
	}

	lofi_bucket_t get_bucket(uint32_t index) const {
		return buckets[index];
	}

	uint32_t hash_to_index(uint32_t hash) const {
		return ((hash >> capacity_log2) + hash) & capacity_m1; // add upper bits to lower
	}

	uint32_t hash_to_index2(uint32_t hash) const {
		return hash_to_index((hash >> capacity_log2) + (hash & capacity_m1)); // extra addition
	}

	uint32_t get_capacity() const {
		return capacity_m1 + 1;
	}

	lofi_flat_list_walker_t iter(uint32_t head) const {
		assert(head < item_count);
		return lofi_flat_list_walker_t{next_item, item_count, head};
	}

	lofi_bucket_t* buckets{};
	uint32_t capacity_m1{}; // TODO : rename to bucket_capacity_m1
	uint32_t capacity_log2{}; // TODO : rename to capacity_log2
	uint32_t* next_item{};
	uint32_t item_count{};
};

template<class type_t>
struct lofi_view_t {
	type_t& operator[] (int index) const {
		assert(index >= 0 && index < _size);
		return _data[index];
	}

	type_t* data() const {
		return _data;
	}

	int size() const {
		return _size;
	}

	int byte_size() const {
		return _size * sizeof(type_t);
	}

	bool valid() const {
		return _size != 0;
	}

	auto bite(int amount) {
		assert(amount <= _size);
		lofi_view_t part{_data, amount};
		_data += amount;
		_size -= amount;
		return part;
	}

	type_t* _data{};
	int _size{};
};

template<class type_t>
auto lofi_create_view(type_t* data, int start, int size) {
	return lofi_view_t{data + start, size};
}

template<class type_t>
struct lofi_stack_alloc_t {
	lofi_stack_alloc_t() = default;
	lofi_stack_alloc_t(type_t* _data, int _capacity) {
		reset(_data, _capacity);
	}

	// master
	void reset(type_t* _data, int _capacity) {
		data = _data;
		capacity = _capacity;
		top = 0;
	}

	// worker
	[[nodiscard]] auto allocate(int count) {
		int old_top = std::atomic_ref(top).fetch_add(count, std::memory_order_relaxed);
		if (valid_range(old_top, count)) { // can overflow
			return view(old_top, count);
		}
		return lofi_view_t<type_t>{};
	}

	// worker
	[[nodiscard]] auto view(int start, int count) {
		if (valid_range(start, count)) {
			return lofi_view_t{data + start, count};
		}
		return lofi_view_t<type_t>{};
	}

	[[nodiscard]] auto view_allocated() {
		return lofi_view_t{data, allocated()};
	}

	// worker & master
	int allocated() const {
		return top;
	}

	bool valid_range(int start, int count) const {
		return start + count <= capacity;
		// return start < capacity && count <= capacity && start + count <= capacity; // stricter check
	}

	type_t* data{};
	int capacity{};
	int top{};
};