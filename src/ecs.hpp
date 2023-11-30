#pragma once

#include <tuple>
#include <array>
#include <vector>
#include <cstdint>
#include <cassert>
#include <utility>
#include <type_traits>

namespace impl {
	template<class handle_desc_t>
	struct handle_pool_t {
	public:
		using handle_t = typename handle_desc_t::handle_t;

		inline static constexpr handle_t null_handle = handle_desc_t::null_handle;

		static_assert(std::is_unsigned_v<handle_t>, "handle must be unsigned");

	public:
		[[nodiscard]] handle_t acquire() {
			if (head == null_handle) {
				handle_t handle = handles.size();
				handles.push_back(handle);
				return handle;
			}
			handle_t handle = head;
			head = handles[handle];
			handles[handle] = handle;
			return handle;
		}

		void release(handle_t handle) {
			assert(is_used(handle));
			handles[handle] = head;
			head = handle;
		}

		bool is_used(handle_t handle) const {
			if (handle != null_handle && handle < handles.size()) {
				return handles[handle] == handle;
			}
			return false;
		}

		void clear() {
			head = null_handle;
			handles.clear();
		}

		std::size_t get_use_count() const {
			return handles.size();
		}

	private:
		std::vector<handle_t> handles;
		handle_t head{null_handle};
	};

	template<class index_t, class item_t>
	class sparse_map_t {
	private:
		static_assert(std::is_unsigned_v<index_t>, "index type must be unsigned");

		inline static constexpr index_t items_per_chunk = 64;

		struct mask64_t {
			bool empty() const {
				return mask == 0;
			}

			bool full() const {
				return ~mask == 0;
			}

			bool has(std::uint64_t index) const {
				assert(index < items_per_chunk);
				return mask & ((std::uint64_t)1 << index);
			}

			void toggle(std::uint64_t index) {
				assert(index < items_per_chunk);
				mask ^= (std::uint64_t)1 << index;
			}

			void set(std::uint64_t index) {
				assert(index < items_per_chunk);
				assert(!has(index));
				toggle(index);
			}

			void reset(std::uint64_t index) {
				assert(index < items_per_chunk);
				assert(has(index));
				toggle(index);
			}

			void clear() {
				mask = 0;
			}

			std::uint64_t mask{};
		};

		// TODO : use storage type allowing placement new
		struct chunk_t {
			bool empty() const {
				return mask.empty();
			}

			bool full() const {
				return mask.full();
			}

			bool has(index_t index) const {
				return mask.has(index);
			}

			bool has_storage() const {
				return (bool)elems;
			}

			void create_storage() {
				elems = std::make_unique<std::array<item_t, items_per_chunk>>();
			}

			void destroy_storage() {
				elems.reset();
			}

			void set(index_t index, item_t item) {
				assert(has_storage());
				mask.set(index);
				auto& ref = *elems;
				ref[index] = item;
			}

			item_t& get(index_t index) {
				assert(mask.has(index));
				return (*elems)[index];
			}

			const item_t& get(index_t index) const {
				assert(mask.has(index));
				return (*elems)[index];
			}

			const item_t& get_or_default(index_t index, const item_t& def) const {
				if (!mask.has(index)) {
					return def;
				}
				return (*elems)[index];
			}

			void del(index_t index) {
				assert(has_storage());
				mask.reset(index);
			}

			void clear() {
				mask.clear();
				elems.reset();
			}

			mask64_t mask{};
			std::unique_ptr<std::array<item_t, items_per_chunk>> elems{};
		};

		chunk_t& acquire_chunk(index_t chunk_index) {
			if (chunk_index >= chunks.size()) {
				chunks.resize(chunk_index + 1);
			}

			auto& chunk = chunks[chunk_index];
			if (!chunk.has_storage()) {
				chunk.create_storage();
			}
			return chunk;
		}

	public:
		item_t& get(index_t index) {
			return chunks[index / items_per_chunk].get(index % items_per_chunk);
		}

		const item_t& get(index_t index) const {
			return chunks[index / items_per_chunk].get(index % items_per_chunk);
		}

		void set(index_t index, item_t item) {
			count++;
			acquire_chunk(index / items_per_chunk).set(index % items_per_chunk, item);
		}

		void del(index_t index) {
			count--;
			auto& chunk = chunks[index / items_per_chunk];
			chunk.del(index % items_per_chunk);
			if (chunk.empty()) {
				chunk.destroy_storage();
			}
		}

		bool has(index_t index) const {
			int chunk_index = index / items_per_chunk;
			int item_index = index % items_per_chunk;
			if (chunk_index >= chunks.size()) {
				return false;
			}
			return chunks[chunk_index].has(item_index);
		}

		const item_t& get_or_default(index_t index, const item_t& def) const {
			int chunk_index = index / items_per_chunk;
			int item_index = index % items_per_chunk;
			if (chunk_index >= chunks.size()) {
				return def;
			}
			return chunks[chunk_index].get_or_default(item_index, def);
		}

		int get_size() const {
			return count;
		}

		void clear() {
			chunks.clear();
		}

	private:
		std::vector<chunk_t> chunks;
		int count{};
	};

	template<class ... type_t>
	class sparse_storage_view_t {
	public:
		template<class _sparse_storage_view_t>
		class iter_t {
		public:
			iter_t() = default;

			iter_t(_sparse_storage_view_t* _view, int _current) : view{_view}, current{_current}
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
			_sparse_storage_view_t* view{};
			int current{};
		};

		sparse_storage_view_t() = default;

		sparse_storage_view_t(int _size, type_t* ... view) : views{view...}, size{_size}
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
		auto slice(int start, int stop, std::index_sequence<index...>) const {
			if (start > stop) {
				std::swap(start, stop);
			}
			start = std::clamp(start, 0, size);
			stop = std::clamp(stop, 0, size);
			return sparse_storage_view_t{stop - start, (std::get<index>(views) + start)...};
		}

		auto slice(int start, int stop) const {
			return slice(start, stop, std::index_sequence_for<type_t...>{});
		}

	private:
		std::tuple<type_t*...> views{};
		int size{};
	};
	
	template<class item_t, class index_t>
	void erase_by_last(std::vector<item_t>& storage, index_t index) {
		assert(!storage.empty());
		if (index != (index_t)storage.size() - 1) {
			storage[index] = std::move(storage.back());
		}
		storage.pop_back();
	}

	template<class constructed_t, class ... type_t, std::size_t ... index>
	constructed_t _from_tuple(std::tuple<type_t...>& tuple, std::index_sequence<index...>) {
		if constexpr (std::is_aggregate_v<constructed_t>) {
			return constructed_t{std::get<index>(std::move(tuple))...};
		} else {
			return constructed_t(std::get<index>(std::move(tuple))...);
		}
	}

	template<class constructed_t, class ... type_t>
	constructed_t from_tuple(std::tuple<type_t...> tuple) {
		return _from_tuple<constructed_t>(tuple, std::index_sequence_for<type_t...>{});
	}

	template<class type_t>
	std::add_const_t<type_t>* as_const(type_t* ptr) {
		return ptr;
	}

	template<class handle_desc_t, class ... type_t>
	class sparse_storage_t {
	public:
		using handle_t = typename handle_desc_t::handle_t;

		inline static constexpr handle_t null_handle = handle_desc_t::null_handle;

		static_assert(std::is_unsigned_v<handle_t>, "handle must be unsigned");

	private:
		template<std::size_t ... index>
		auto _emplace(handle_t handle, std::index_sequence<index...>) {
			std::tuple<type_t&...> elems{std::get<index>(storages).emplace_back()...};
			handle_map.set(handle, (handle_t)used_handles.size());
			used_handles.push_back(handle);
			return elems;
		}

		template<std::size_t ... index, class ... _type_t>
		auto _emplace(handle_t handle, std::index_sequence<index...>, _type_t&& ... init) {
			std::tuple<type_t&...> elems{std::get<index>(storages).emplace_back(std::forward<_type_t>(init))...};
			handle_map.set(handle, (handle_t)used_handles.size());
			used_handles.push_back(handle);
			return elems;
		}

		template<std::size_t ... index>
		void _erase(handle_t handle, std::index_sequence<index...>) {
			handle_t storage_index = handle_map.get_or_default(handle, null_handle);
			if (storage_index == null_handle) {
				return;
			}

			handle_map.del(handle);

			(erase_by_last(std::get<index>(storages), storage_index), ...);
			erase_by_last(used_handles, storage_index);

			if (storage_index != (handle_t)used_handles.size()) {
				handle_map.get(used_handles[storage_index]) = storage_index;
			}
		}

		template<std::size_t ... index>
		void _clear(std::index_sequence<index...>) {
			(std::get<index>(storages).clear(), ...);
		}

		template<std::size_t ... index>
		auto _get(handle_t handle, std::index_sequence<index...>) {
			assert(has(handle));
			handle_t storage_index = handle_map.get(handle);
			return std::tuple<type_t&...>{std::get<index>(storages)[storage_index]...};
		}

		template<std::size_t ... index>
		auto _get(handle_t handle, std::index_sequence<index...>) const {
			assert(has(handle));
			handle_t storage_index = handle_map.get(handle);
			return std::tuple<const type_t&...>{std::get<index>(storages)[storage_index]...};
		}

		template<std::size_t ... index>
		auto _view(std::index_sequence<index...>) {
			return sparse_storage_view_t{(int)used_handles.size(), (const handle_t*)(used_handles.data()), std::get<index>(storages).data()...};
		}

		template<std::size_t ... index>
		auto _view(std::index_sequence<index...>) const {
			return sparse_storage_view_t{(int)used_handles.size(), (const handle_t*)(used_handles.data()), std::get<index>(storages).data()...};
		}

	public:
		auto emplace(handle_t handle) {
			assert(handle != null_handle);
			return _emplace(handle, std::index_sequence_for<type_t...>{});
		}

		template<class ... _type_t>
		auto emplace(handle_t handle, _type_t&& ... init) {
			assert(handle != null_handle);
			return _emplace(handle, std::index_sequence_for<type_t...>{}, std::forward<_type_t>(init)...);
		}

		void erase(handle_t handle) {
			assert(handle != null_handle);
			_erase(handle, std::index_sequence_for<type_t...>{});
		}

		auto get(handle_t handle) {
			return _get(handle, std::index_sequence_for<type_t...>{});
		}

		auto get(handle_t handle) const {
			return _get(handle, std::index_sequence_for<type_t...>{});
		}

		auto const_get(handle_t handle) const {
			return get(handle);
		}

		bool has(handle_t handle) const {
			assert(handle != null_handle);
			return handle_map.get_or_default(handle, null_handle) != null_handle;
		}

		bool empty() const {
			return used_handles.empty();
		}

		int size() const {
			return (int)used_handles.size();
		}

		auto view() {
			return _view(std::index_sequence_for<type_t...>{});
		}

		auto view() const {
			return _view(std::index_sequence_for<type_t...>{});
		}

		auto const_view() const {
			return view();
		}

		auto handles_view() const {
			return _view(std::index_sequence<>{});
		}

		void clear() {
			_clear(std::index_sequence_for<type_t...>{});
			used_handles.clear();
			handle_map.clear();
		}

	private:
		std::tuple<std::vector<type_t>...> storages;
		std::vector<handle_t> used_handles{};
		sparse_map_t<handle_t, handle_t> handle_map{};
	};
}

struct handle_desc_t {
	using handle_t = unsigned;

	inline static constexpr handle_t null_handle = ~0;
};

using handle_t = typename handle_desc_t::handle_t;

inline constexpr handle_t null_handle = handle_desc_t::null_handle;

using handle_pool_t = impl::handle_pool_t<handle_desc_t>;

using impl::sparse_map_t;
using impl::sparse_storage_view_t;

template<class ... type_t>
using sparse_storage_t = impl::sparse_storage_t<handle_desc_t, type_t...>;