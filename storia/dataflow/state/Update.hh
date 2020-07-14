#pragma once

#include "Commutators.hh"

#include "dataflow/Logics.hh"

namespace storia {

namespace state {

// Update type
enum UpdateType {
    BlindWrite = 0,
    DeltaWrite,
    PartialBlindWrite,
    PartialDeltaWrite,
    Delete,
};

class Update {
public:
    typedef UpdateType Type;

protected:
    Update() {}
};

template <typename T>
class BlindWriteUpdate : public Update {
public:
    static constexpr Type TYPE = BlindWrite;

    BlindWriteUpdate(const T& value) : value_(value) {}

    template <typename K>
    K key() const {
        return value_.key();
    }

    const T& value() const {
        return value_;
    }

    T* vp() {
        return &value_;
    }

private:
    T value_;
};

template <typename K, typename T>
class DeltaWriteUpdate : public Update {
public:
    static constexpr Type TYPE = DeltaWrite;

private:
    typedef ::commutators::Commutator<T> comm_base_type;

public:
    typedef std::function<T&(T&)> func_type;
    class Updater : public comm_base_type {
    public:
        Updater() = default;
        explicit Updater(const func_type& function) : func_(function) {}

        T& operate(T& v) const {
            return func_(v);
        }

    private:
        const func_type func_;
    };

    DeltaWriteUpdate(const K& key, const func_type& update_func)
        : key_(key), updater_(update_func) {}

    K key() const {
        return key_;
    }

    const Updater& updater() const {
        return updater_;
    }

private:
    const K key_;
    const Updater updater_;
};

};  // namespace state

};  // namespace storia
