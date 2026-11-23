
#include "../src/parallel_task_runner.hpp"

#include <atomic>
#include <iostream>

class CounterTask final : public Task {
    int depth_;
    int fanout_;
    std::atomic<int>& solves_;
    std::atomic<int>& merges_;

public:
    CounterTask(int depth, int fanout,
                std::atomic<int>& solves,
                std::atomic<int>& merges)
        : depth_(depth),
          fanout_(fanout),
          solves_(solves),
          merges_(merges) {}

    int split(TaskCollection* collection) override {
        collection->clear();

        if (depth_ == 0)
            return 0;

        for (int i = 0; i < fanout_; ++i) {
            collection->push(new CounterTask(
                depth_ - 1, fanout_, solves_, merges_));
        }

        return fanout_;
    }

    void merge(TaskCollection* collection) override {
        while (collection->size())
            delete collection->pop();

        merges_.fetch_add(1, std::memory_order_relaxed);
    }

    void solve() override {
        solves_.fetch_add(1, std::memory_order_relaxed);
    }

    void write(std::ostream& os) const override {
        os << "CounterTask(" << depth_ << ")";
    }
};

int main() {
    constexpr int expected_solves = 2187;
    constexpr int expected_merges = 1093;

    for (int n : {1, 2, 4, 8, 16, 32, 64}) {
        for (int rep = 0; rep < 10; ++rep) {
            std::atomic<int> solves{0};
            std::atomic<int> merges{0};

            CounterTask root(7, 3, solves, merges);
            ParallelTaskRunner runner(n, 32);

            runner.run(&root);

            if (solves.load() != expected_solves ||
                merges.load() != expected_merges) {
                std::cerr << "ERROR threads=" << n << '\n';
                return 1;
            }
        }

        std::cout << "OK threads=" << n
                  << " solves=" << expected_solves
                  << " merges=" << expected_merges << '\n';
    }

    return 0;
}
