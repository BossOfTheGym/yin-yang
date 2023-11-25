#include <tuple>
#include <array>
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <type_traits>

template<class type_t>
class chunked_vector_t {
private:
	inline static constexpr int elems_per_chunk = 64;

	struct mask64_t {
		bool empty() const {
			return mask == 0;
		}

		bool full() const {
			return ~mask == 0;
		}

		bool has(std::uint64_t index) const {
			assert(index < elems_per_chunk);
			return mask & ((std::uint64_t)1 << index);
		}

		void toggle(std::uint64_t index) {
			assert(index < elems_per_chunk);
			mask ^= (std::uint64_t)1 << index;
		}

		void set(std::uint64_t index) {
			assert(index < elems_per_chunk);
			assert(!has(index));
			toggle(index);
		}

		void reset(std::uint64_t index) {
			assert(index < elems_per_chunk);
			assert(has(index));
			toggle(index);
		}

		std::uint64_t mask{};
	};

	struct chunk_t {
		bool empty() const {
			return mask.empty();
		}

		bool full() const {
			return mask.full();
		}

		bool has(int index) const {
			return mask.has(index);
		}

		bool has_storage() const {
			return (bool)elems;
		}

		void create_storage(type_t init = {}) {
			elems = std::make_unique<std::array<type_t, elems_per_chunk>>();
			for (auto& elem : *elems) {
				elem = init;
			}
		}

		void destroy_storage() {
			elems.reset();
		}

		void set(int index, type_t elem) {
			assert(has_storage());
			mask.set(index);
			(*elems)[index] = elem;
		}

		type_t& get(int index) {
			assert(mask.has(index));
			return (*elems)[index];
		}

		const type_t& get(int index) const {
			assert(mask.has(index));
			return (*elems)[index];
		}

		type_t get_or_default(int index, type_t def) const {
			if (!mask.has(index)) {
				return def;
			}
			return (*elems)[index];
		}

		void del(int index) {
			assert(has_storage());
			mask.reset(index);
		}

		mask64_t mask{};
		std::unique_ptr<std::array<type_t, elems_per_chunk>> elems{};
	};

	chunk_t& acquire_chunk(int chunk_index) {
		if (chunk_index >= chunks.size()) {
			chunks.resize(chunk_index + 1);
		}

		auto& chunk = chunks[chunk_index];
		if (!chunk.has_storage()) {
			chunk.create_storage(init);
		}
		return chunk;
	}

public:
	chunked_vector_t(type_t _init = {}) : init{_init} {}

	type_t& get(int index) {
		assert(index >= 0);
		return chunks[index / elems_per_chunk].get(index % elems_per_chunk);
	}

	const type_t& get(int index) const {
		return const_cast<chunked_vector_t*>(this)->get(index);
	}

	void set(int index, type_t elem) {
		assert(index >= 0);
		acquire_chunk(index / elems_per_chunk).set(index % elems_per_chunk, elem);
	}

	void del(int index) {
		assert(index >= 0);
		auto& chunk = chunks[index / elems_per_chunk];
		chunk.del(index % elems_per_chunk);
		if (chunk.empty()) {
			chunk.destroy_storage();
		}
	}

	bool has(int index) const {
		assert(index >= 0);
		int chunk_index = index / elems_per_chunk;
		int elem_index = index % elems_per_chunk;
		if (chunk_index >= chunks.size()) {
			return false;
		}
		return chunks[chunk_index].has(elem_index);
	}

	type_t get_or_default(int index) const {
		assert(index >= 0);
		int chunk_index = index / elems_per_chunk;
		int elem_index = index % elems_per_chunk;
		if (chunk_index >= chunks.size()) {
			return init;
		}
		return chunks[chunk_index].get_or_default(elem_index, init);
	}

	int get_size() const {
		return chunks.size() * elems_per_chunk;
	}

private:
	std::vector<chunk_t> chunks;
	type_t init{};
};

template<class type_t>
void erase_by_last(std::vector<type_t>& storage, int index) {
	assert(!storage.empty());
	if (index != (int)storage.size() - 1) {
		storage[index] = std::move(storage.back());
	}
	storage.pop_back();
}

template<class ... type_t>
class storage_view_t {
public:
	template<class _storage_view_t>
	class iter_t {
	public:
		iter_t() = default;

		iter_t(_storage_view_t* _view, int _current) : view{_view}, current{_current}
		{}

		auto operator* () const {
			assert(valid());
			return view->get(current);
		}

		iter_t& operator++ () {
			current++;
			return *this;
		}

		iter_t operator++ (int) {
			iter_t iter{*this};
			return ++(*this), iter;
		}

		iter_t operator+ (std::ptrdiff_t diff) {
			return iter_t{view, current + diff};
		}

		iter_t& operator-- () {
			current--;
			return *this;
		}

		iter_t operator-- (int) {
			iter_t iter{*this};
			return --(*this), iter;
		}

		iter_t operator- (std::ptrdiff_t diff) {
			return iter_t{view, current - diff};
		}

		bool operator== (iter_t iter) const {
			assert(view == iter.view);
			return current == iter.current;
		}

		explicit operator bool() const {
			return valid();
		}

		bool valid() const {
			return view && current >= 0 && current < view->size;
		}

	private:
		_storage_view_t* view{};
		int current{};
	};

	storage_view_t() = default;

	storage_view_t(int _size, type_t* ... view) : views{view...}, size{_size}
	{}

private:
	template<bool use_const, std::size_t ... index>
	auto _get(int elem_index, std::index_sequence<index...>) const {
		if constexpr (!use_const) {
			return std::tuple<type_t&...>{std::get<index>(views)[elem_index]...};
		} else {
			return std::tuple<const type_t&...>{std::get<index>(views)[elem_index]...};
		}
	}

public:
	auto get(int index) {
		assert(index >= 0 && index < size);
		return _get<false>(index, std::index_sequence_for<type_t...>{});
	}

	auto get(int index) const {
		assert(index >= 0 && index < size);
		return _get<true>(index, std::index_sequence_for<type_t...>{});
	}

	auto operator[] (int index) {
		return get(index);
	}

	auto operator[] (int index) const {
		return get(index);
	}

	int get_size() const {
		return size;
	}

	bool empty() const {
		return size == 0;
	}

	auto begin() {
		return iter_t{this, 0};
	}

	auto end() {
		return iter_t{this, size};
	}

	auto begin() const {
		return iter_t{this, 0};
	}

	auto end() const {
		return iter_t{this, size};
	}

	template<std::size_t ... index>
	storage_view_t slice(int start, int stop, std::index_sequence<index...>) const {
		if (start > stop) {
			std::swap(start, stop);
		}
		start = std::clamp(start, 0, size);
		stop = std::clamp(stop, 0, size);
		return storage_view_t{stop - start, (std::get<index>(views) + start)...};
	}

	storage_view_t slice(int start, int stop) const {
		return slice(start, stop, std::index_sequence_for<type_t...>{});
	}

private:
	std::tuple<type_t*...> views{};
	int size{};
};

template<class ... type_t>
class storage_t {
private:
	template<std::size_t ... index>
	auto _emplace(int handle, std::index_sequence<index...>) {
		std::tuple<type_t&...> elems{std::get<index>(storages).emplace_back()...};
		handle_map.set(handle, used_handles.size());
		used_handles.push_back(handle);
		return elems;
	}

	template<std::size_t ... index>
	void _erase(int handle, std::index_sequence<index...>) {
		int storage_index = handle_map.get(handle);
		handle_map.get(handle) = -1;
		handle_map.del(handle); // mark as deleted

		(erase_by_last(std::get<index>(storages), storage_index), ...);
		erase_by_last(used_handles, storage_index);

		if (storage_index != (int)used_handles.size()) {
			handle_map.get(used_handles[storage_index]) = storage_index;
		}
	}

	template<class handles_t, class storage_tuple_t, std::size_t ... index>
	static auto _get_view(const handles_t& handles, storage_tuple_t& storage_tuple, std::index_sequence<index...>) {
		return storage_view_t{(int)handles.size(), handles.data(), std::get<index>(storage_tuple).data()...};
	}

public:
	auto emplace(int handle) {
		return _emplace(handle, std::index_sequence_for<type_t...>{});
	}

	void erase(int handle) {
		_erase(handle, std::index_sequence_for<type_t...>{});
	}

	bool has(int handle) const {
		assert(handle >= 0);
		return handle_map.get_or_default(handle) != -1; // we either get valid index or get -1
	}

	bool empty() const {
		return used_handles.empty();
	}

	int get_size() const {
		return (int)used_handles.size();
	}

	auto view() {
		return _get_view(used_handles, storages, std::index_sequence_for<type_t...>{});
	}

	auto view() const {
		return _get_view(used_handles, storages, std::index_sequence_for<type_t...>{});
	}

	template<std::size_t ... index>
	auto view() {
		return _get_view(used_handles, storages, std::index_sequence<index...>{});
	}

	template<std::size_t ... index>
	auto view() const {
		return _get_view(used_handles, storages, std::index_sequence<index...>{});
	}

	auto handles_view() const {
		return _get_view(used_handles, storages, std::index_sequence<>{});
	}

private:
	std::tuple<std::vector<type_t>...> storages;
	std::vector<int> used_handles{};
	chunked_vector_t<int> handle_map{-1}; // default element is -1
};

struct some_t {
	int a{};
	int b{};
};

int main() {
	std::cout << "view test" << "\n";

	int a[4] = {1, 2, 3, 4};
	const double b[4] = {1, 2, 3, 4};
	short c[4] = {1, 2, 3, 4};

	storage_view_t view{4, a, b, c};
	const auto& const_view = view;

	for (auto [av, bv, cv] : view) {
		std::cout << av << " " << bv << " " << cv << "\n";
	}

	std::cout << "\n";
	for (auto [av, bv, cv] : view.slice(1, 3)) {
		std::cout << av << " " << bv << " " << cv << "\n";
	}

	std::cout << "\n";
	for (auto [av, bv, cv] : view.slice(-1, -1)) {
		std::cout << av << " " << bv << " " << cv << "\n";
	}

	std::cout << "\n";
	for (auto [av, bv, cv] : const_view) {
		std::cout << av << " " << bv << " " << cv << "\n";
	}

	std::cout << "\n";

	std::cout << "storage test" << "\n";
	storage_t<int, float, some_t> storage;
	storage.emplace(0);
	storage.emplace(1);
	storage.emplace(2);

	const auto& const_storage = storage;
	std::cout << "\n";
	for (auto [handle, _1, _2, _3] : const_storage.view()) {
		std::cout << handle << "\n";
	}

	std::cout << "\n";
	for (int i = 0; i < 5; i++) {
		std::cout << storage.has(i) << "\n";
	}

	storage.erase(1);

	std::cout << "\n";
	for (int i = 0; i < 5; i++) {
		std::cout << storage.has(i) << "\n";
	}

	storage.emplace(1);
	storage.emplace(64);

	std::cout << "\n";
	for (auto [handle, _1, _2, _3] : storage.view()) {
		std::cout << handle << "\n";
	}

	storage.erase(64);

	std::cout << "\n";
	for (auto [handle, _1, _2, _3] : storage.view()) {
		std::cout << handle << "\n";
	}

	std::cout << "\n";
	for (auto [handle] : storage.handles_view()) {
		std::cout << handle << " ";
	}
	std::cout << "\n";

	storage_t<> storage_empty;
	storage_empty.emplace(0);

	return 0;
}