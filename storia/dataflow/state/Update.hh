#pragma once

#include "Commutators.hh"

#include "dataflow/Logics.hh"

namespace storia {

// Update type
enum class UpdateType {
    BlindWrite = 0,
    ReadWrite,
    PartialBlindWrite,
    PartialReadWrite,
    Delete,
};

class Update {
public:
    typedef UpdateType Type;

protected:
    Update() {}
};

template <typename T>
class BlindWrite : public Update {
public:
    static constexpr Type TYPE = Type::BlindWrite;

    BlindWrite(const T& value, bool overwrite=true)
        : overwrite_(overwrite), value_(value) {}

    template <typename K>
    K key() const {
        return value_.key();
    }

    bool overwrite() const {
        return overwrite_;
    }

    const T& value() const {
        return value_;
    }

    T* vp() {
        return &value_;
    }

private:
    bool overwrite_;
    T value_;
};

template <typename T>
class ReadWrite : public Update {
public:
    typedef Predicate<T, void, true> pred_type;
    static constexpr Type TYPE = Type::ReadWrite;

    ReadWrite(const pred_type& pred, const T& value, bool overwrite=true)
        : overwrite_(overwrite), pred_(pred), value_(value) {}

    template <typename K>
    K key() const {
        return value_.key();
    }

    bool overwrite() const {
        return overwrite_;
    }

    const pred_type& predicate() const {
        return pred_;
    }

    const T& value() const {
        return value_;
    }

    T* vp() {
        return &value_;
    }

private:
    bool overwrite_;
    const pred_type pred_;
    T value_;
};

template <typename Key, typename T>
class PartialBlindWrite : public Update {
public:
    static constexpr Type TYPE = Type::PartialBlindWrite;

private:
    typedef ::commutators::Commutator<T> comm_base_type;

public:
    typedef std::function<T&(T&)> func_type;
    class Updater : public comm_base_type {
    public:
        Updater() = default;
        explicit Updater(const func_type& function) : func_(function) {}

        T& operate(T& v) const override {
            return func_(v);
        }

    private:
        const func_type func_;
    };

    PartialBlindWrite(const Key& key, const func_type& update_func)
        : key_(key), updater_(update_func) {}

    template <typename K>
    K key() const {
        return key_;
    }

    const Updater& updater() const {
        return updater_;
    }

private:
    const Key key_;
    const Updater updater_;
};

template <typename Key, typename T>
class PartialReadWrite : public Update {
public:
    typedef Predicate<T, void, true> pred_type;
    static constexpr Type TYPE = Type::PartialReadWrite;

private:
    typedef ::commutators::Commutator<T> comm_base_type;

public:
    typedef std::function<T&(T&)> func_type;
    class Updater : public comm_base_type {
    public:
        Updater() = default;
        explicit Updater(const func_type& function) : func_(function) {}

        T& operate(T& v) const override {
            return func_(v);
        }

    private:
        const func_type func_;
    };

    PartialReadWrite(
        const Key& key, const pred_type& pred, const func_type& update_func)
        : key_(key), pred_(pred), updater_(update_func) {}

    template <typename K>
    K key() const {
        return key_;
    }

    const pred_type& predicate() const {
        return pred_;
    }

    const Updater& updater() const {
        return updater_;
    }

private:
    const Key key_;
    const pred_type pred_;
    const Updater updater_;
};

};  // namespace storia
