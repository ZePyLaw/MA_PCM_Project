
#pragma once

#include "task.hpp"
#include "lock_free_stack.hpp"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

class ParallelTaskRunner : public TaskRunner {
private:
    struct Frame {
        Task* task;
        Frame* parent;
        TaskStack children;
        std::atomic<int> remaining{0};

        Frame(Task* t, Frame* p, int capacity)
            : task(t), parent(p), children(capacity) {}
    };

    struct Job : WorkNode {
        Frame* frame;
        explicit Job(Frame* f) : frame(f) {}
    };

    int thread_count_;
    int capacity_;
    LockFreeStack queue_;
    std::atomic<std::size_t> unfinished_{0};

    // Each worker owns its list; jobs live until all threads join.
    std::vector<std::vector<std::unique_ptr<Job>>> owned_jobs_;

    void submit(Frame* frame, int worker_id) {
        auto job = std::make_unique<Job>(frame);
        Job* ptr = job.get();
        owned_jobs_[worker_id].push_back(std::move(job));
        queue_.push(ptr);
    }

    void complete(Frame* frame) {
        Frame* parent = frame->parent;
        delete frame;

        if (parent &&
            parent->remaining.fetch_sub(
                1, std::memory_order_acq_rel) == 1) {

            parent->task->merge(&parent->children);
            complete(parent);
        }

        unfinished_.fetch_sub(
            1, std::memory_order_acq_rel);
    }

    void process(Frame* frame, int worker_id) {
        int n = frame->task->split(&frame->children);

        if (n == 0) {
            frame->task->solve();
            complete(frame);
            return;
        }

        // Copy children before other workers can merge them.
        std::vector<Task*> tasks;
        tasks.reserve(n);

        for (int i = 0; i < n; ++i)
            tasks.push_back(frame->children[i]);

        frame->remaining.store(
            n, std::memory_order_relaxed);

        unfinished_.fetch_add(
            n, std::memory_order_acq_rel);

        for (Task* child : tasks) {
            Frame* child_frame =
                new Frame(child, frame, capacity_);

            submit(child_frame, worker_id);
        }
    }

    void worker(int id) {
        for (;;) {
            WorkNode* node = queue_.pop();

            if (node) {
                process(static_cast<Job*>(node)->frame, id);
            }
            else if (unfinished_.load(
                std::memory_order_acquire) == 0) {
                break;
            }
            else {
                std::this_thread::yield();
            }
        }
    }

public:
    ParallelTaskRunner(int threads, int capacity)
        : thread_count_(threads),
          capacity_(capacity),
          owned_jobs_(threads) {

        if (threads < 1 || capacity < 1)
            throw std::invalid_argument(
                "threads and capacity must be positive");

        if (!queue_.is_lock_free())
            throw std::runtime_error(
                "Atomic pointer is not lock-free");
    }

    void run(Task* root) override {
        for (auto& jobs : owned_jobs_)
            jobs.clear();

        unfinished_.store(
            1, std::memory_order_relaxed);

        submit(new Frame(root, nullptr, capacity_), 0);

        TaskRunner::startTimer();

        std::vector<std::thread> threads;
        threads.reserve(thread_count_);

        for (int i = 0; i < thread_count_; ++i)
            threads.emplace_back(
                [this, i] { worker(i); });

        for (auto& t : threads)
            t.join();

        TaskRunner::stopTimer();

        // Reclaim nodes only after every worker has stopped.
        for (auto& jobs : owned_jobs_)
            jobs.clear();
    }
};
