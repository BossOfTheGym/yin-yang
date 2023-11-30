#include <set>
#include <map>
#include <unordered_map>
#include <tuple>
#include <array>
#include <vector>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <type_traits>

#include <ecs.hpp>

struct some_t {
	int a{};
	int b{};
};

struct ssome_t {
	ssome_t(int a, int b, int c) : a{a}, b{b}, c{c} {}

	int a, b, c;
};

int main() {
	std::cout << "view test" << "\n";

	int a[4] = {1, 2, 3, 4};
	const double b[4] = {1, 2, 3, 4};
	short c[4] = {1, 2, 3, 4};

	sparse_storage_view_t view{4, a, b, c};
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
	sparse_storage_t<int, float, some_t> storage;
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

	sparse_storage_t<int> storage_empty;
	storage_empty.emplace(0, 1);
	for (auto [handle, v] : storage_empty.view()) {

	}

	return 0;
}