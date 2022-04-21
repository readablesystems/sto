// Used for working with commutative operations

#pragma once

#include <experimental/type_traits>

#include "MVCCTypes.hh"

namespace commutators {

template <typename T>
class Commutator {
public:
    // Each type must define its own Commutator variant
    Commutator() = default;

    virtual void operate(T&) const {
        always_assert(false, "Should never operate on the default commutator.");
    }
};

//////////////////////////////////////////////
//
// Commutator type for integral +/- operations
//
//////////////////////////////////////////////

template <>
class Commutator<int64_t> {
public:
    Commutator() = default;
    explicit Commutator(int64_t delta) : delta(delta) {}

    virtual bool is_blind_write() const {
        return false;
    }

    virtual void operate(int64_t& v) const {
        v += delta;
    }

private:
    int64_t delta;
};

// Taken from https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error
class CommAdapter {
private:
    //template <typename T>
    //static constexpr bool CellSplit(decltype(&T::required_cells)) {
    //    return true;
    //}
    //template <typename T>
    //static constexpr bool CellSplit(...) {
    //    return false;
    //}

    template <typename T> using has_cell_split_t = typename Commutator<T>::supports_cellsplit;
    //template <typename T> using has_cell_split_t = decltype(&Commutator<T>::required_cells);

    //template <typename... T> using void_t = void;
    //template <typename T, typename = void>
    //struct has_cell_split_t : std::false_type {};
    //template <typename T>
    //struct has_cell_split_t<T, void_t<typename T::supports_cellsplit>> : std::true_type {};

public:
    template <typename T>
    struct Properties {
        //static constexpr bool supports_cellsplit = sizeof(CellSplit<Commutator<T>>());
        static constexpr bool supports_cellsplit =
            std::experimental::is_detected_v<has_cell_split_t, T>;
    };

    template <typename T>
    static constexpr auto required_cells(
            std::initializer_list<typename T::NamedColumn> columns, int split) {
        using accessor = typename T::RecordAccessor;

        std::array<bool, accessor::MAX_SPLITS> cells {};
        std::fill(cells.begin(), cells.end(), false);

        printf("Checking required cells with split %d\n", split);

        // Shortcut for split-0
        if (split == 0) {
            return cells;
        }

        for (auto column : columns) {
            cells[accessor::split_of(split, column)] = true;
        }
        return cells;

    }
};

}
