#pragma once

#include "Hashtable.hh"
#include "Sto.hh"

#include "Entry.hh"
#include "Update.hh"

namespace storia {

namespace state {

template <typename K, typename V>
class Table {
public:
    typedef K key_type;
    typedef V value_type;

private:
    class table_params_ : public Hashtable_params<K, value_type> {};
    typedef Hashtable<table_params_> table_type;

public:

    explicit Table() {
        static_assert(
            std::is_base_of_v<Entry, value_type>,
            "Value type must derive from storia::StateEntry.");
    }

    template <typename UpdateType>
    std::enable_if_t<std::is_base_of_v<Update, UpdateType>, void>
    apply(UpdateType& update) {
        if constexpr (UpdateType::TYPE == UpdateType::Type::BlindWrite) {
            TRANSACTION {
                const key_type key = update.template key<key_type>();
                auto result = table_.transPut(key, update.vp(), true);
                CHK(!result.abort);
            } RETRY(true);
        } else if constexpr (UpdateType::TYPE == UpdateType::Type::DeltaWrite) {
            TRANSACTION {
                const key_type key = update.key();
                auto read_result = table_.transGet(
                        key, table_type::AccessMethod::Blind);
                CHK(!read_result.abort);
                auto write_result = table_.transUpdate(
                        read_result.ref, update.updater());
                CHK(!write_result.abort);
            } RETRY(true);
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

};  // namespace

};  // namespace storia
