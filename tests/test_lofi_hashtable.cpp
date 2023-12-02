#include <chrono>
#include <string>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include <lofi.hpp>
#include <utils.hpp>
#include <sparse_cell.hpp>
#include <thread_pool.hpp>

#include <nlohmann/json.hpp>

namespace nlj = nlohmann;

using json = nlj::ordered_json;

struct lofi_test_settings_t {
	int cell_count{};
	int repeat{};
	int job_count{};

	int x_seed{};
	int x_min{};
	int x_max{};

	int y_seed{};
	int y_min{};
	int y_max{};

	int z_seed{};
	int z_min{};
	int z_max{};

	int shuffle_seed{};
	bool should_shuffle{};

	bool should_test_std{};
 };

template<class hashtable_t>
struct lofi_test_ctx_t {
	using std_hashtable_t = std::unordered_multiset<sparse_cell_t, sparse_cell_hasher_t, sparse_cell_equals_t>;

	struct job_t : public job_if_t {
		job_t(lofi_test_ctx_t* _ctx, int _job_id)
			: ctx{_ctx}
			, job_id{_job_id}
		{}

		void execute() override {
			ctx->worker(this);
		}

		lofi_test_ctx_t* ctx{};
		int job_id{};
		int max_scans{};
		int total_scans{};
	};

	enum process_stage_t {
		HashtableReset,
		PrepareHashtable,
		BuildHashtable,
		BuildStdHashtable,
	};

	static std::uint32_t hash(lofi_test_ctx_t* ctx, int cell) {
		return sparse_cell_hasher_t{}(ctx->cells[cell]);
	}

	static bool equals(lofi_test_ctx_t* ctx, int cell1, int cell2) {
		return ctx->cells[cell1] == ctx->cells[cell2];
	}

	lofi_test_ctx_t(const lofi_test_settings_t& settings)
		: cell_count{settings.cell_count}
		, repeat{settings.repeat}
		, job_count{settings.job_count}
		, total_cell_count{cell_count * repeat}
		, hashtable{total_cell_count, lofi_hasher_t{this, hash}, lofi_equals_t{this, equals}}
		, used_buckets{total_cell_count}
		, thread_pool{job_count}
		, should_test_std{settings.should_test_std} {

		int_gen_t x_gen(settings.x_seed, settings.x_min, settings.x_max);
		int_gen_t y_gen(settings.y_seed, settings.y_min, settings.y_max);
		int_gen_t z_gen(settings.z_seed, settings.z_min, settings.z_max);

		cells.reserve(total_cell_count);
		for (int i = 0; i < cell_count; i++) {
			int x = x_gen.gen();
			int y = y_gen.gen();
			int z = z_gen.gen();
			for (int k = 0; k < repeat; k++) {
				cells.push_back(sparse_cell_t{x, y, z});
			}
		}

		if (settings.should_shuffle) {
			shuffle_vec(cells, settings.shuffle_seed);
		}

		jobs.reserve(job_count);
		for (int i = 0; i < job_count; i++) {
			jobs.push_back(std::make_unique<job_t>(this, i));
		}

		if (should_test_std) {
			std_hashtable.reserve(total_cell_count);
		}
	}

	json master() {
		if (should_test_std) {
			std_hashtable.clear();
		}

		auto t1 = std::chrono::high_resolution_clock::now();

		hashtable.reset_size(total_cell_count);
		used_buckets.reset();

		stage = process_stage_t::PrepareHashtable;
		dispatch_jobs();

		auto t2 = std::chrono::high_resolution_clock::now();

		stage = process_stage_t::BuildHashtable;
		dispatch_jobs();

		auto t3 = std::chrono::high_resolution_clock::now();

		stage = process_stage_t::BuildStdHashtable;
		dispatch_jobs();

		auto t4 = std::chrono::high_resolution_clock::now();


		using microseconds_t = std::chrono::duration<long long, std::micro>;

		auto dt21 = std::chrono::duration_cast<microseconds_t>(t2 - t1).count();
		auto dt32 = std::chrono::duration_cast<microseconds_t>(t3 - t2).count();
		auto dt43 = std::chrono::duration_cast<microseconds_t>(t4 - t3).count();

		int max_count = 0;
		int inserted = 0;
		for (int i = 0; i < hashtable.get_buckets_count(); i++) {
			int count = hashtable.get_bucket(i).count.load(std::memory_order_relaxed);
			max_count = std::max(max_count, count);
			inserted += count;
		}

		int max_scans = 0;
		int total_scans = 0;
		for (auto& job : jobs) {
			max_scans = std::max(max_scans, job->max_scans);
			total_scans += job->total_scans;
		}

		double avg_scans = (double)total_scans / total_cell_count;
		bool hashtable_valid = check_lofi_hashtable();

		json stats = json::object({
			{"std_build", dt43},
			{"lofi_prepare", dt21},
			{"lofi_build", dt32},
			{"lofi_total", dt21 + dt32}, // unnecessary but human readable
			{"max_count_in_bucket", max_count},
			{"max_insertion_scans", max_scans},
			{"total_scans", total_scans},
			{"avg_scans", avg_scans},
			{"total_inserted", inserted},
			{"used_buckets", used_buckets.get_size()},
			{"hashtable_valid", hashtable_valid},
		});

		return stats;
	}

	bool check_lofi_hashtable() {
		int added_count = 0;
		std::vector<bool> cell_added(total_cell_count);
		for (int i = 0, count = used_buckets.get_size(); i < count; i++) {
			auto& bucket = hashtable.get_bucket(used_buckets.data[i]);

			int curr = bucket.head.load(std::memory_order_relaxed);
			if (curr == -1) {
				return false;
			}
			auto cell = cells[curr];
			while (true) {
				if (cell_added[curr] || cell != cells[curr]) {
					return false;
				}
				cell_added[curr] = true;
				added_count++;

				int next = hashtable.list_entries[curr].next;
				if (next == curr) {
					break;
				}
				curr = next;
			}
		}

		if (added_count != total_cell_count) {
			return false;
		}

		std::unordered_set<int> buckets;
		int used_buckets_count = used_buckets.size.load(std::memory_order_relaxed);
		for (int i = 0; i < used_buckets_count; i++) {
			auto [it, inserted] = buckets.insert(used_buckets.data[i]);
			if (!inserted) {
				return false;
			}
		}

		return true;
	}

	void worker(job_t* job) {
		switch (stage) {
			// prepare
			case PrepareHashtable: {
				auto [start, stop] = compute_data_range(hashtable.get_buckets_data(), hashtable.get_buckets_count(), job_count, job->job_id);
				while (start != stop) {
					start->reset();
					start++;
				}
				break;
			}

			// put
			case BuildHashtable: {
				constexpr int batch_size = 128;

				int buckets[batch_size] = {};
				int buckets_count = 0;
				int max_scans = 0;
				int total_scans = 0;

				auto [start, stop] = compute_job_range(hashtable.get_object_count(), job_count, job->job_id);
				while (start != stop) {
					buckets_count = 0;

					int next_stop = std::min(stop, start + batch_size);
					while (start != next_stop) {
						auto [bucket, scans] = hashtable.put(start);
						if (bucket != -1) {
							buckets[buckets_count++] = bucket;
						}
						max_scans = std::max(max_scans, scans);
						total_scans += scans;
						start++;
					}

					if (buckets_count > 0) {
						int* used_buckets_mem = used_buckets.push(buckets_count);
						assert(used_buckets_mem);
						std::memcpy(used_buckets_mem, buckets, sizeof(buckets[0]) * buckets_count);
					}
				}

				job->max_scans = max_scans;
				job->total_scans = total_scans;
				break;
			}

			// std
			case BuildStdHashtable: {
				if (should_test_std && job->job_id == 0) {
					auto [start, stop] = compute_job_range(hashtable.get_object_count(), 1, 0);
					while (start != stop) {
						std_hashtable.insert(cells[start++]);
					}
				}
				break;
			}
		}
	}

	void dispatch_jobs() {
		for (auto& job : jobs) {
			thread_pool.push_job(job.get());
		}
		for (auto& job : jobs) {
			job->wait();
		}
	}

	int cell_count{};
	int repeat{};
	int job_count{};
	int total_cell_count{};

	bool should_test_std{};

	std::vector<sparse_cell_t> cells{};

	hashtable_t hashtable;
	lofi_stack_t<int> used_buckets;

	thread_pool_t thread_pool;
	std::vector<std::unique_ptr<job_t>> jobs;
	process_stage_t stage{};

	std_hashtable_t std_hashtable;
};

inline constexpr const int test_invocations = 32;

void test_lofi_hashtable() {
	const std::string basic_test_name = "some_basic_case";

	lofi_test_settings_t settings{
		.cell_count = 1 << 16,
		.repeat = 1 << 4,
		.job_count = 24,

		.x_seed = 41,
		.x_min = -10000,
		.x_max = +10000,

		.y_seed = 42,
		.y_min = -10000,
		.y_max = +10000,

		.z_seed = 43,
		.z_min = -10000,
		.z_max = +10000,

		.shuffle_seed = 123,
		.should_shuffle = true,

		.should_test_std = false,
	};

	json basic_stats = json::object({
		{"cell_count", settings.cell_count},
		{"repeat", settings.repeat},
		{"total_cells", settings.cell_count * settings.repeat}, // not neccessary but human readable
		{"job_count", settings.job_count},
		{"x_seed_min_max", {settings.x_seed, settings.x_min, settings.x_max}},
		{"y_seed_min_max", {settings.y_seed, settings.y_min, settings.y_max}},
		{"z_seed_min_max", {settings.z_seed, settings.z_min, settings.z_max}},
		{"shuffle_seed", settings.shuffle_seed},
		{"should_shuffle", settings.should_shuffle},
		{"should_test_std", settings.should_test_std},
		{"stats", json::array()},
	});

	json stats1 = basic_stats;
	lofi_test_ctx_t<lofi_hashtable1_t> ctx1{settings};
	for (int i = 0; i < test_invocations; i++) {
		stats1["stats"].push_back(ctx1.master());
	}
	{
		std::ofstream ofs(basic_test_name + "_v1.json");
		ofs << std::setw(4) << stats1;
	}

	json stats2 = basic_stats;
	lofi_test_ctx_t<lofi_hashtable2_t> ctx2{settings};
	for (int i = 0; i < test_invocations; i++) {
		stats2["stats"].push_back(ctx2.master());
	}
	{
		std::ofstream ofs(basic_test_name + "_v2.json");
		ofs << std::setw(4) << stats2 << "\n";
	}
}

int main() {
	test_lofi_hashtable();
	return 0;
}