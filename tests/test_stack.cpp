
#include "../src/lock_free_stack.hpp"

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    constexpr int N = 100000;
    constexpr int WORKERS = 4;

    LockFreeStack stack;
    std::vector<WorkNode> nodes(N);
    std::vector<std::atomic<int>> seen(N);

    for (int i = 0; i < N; ++i) {
        nodes[i].id = i;
        seen[i].store(0);
    }

    std::atomic<bool> done{false};

    // Threads consommateurs : retirer les tâches.
    std::vector<std::thread> consumers;

    for (int w = 0; w < WORKERS; ++w) {
        consumers.emplace_back([&] {
            while (true) {
                WorkNode* node = stack.pop();

                if (node != nullptr) {
                    seen[node->id].fetch_add(
                        1, std::memory_order_relaxed);
                }
                else if (done.load(std::memory_order_acquire)) {
                    break;
                }
                else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Threads producteurs : ajouter les tâches.
    std::vector<std::thread> producers;

    for (int w = 0; w < WORKERS; ++w) {
        producers.emplace_back([&, w] {
            for (int i = w; i < N; i += WORKERS) {
                stack.push(&nodes[i]);
            }
        });
    }

    // Attendre que tous les producteurs terminent.
    for (auto& t : producers)
        t.join();

    done.store(true, std::memory_order_release);

    // Attendre que tous les consommateurs terminent.
    for (auto& t : consumers)
        t.join();

    // Vérifier qu'aucune tâche n'est perdue ou dupliquée.
    int errors = 0;

    for (int i = 0; i < N; ++i) {
        if (seen[i].load(std::memory_order_relaxed) != 1)
            ++errors;
    }

    std::cout << "Pointer atomics lock-free: "
              << std::boolalpha << stack.is_lock_free() << '\n';

    std::cout << (errors == 0 ? "OK: " : "ERROR: ")
              << N - errors << "/" << N
              << " tasks processed exactly once\n";

    return errors == 0 && stack.is_lock_free() ? 0 : 1;
}
