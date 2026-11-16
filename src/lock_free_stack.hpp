
#pragma once

#include <atomic>

// Un élément stocké dans notre pile.
struct WorkNode {
    int id = -1;
    WorkNode* next = nullptr;
};

class LockFreeStack {
private:
    // Sommet de la pile, partagé entre les threads.
    std::atomic<WorkNode*> head_{nullptr};

public:
    bool is_lock_free() const noexcept {
        return head_.is_lock_free();
    }

    // Ajouter un élément au sommet.
    void push(WorkNode* node) noexcept {
        WorkNode* old = head_.load(std::memory_order_relaxed);

        do {
            node->next = old;
        } while (!head_.compare_exchange_weak(
            old, node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    // Retirer un élément du sommet.
    // Retourne nullptr si la pile est vide.
    WorkNode* pop() noexcept {
        WorkNode* old = head_.load(std::memory_order_acquire);

        while (old && !head_.compare_exchange_weak(
            old, old->next,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        }

        return old;
    }
};
