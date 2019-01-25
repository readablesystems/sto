// Forward-declared MVCC types and common includes

#pragma once

#include <atomic>
#include <type_traits>

#include "Interface.hh"
#include "TransItem.hh"
#include "VersionBase.hh"

namespace commutators {

// For handling commutative updates
template <typename T> class MvCommutator;

}

// Base class for MvHistory
class MvHistoryBase;

// History list item for MVCC
template <typename T> class MvHistory;

// Generic contained for MVCC abstractions applied to a given object
template <typename T> class MvObject;

// Registry for keeping track of MVCC objects
class MvRegistry;
