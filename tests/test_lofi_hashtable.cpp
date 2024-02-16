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

	bool should_check_hashtable{};
 };

struct lofi_test_ctx_t {
	struct job_t : public job_if_t {
		job_t(lofi_test_ctx_t* _ctx, int _job_id)
			: ctx{_ctx}
			, job_id{_job_id}
		{}

		void execute() override {
			ctx->worker(this);
		}

		void reset_track_state() {
			max_insertion_scans = 0;
			total_insertion_scans = 0;
			max_lookup_scans = 0;
			total_lookup_scans = 0;
			all_lookups_passed = true;
		}

		lofi_test_ctx_t* ctx{};
		int job_id{};
		int max_insertion_scans{};
		int total_insertion_scans{};
		int max_lookup_scans{};
		int total_lookup_scans{};
		bool all_lookups_passed{};
	};

	enum process_stage_t {
		BuildHashtable,
		DoLookups,
	};

	struct hash_ops_t {
		uint32_t hash(uint32_t cell_id) const {
			return sparse_cell_hasher_t{}(ctx->cells[cell_id]);
		}

		uint32_t hash(const sparse_cell_t& cell) const {
			return sparse_cell_hasher_t{}(cell);
		}

		bool equals(uint32_t cell_id1, uint32_t cell_id2) const {
			return ctx->cells[cell_id1] == ctx->cells[cell_id2];
		}

		bool equals(uint32_t cell_id, const sparse_cell_t& cell) const {
			return ctx->cells[cell_id] == cell;
		}

		lofi_test_ctx_t* ctx{};
	};

	lofi_test_ctx_t(const lofi_test_settings_t& settings)
		: cell_count{settings.cell_count}
		, repeat{settings.repeat}
		, job_count{settings.job_count}
		, total_cell_count{cell_count * repeat}
		, total_bucket_count{nextpow2(total_cell_count) * 2}
		, thread_pool{job_count}
		, should_check_hashtable{settings.should_check_hashtable} {
		assert(total_cell_count <= lofi_max_buckets / 2);

		int_gen_t x_gen(settings.x_seed, settings.x_min, settings.x_max);
		int_gen_t y_gen(settings.y_seed, settings.y_min, settings.y_max);
		int_gen_t z_gen(settings.z_seed, settings.z_min, settings.z_max);

		cells = std::make_unique<sparse_cell_t[]>(total_cell_count);
		for (int i = 0; i < cell_count; i++) {
			int x = x_gen.gen();
			int y = y_gen.gen();
			int z = z_gen.gen();
			for (int k = 0; k < repeat; k++) {
				cells[k * cell_count + i] = sparse_cell_t{x, y, z};
			}
		}

		if (settings.should_shuffle) {
			shuffle_vec(cells.get(), total_cell_count, settings.shuffle_seed);
		}

		next_cell = std::make_unique<uint32_t[]>(total_cell_count);

		used_buckets_buffer = std::make_unique<uint32_t[]>(total_cell_count);

		bucket_buffer = std::make_unique<lofi_bucket_t[]>(total_bucket_count);
		std::memset(bucket_buffer.get(), 0x00, sizeof(lofi_bucket_t) * total_bucket_count);

		jobs.reserve(job_count);
		for (int i = 0; i < job_count; i++) {
			jobs.push_back(std::make_unique<job_t>(this, i));
		}
	}

	json update() {
		using microseconds_t = std::chrono::duration<long long, std::micro>;

		auto now = [] () {
			return std::chrono::high_resolution_clock::now();
		};

		auto to_microsecs = [] (auto time_point){
			return std::chrono::duration_cast<microseconds_t>(time_point).count();
		};

		auto reset_t0 = now();
		std::memset(bucket_buffer.get(), 0x00, total_bucket_count * sizeof(lofi_bucket_t));
		auto reset_t1 = now();
		auto reset_dt = to_microsecs(reset_t1 - reset_t0);

		for (auto& job : jobs) {
			job->reset_track_state();
		}

		hashtable.reset(bucket_buffer.get(), total_bucket_count, next_cell.get(), total_cell_count);
		used_buckets.reset(used_buckets_buffer.get(), total_cell_count);

		auto t1 = now();
		dispatch_jobs(BuildHashtable);
		auto t2 = now();
		dispatch_jobs(DoLookups);
		auto t3 = now();

		auto dt21 = to_microsecs(t2 - t1);
		auto dt32 = to_microsecs(t3 - t2);
		
		int total_buckets = used_buckets.allocated();
		int max_count = 0;

		auto used_buckets_view = used_buckets.view(0, total_buckets);
		for (int i = 0; i < total_buckets; i++) {
			int count = hashtable.get_bucket(used_buckets_view[i]).count;
			max_count = std::max(max_count, count);
		}

		int max_insertion_scans = 0;
		int total_insertion_scans = 0;
		int max_lookup_scans = 0;
		int total_lookup_scans = 0;
		bool all_lookups_passed = true;
		for (auto& job : jobs) {
			max_insertion_scans = std::max(max_insertion_scans, job->max_insertion_scans);
			total_insertion_scans += job->total_insertion_scans;
			max_lookup_scans = std::max(max_lookup_scans, job->max_lookup_scans);
			total_lookup_scans += job->total_lookup_scans;
			all_lookups_passed &= job->all_lookups_passed;
		}

		double avg_insertion_scans = (double)total_insertion_scans / total_cell_count;
		double avg_lookup_scans = (double)total_lookup_scans / total_cell_count;

		const char* check_status = "unchecked";
		if (should_check_hashtable) {
			check_status = check_lofi_hashtable() ? "valid" : "invalid";
		}

		json stats = json::object({
			{"lofi_reset", reset_dt},
			{"lofi_build", dt21},
			{"lofi_lookup", dt32},

			{"max_insertion_scans", max_insertion_scans},
			{"total_insertion_scans", total_insertion_scans},
			{"avg_insertion_scans", avg_insertion_scans},

			{"max_lookup_scans", max_lookup_scans},
			{"total_lookup_scans", total_lookup_scans},
			{"avg_lookup_scans", avg_lookup_scans},
			{"all_lookups_passed", all_lookups_passed},

			{"max_count_in_bucket", max_count},
			{"used_buckets", total_buckets},
			{"hashtable_check", check_status},
		});

		return stats;
	}

	bool check_lofi_hashtable() {
		std::vector<bool> cell_added(total_cell_count);
		std::unordered_set<uint32_t> bucket_inserted;

		int added = 0;
		int total_buckets = used_buckets.allocated();
		auto used_buckets_view = used_buckets.view_allocated();
		for (int i = 0; i < total_buckets; i++) {
			lofi_bucket_t bucket = hashtable.get_bucket(used_buckets_view[i]);
	
			if (!bucket.used()) {
				return false;
			}

			auto [it, inserted] = bucket_inserted.insert(used_buckets_view[i]);
			if (!inserted) {
				return false;
			}

			auto cell = cells[bucket.head()];
			for (auto it = hashtable.iter(bucket.head()); it.valid(); it.next()) {
				uint32_t item = it.get();
				if (cell_added[item] || cell != cells[item]) {
					return false;
				}
				cell_added[item] = true;
				added++;
			}
		}

		return added == total_cell_count;
	}

	void worker(job_t* job) {
		switch (stage) {
			case BuildHashtable: {
				build_hashtable(job);
				break;
			}

			case DoLookups: {
				do_lookups(job);
				break;
			}
		}
	}

	struct bucket_batch_t {
		static constexpr int capacity = 256;

		bool push(uint32_t item) {
			if (_count < capacity) {
				_data[_count++] = item;
				return true;
			}
			return false;
		}

		void reset() {
			_count = 0;
		}

		int byte_size() const {
			return sizeof(uint32_t) * _count;
		}

		int count() const {
			return _count;
		}

		const uint32_t* data() const {
			return _data;
		}

		bool empty() const {
			return _count == 0;
		}

		int _count{};
		uint32_t _data[capacity] = {};
	};

	void build_hashtable(job_t* job) {
		bucket_batch_t batch{};

		auto flush_batch = [&] () {
			auto view = used_buckets.allocate(batch.count());
			assert(view.valid());
			std::memcpy(view.data(), batch.data(), batch.byte_size());
			batch.reset();
		};

		auto [start, stop] = compute_job_range(total_cell_count, jobs.size(), job->job_id);
		for (int item = start; item < stop; item++) {
			lofi_insertion_t insertion = hashtable.put(item, hash_ops_t{this});
			assert(insertion.inserted());

			int scans = insertion.scans() + 1;
			job->max_insertion_scans = std::max(job->max_insertion_scans, scans);
			job->total_insertion_scans += scans;

			if (insertion.new_bucket()) {
				uint32_t bucket = insertion.bucket();
				if (batch.push(bucket)) {
					continue;
				}
				flush_batch();
				batch.push(bucket); // guaranteed to succeed
			}
		}
		if (!batch.empty()) {
			flush_batch();
		}
	}

	void do_lookups(job_t* job) {
		auto [start, stop] = compute_job_range(total_cell_count, job_count, job->job_id);
		for (int i = start; i < stop; i++) {
			lofi_search_result_t result = hashtable.get(cells[i], hash_ops_t{this});
			if (!result.valid()) {
				job->all_lookups_passed = false;
				return;
			}
			int scans = result.scans + 1;
			job->max_lookup_scans = std::max(job->max_lookup_scans, scans);
			job->total_lookup_scans += scans;
		}
	}

	void dispatch_jobs(process_stage_t _stage) {
		stage = _stage;
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
	int total_bucket_count{};

	bool should_test_std{};
	bool should_check_hashtable{};

	std::unique_ptr<sparse_cell_t[]> cells{};
	std::unique_ptr<uint32_t[]> next_cell{};
	std::unique_ptr<lofi_bucket_t[]> bucket_buffer{};
	std::unique_ptr<uint32_t[]> used_buckets_buffer{};

	lofi_hashtable_t hashtable{};
	lofi_stack_alloc_t<uint32_t> used_buckets{};

	thread_pool_t thread_pool;
	std::vector<std::unique_ptr<job_t>> jobs;
	process_stage_t stage{};
};

inline constexpr const int test_invocations = 128;

void test_lofi_hashtable() {
	const std::string basic_test_name = "some_basic_case";

	lofi_test_settings_t settings{
		.cell_count = 1 << 14, // !!! total_cell_count <= 1 << 20, implementation limit !!!
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
		.should_shuffle = false,

		.should_check_hashtable = true
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
		{"stats", json::array()},
	});

	json stats = basic_stats;
	lofi_test_ctx_t ctx{settings};
	for (int i = 0; i < test_invocations; i++) {
		stats["stats"].push_back(ctx.update());
	}

	std::ofstream ofs(basic_test_name + ".json");
	ofs << std::setw(4) << stats;
}

int main() {
	test_lofi_hashtable();
	return 0;
}