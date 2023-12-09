#pragma once

#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <condition_variable>

template<class data_t>
struct mt_queue_t {
	template<class _data_t>
	void push(_data_t&& data) {
		std::unique_lock lock_guard{lock};
		queue.push(std::forward<_data_t>(data));
		added.notify_one();
	}

	data_t pop() {
		std::unique_lock lock_guard{lock};
		if (queue.empty()) {
			added.wait(lock_guard, [&] (){
				return !queue.empty();
			});
		}
		data_t data = std::move(queue.front());
		queue.pop();
		return data;
	}

	std::mutex lock;
	std::condition_variable added;
	std::queue<data_t> queue;
};

struct job_if_t {
	job_if_t() = default;
	virtual ~job_if_t() = default;

	job_if_t(job_if_t&&) noexcept = delete;
	job_if_t& operator= (job_if_t&&) noexcept = delete;

	job_if_t(const job_if_t&) = delete;
	job_if_t& operator= (const job_if_t&) = delete;

	virtual void execute() = 0;

	void set_ready() {
		std::unique_lock lock_guard{lock};
		ready_status = true;
		ready.notify_all();
	}

	void wait() {
		std::unique_lock lock_guard{lock};
		ready.wait(lock_guard, [&] (){
			return ready_status;
		});
		ready_status = false;
	}

	std::mutex lock;
	std::condition_variable ready;
	bool ready_status{};
};

template<class func_t>
struct func_job_t : public job_if_t {
	template<class _func_t>		
	func_job_t(_func_t&& _func)
		: func{std::forward<_func_t>(_func)}
	{}

	void execute() override {
		func();
	}

	func_t func;
};

template<class func_t>
func_job_t(func_t&& func) -> func_job_t<func_t>;

// very simple thread pool
// you submit some set of jobs
// you wait for them
// you cannot drop thread jobs
// you'd better not push the same job more then once (so only one thread can execute the job)
// you are not allowed to push nullptr: it is a special value that will terminate a worker
struct thread_pool_t {
	static constexpr int thread_count_fallback = 8;

	thread_pool_t(int thread_count = 0) {
		if (thread_count <= 0) {
			thread_count = thread_count_fallback;
		}
		for (int i = 0; i < thread_count; i++) {
			workers.push_back(std::thread([&]() {
				thread_pool_worker_func();
			}));
		}
	}

	~thread_pool_t() {
		for (int i = 0; i < workers.size(); i++) {
			job_queue.push(nullptr);
		}

		std::unique_lock lock_guard{lock};
		worker_terminated.wait(lock_guard, [&] (){
			return workers_terminated == workers.size();
		});
		lock_guard.unlock();

		for (auto& worker : workers) {
			worker.join();
		}
	}

	void thread_pool_worker_func() {
		while (true) {
			job_if_t* job = job_queue.pop();
			if (!job) {
				break;
			}
			job->execute();
			job->set_ready();
		}

		std::unique_lock lock_guard{lock};
		workers_terminated++;
		worker_terminated.notify_one();
	}

	void push_job(job_if_t* job) {
		job_queue.push(job);
	}

	int worker_count() const {
		return workers.size();
	}

	std::vector<std::thread> workers;
	mt_queue_t<job_if_t*> job_queue;

	std::mutex lock;
	std::condition_variable worker_terminated;
	int workers_terminated{};
};