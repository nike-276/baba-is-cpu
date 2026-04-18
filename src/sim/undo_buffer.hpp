#pragma once

#include "core/change.hpp"

#include <deque>
#include <optional>
#include <vector>

namespace baba::sim {

// Ring buffer of per-tick Change lists, capped at `cap` entries.
// push() adds the newest tick; pop() removes and returns the most recent.
// When the buffer is at capacity, push() silently drops the oldest entry.
class UndoBuffer {
public:
    explicit UndoBuffer(std::size_t cap = 10'000) : cap_{cap} {}

    void push(std::vector<core::Change> changes) {
        if (changes.empty()) return;  // no-op ticks don't consume undo slots
        if (ring_.size() >= cap_) ring_.pop_back();
        ring_.push_front(std::move(changes));
    }

    std::optional<std::vector<core::Change>> pop() {
        if (ring_.empty()) return std::nullopt;
        auto v = std::move(ring_.front());
        ring_.pop_front();
        return v;
    }

    void clear() { ring_.clear(); }
    bool empty() const { return ring_.empty(); }
    std::size_t size() const { return ring_.size(); }

private:
    std::deque<std::vector<core::Change>> ring_;
    std::size_t cap_;
};

}  // namespace baba::sim
