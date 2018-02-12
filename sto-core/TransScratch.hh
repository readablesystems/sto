#pragma once

#include <cstdlib>
#include <cstring>
#include <cassert>

class TransScratch {
public:
    static constexpr size_t initial_zone_capacity = 4096; // 4KB
    static constexpr size_t max_scratch_restart_capacity = 2097152; // 2MB

    TransScratch()
            : tail_next_avail(0), tail_capacity(0),
              total_capacity(0), zone_head(nullptr), zone_tail(nullptr) {}

    explicit TransScratch(size_t init_size)
            : tail_next_avail(0), tail_capacity(init_size),
              total_capacity(init_size) {
        auto z = new char [sizeof(zone_hdr) + init_size];
        new (reinterpret_cast<zone_hdr *>(z)) zone_hdr(init_size);
        zone_head = zone_tail = reinterpret_cast<zone_hdr *>(z);
    }

    template <typename T>
    inline T& allocate();

    template <typename T>
    inline T& clone(const T& other);

    inline void clear();

private:
    struct zone_hdr {
        zone_hdr() : length(0), next(nullptr) {}
        explicit zone_hdr(size_t zone_size) : length(zone_size), next(nullptr) {}
        char *in_zone(off_t offset) {
            return (reinterpret_cast<char *>(this + 1) + offset);
        }
        size_t length;
        zone_hdr *next;
    };

protected:
    size_t tail_next_avail;
    size_t tail_capacity;
    size_t total_capacity;
    zone_hdr *zone_head;
    zone_hdr *zone_tail;
};

template <typename T>
T& TransScratch::allocate() {
    char* ptr = nullptr;
    size_t new_next_avail = tail_next_avail + sizeof(T);
    if (new_next_avail >= tail_capacity) {
        // allocate a new zone
        size_t new_capacity = (tail_capacity > 0) ? (tail_capacity * 2) : initial_zone_capacity;
        assert(new_capacity > sizeof(T));

        auto z = new char [sizeof(zone_hdr) + new_capacity];
        auto new_zone = reinterpret_cast<zone_hdr *>(z);
        new (new_zone) zone_hdr(new_capacity);

        if (zone_head == nullptr) {
            assert(zone_tail == nullptr);
            zone_head = new_zone;
        }

        if (new_next_avail > tail_capacity) {
            // allocate from the new zone
            ptr = new_zone->in_zone(0);
            tail_next_avail = sizeof(T);
        } else {
            // eat up the last chunk remaining in the old zone
            ptr = zone_tail->in_zone(tail_next_avail);
            tail_next_avail = 0;
        }

        tail_capacity = new_capacity;
        total_capacity += new_capacity;
        if (zone_tail != nullptr)
            zone_tail->next = new_zone;
        zone_tail = new_zone;
    } else {
        ptr = zone_tail->in_zone(tail_next_avail);
        tail_next_avail = new_next_avail;
    }
    return *reinterpret_cast<T *>(ptr);
}

template <typename T>
T& TransScratch::clone(const T &other) {
    T *obj = &allocate<T>();
    memcpy(obj, &other, sizeof(T));
    return *obj;
}

void TransScratch::clear() {
    if (zone_head == zone_tail) {
        // there is only one segment, reuse that zone
        assert(total_capacity == tail_capacity);
        tail_next_avail = 0;
        return;
    }

    zone_hdr *curr = zone_head;
    size_t capacity_check = 0;
    while (curr != nullptr) {
        auto next_zone = curr->next;
        capacity_check += curr->length;
        delete[] curr;
        curr = next_zone;
    }
    assert(capacity_check == total_capacity);

    // limit the size of the consolidated initial zone
    if (total_capacity > max_scratch_restart_capacity)
        total_capacity = max_scratch_restart_capacity;

    auto z = new char [sizeof(zone_hdr) + total_capacity];
    auto single_zone = reinterpret_cast<zone_hdr *>(z);
    new (single_zone) zone_hdr(total_capacity);

    tail_next_avail = 0;
    tail_capacity = total_capacity;
    zone_head = zone_tail = single_zone;
}

