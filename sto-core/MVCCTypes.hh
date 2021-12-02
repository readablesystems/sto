// Forward-declared MVCC types and common includes

#pragma once

#include <atomic>
#include <type_traits>

#include "Interface.hh"
#include "TransItem.hh"
#include "VersionBase.hh"
#include "Commutators.hh"

// Base class for MvHistory
class MvHistoryBase;

// History list item for MVCC
template <typename T, size_t Cells=1> class MvHistory;

// Generic contained for MVCC abstractions applied to a given object
template <typename T, size_t Cells=1> class MvObject;
#ifndef MVCC_INLINING
#define MVCC_INLINING 0
#endif
