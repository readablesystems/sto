#pragma once

#include "Hashtable.hh"
#include "Sto.hh"

#include "StateEntry.hh"
#include "StateUpdate.hh"

namespace storia {

template <typename K, typename V>
class StateTable {
public:
    typedef K key_type;
    typedef V value_type;

    typedef StateUpdate<V> update_type;

private:
    class table_params_ : public Hashtable_params<K, value_type> {};
    typedef Hashtable<table_params_> table_type;

public:

    explicit StateTable() {
        static_assert(
            std::is_base_of_v<StateEntry, V>,
            "Value type must derive from storia::StateEntry.");
    }

    void apply(const update_type& update) {
        switch (update.type) {
            case update_type::Type::BlindWrite: {
                TRANSACTION {
                    const key_type key = update.value.key();
                    // Need to manually downcast const out of pointer
                    auto result = table_.transPut(
                            key, (value_type*)&update.value, true);
                    CHK(!result.abort);
                } RETRY(true);
                break;
            }
            default:
                break;
        }
    }

    std::optional<value_type> get(const key_type& key) {
        std::optional<value_type> value = std::nullopt;
        TRANSACTION {
            auto result = table_.transGet(
                    key, table_type::AccessMethod::ReadOnly);
            CHK(!result.abort);
            if (result.success) {
                value = *result.value;
            }
        } RETRY(true);
        return value;
    }

private:
    table_type table_;
};

};  // namespace storia
