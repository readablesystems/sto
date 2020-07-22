#pragma once

#include "Hashtable.hh"
#include "Sto.hh"

#include "Entry.hh"
#include "Update.hh"

namespace storia {

template <typename K, typename V>
class Table {
public:
    typedef K key_type;
    typedef V value_type;

private:
    class table_params_ : public Hashtable_params<key_type, value_type> {};
    typedef Hashtable<table_params_> table_type;

public:

    explicit Table() {
        static_assert(
                std::is_base_of_v<Entry, value_type>,
                "Value type must derive from storia::Entry.");
    }

    template <typename UpdateType>
    std::enable_if_t<std::is_base_of_v<Update, UpdateType>, bool>
    apply(UpdateType& update) {
        bool success = false;
        if constexpr (UpdateType::TYPE == UpdateType::Type::BlindWrite) {
            bool retry = true;
            TXN {
                const key_type key = update.template key<key_type>();
                auto result = table_.transPut(
                        key, update.vp(), update.overwrite());
                CHK(!result.abort);
                if (!update.overwrite() && result.existed) {
                    retry = false;
                    CHK(false);
                }
            } RETRY(retry);
            success = retry;
        } else if constexpr (
                UpdateType::TYPE == UpdateType::Type::ReadWrite) {
            bool retry = true;
            RWTXN {
                const key_type key = update.template key<key_type>();
                auto read_result = table_.transGet(
                        key, table_type::AccessMethod::ReadWrite);
                CHK(!read_result.abort);
                if (!update.predicate().eval(*read_result.value)) {
                    retry = false;  // Give up
                    CHK(false);
                }
                auto write_result = table_.transPut(
                        key, update.vp(), update.overwrite());
                CHK(!write_result.abort);
            } RETRY(retry);
            success = retry;
        } else if constexpr (
                UpdateType::TYPE == UpdateType::Type::PartialBlindWrite) {
            TXN {
                const key_type key = update.template key<key_type>();
                auto read_result = table_.transGet(
                        key, table_type::AccessMethod::Blind);
                CHK(!read_result.abort);
                CHK(read_result.success);
                table_.transUpdate(read_result.ref, update.updater());
            } RETRY(true);
            success = true;
        } else if constexpr (
                UpdateType::TYPE == UpdateType::Type::PartialReadWrite) {
            bool retry = true;
            TXN {
                const key_type key = update.template key<key_type>();
                auto read_result = table_.transGet(
                        key, table_type::AccessMethod::ReadWrite);
                CHK(!read_result.abort);
                CHK(read_result.success);
                if (!update.predicate().eval(*read_result.value)) {
                    retry = false;  // Give up
                    CHK(false);
                }
                table_.transUpdate(read_result.ref, update.updater());
            } RETRY(true);
            success = retry;
        } else {
            static_assert("Unrecognized Update type provided.");
        }
        return success;
    }

    std::optional<value_type> get(const key_type& key) {
        std::optional<value_type> value = std::nullopt;
        TXN {
            auto result = table_.transGet(
                    key, table_type::AccessMethod::ReadOnly);
            CHK(!result.abort);
            if (result.success) {
                value = *result.value;
            }
        } RETRY(true);
        return value;
    }

    void nontrans_put(const key_type& key, const value_type& value) {
        table_.nontrans_put(key, value);
    }

private:
    table_type table_;
};

};  // namespace storia
