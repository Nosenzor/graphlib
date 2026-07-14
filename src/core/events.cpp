#include "graphlib/events.hpp"

#include <algorithm>

namespace graphlib {

int EventRegistry::connect(std::string_view name, EventCallback callback) {
    const int cid = next_cid_++;
    entries_.push_back({cid, std::string(name), std::move(callback)});
    return cid;
}

void EventRegistry::disconnect(int cid) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [cid](const Entry& e) { return e.cid == cid; }),
                   entries_.end());
}

void EventRegistry::emit(const Event& event) const {
    // Copy the matching callbacks first: a handler may connect or disconnect
    // (mutating entries_) while running — mpl allows this too.
    std::vector<EventCallback> matching;
    for (const Entry& e : entries_) {
        if (e.name == event.name) {
            matching.push_back(e.callback);
        }
    }
    for (const EventCallback& callback : matching) {
        callback(event);
    }
}

} // namespace graphlib
