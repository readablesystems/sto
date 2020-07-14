#pragma once

namespace storia {

namespace state {

class Entry {
public:
    template <typename K>
    K key() const;
};

};  // namespace state

};  // namespace storia
