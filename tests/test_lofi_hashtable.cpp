#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include <lofi.hpp>
#include <utils.hpp>
#include <sparse_cell.hpp>
#include <thread_pool.hpp>


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

	lofi_test_ctx_t(int _cell_count, int _repeat, int _job_count, bool should_shuffle = false)
		: cell_count{_cell_count}
		, repeat{_repeat}
		, job_count{_job_count}
		, total_cell_count{cell_count * repeat}
		, hashtable{total_cell_count, lofi_hasher_t{this, hash}, lofi_equals_t{this, equals}}
		, used_buckets{total_cell_count}
		, thread_pool{job_count} {

		int_gen_t x_gen(561, -40000, 50000);
		int_gen_t y_gen(1442, -40000, 50000);
		int_gen_t z_gen(105001, -40000, 50000);

		cells.reserve(total_cell_count);
		for (int i = 0; i < cell_count; i++) {
			int x = x_gen.gen();
			int y = y_gen.gen();
			int z = z_gen.gen();
			for (int k = 0; k < repeat; k++) {
				cells.push_back(sparse_cell_t{x, y, z});
			}
		}

		if (should_shuffle) {
			shuffle_vec(cells, 666);
		}

		jobs.reserve(job_count);
		for (int i = 0; i < job_count; i++) {
			jobs.push_back(std::make_unique<job_t>(this, i));
		}

		std_hashtable.reserve(total_cell_count);
	}

	int master() {
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

		std::cout << " ---=== in microseconds ===---" << "\n";
		std::cout << "lofi: " << dt32 + dt21 << " = " << dt32 << " + " << dt21 << "\n";
		std::cout << "std: " << dt43 << "\n";

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

		std::cout << "max count: " << max_count << "\n";
		std::cout << "max scans: " << max_scans << "\n";
		std::cout << "total scans: " << total_scans << "\n";
		std::cout << "avg scans: " << (double)total_scans / total_cell_count << "\n";
		std::cout << "inserted: " << inserted << "\n";
		std::cout << "used buckets: " << used_buckets.get_size() << "\n";
		std::cout << "hashtable valid: " << check_lofi_hashtable() << "\n";

		return dt32 + dt21;
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

		return added_count == total_cell_count;
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
				if (job->job_id == -1) {
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

	std::vector<sparse_cell_t> cells{};

	lofi_hashtable2_t hashtable;
	lofi_stack_t<int> used_buckets;

	thread_pool_t thread_pool;
	std::vector<std::unique_ptr<job_t>> jobs;
	process_stage_t stage{};

	std_hashtable_t std_hashtable;
};

void test_lofi_hashtable() {
	lofi_test_ctx_t ctx{1 << 17, 1 << 4, 24};

	std::vector<int> dts;
	for (int i = 0; i < 100; i++) {
		dts.push_back(ctx.master());
	}
	for (auto& dt : dts) {
		std::cout << dt << " ";
	}
	std::cout << "\n";
}

int main() {
	return 0;
}