#undef NDEBUG
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <cassert>
#include <vector>
#include "Sto.hh"
#include "TART.hh"
#include "Transaction.hh"
#include <unistd.h>
#include <random>
#include <time.h>
#include "DB_index.hh"
#include "DB_params.hh"

class keyval_db {
public:
    virtual void insert(uint64_t int_key, uintptr_t val) = 0;
    virtual void update(uint64_t int_key, uintptr_t val) = 0;
    virtual uintptr_t lookup(uint64_t int_key) = 0;
    virtual void erase(uint64_t int_key) = 0;
};

class masstree_wrapper : public keyval_db {
public:
    struct oi_value {
        enum class NamedColumn : int { val = 0 };
        uintptr_t val;
    };

    struct oi_key {
        uint64_t key;
        oi_key(uint64_t k) {
            key = k;
        }
        bool operator==(const oi_key& other) const {
            return (key == other.key);
        }
        bool operator!=(const oi_key& other) const {
            return !(*this == other);
        }
        operator lcdf::Str() const {
            return lcdf::Str((const char *)this, sizeof(*this));
        }
    };

    typedef bench::ordered_index<oi_key, oi_value, db_params::db_default_params> index_type;
    index_type oi;

    masstree_wrapper() {
    }

    void insert(uint64_t key, uintptr_t val) override {
        bool success;
        std::tie(success, std::ignore) = oi.insert_row(oi_key(key), new oi_value{val});
        if (!success) throw Transaction::Abort();
    }

    uintptr_t lookup(uint64_t key) override {
        uintptr_t ret;
        bool success;
        const oi_value* val;
        std::tie(success, std::ignore, std::ignore, val) = oi.select_row(oi_key(key), bench::RowAccess::None);
        if (!success) throw Transaction::Abort();
        if (!val) {
            ret = 0;
        } else {
            ret = val->val;
        }
        return ret;
    }

    void update(uint64_t key, uintptr_t val) override {
        bool success;
        uintptr_t row;
        const oi_value* value;
        std::tie(success, std::ignore, row, value) = oi.select_row(oi_key(key), bench::RowAccess::UpdateValue);
        if (!success) throw Transaction::Abort();
        auto new_oiv = Sto::tx_alloc<oi_value>(value);
        new_oiv->val = val;
        oi.update_row(row, new_oiv);
    }

    void erase(uint64_t key) override {
        oi.delete_row(oi_key(key));
    }
};

class tart_wrapper : public keyval_db {
public:
    struct oi_value {
        enum class NamedColumn : int { val = 0 };
        uintptr_t val;
    };

    struct oi_key {
        uint64_t key;
        oi_key(uint64_t k) {
            key = k;
        }
        bool operator==(const oi_key& other) const {
            return (key == other.key);
        }
        bool operator!=(const oi_key& other) const {
            return !(*this == other);
        }
        operator lcdf::Str() const {
            return lcdf::Str((const char *)this, sizeof(*this));
        }
    };

    typedef bench::art_index<oi_key, oi_value, db_params::db_default_params> index_type;
    index_type oi;

    tart_wrapper() {
    }

    void insert(uint64_t key, uintptr_t val) override {
        bool success;
        std::tie(success, std::ignore) = oi.insert_row(oi_key(key), new oi_value{val});
        if (!success) throw Transaction::Abort();
    }

    uintptr_t lookup(uint64_t key) override {
        uintptr_t ret;
        bool success;
        bool found;
        const oi_value* val;
        std::tie(success, std::ignore, std::ignore, val) = oi.select_row(oi_key(key), bench::RowAccess::None);
        if (!success) throw Transaction::Abort();
        if (!val) {
            ret = 0;
        } else {
            ret = val->val;
        }
        return ret;
    }

    void update(uint64_t key, uintptr_t val) override {
        bool success;
        uintptr_t row;
        const oi_value* value;
        std::tie(success, std::ignore, row, value) = oi.select_row(oi_key(key), bench::RowAccess::UpdateValue);
        if (!success) throw Transaction::Abort();
        auto new_oiv = Sto::tx_alloc<oi_value>(value);
        new_oiv->val = val;
        oi.update_row(row, new_oiv);
    }

    void erase(uint64_t key) override {
        oi.delete_row(oi_key(key));
    }
};

int main(int argc, char *argv[]) {
    keyval_db* db;

    bool use_art = 1;
    if (argc > 1) {
        use_art = atoi(argv[1]);
    }

    if (use_art) {
        printf("ART\n");
        db = new tart_wrapper();
    } else {
        printf("Masstree\n");
        db = new masstree_wrapper();
    }

    uint64_t key1 = 1;
    uint64_t key2 = 2;
    // {
    //     TransactionGuard t;
    //     db->insert(key1, 123);
    //     db->insert(key2, 321);
    // }
    //
    // {
    //     TransactionGuard t;
    //     volatile auto x = db->lookup(key1);
    //     volatile auto y = db->lookup(key2);
    //     assert(x == 123);
    //     assert(y == 321);
    // }

    // if (use_art) {
    //     printf("ART\n");
    //     db = new tart_wrapper();
    // } else {
    //     printf("Masstree\n");
    //     db = new masstree_wrapper();
    // }

    uint64_t checkkey = 256;

    {
        TransactionGuard t;
        db->insert(key1, 123);
        db->insert(key2, 321);
    }

    {
        TransactionGuard t;
        volatile auto x = db->lookup(key1);
        volatile auto y = db->lookup(key2);
        db->insert(checkkey, 100);
        assert(x == 123);
        assert(y == 321);
    }

    {
        TransactionGuard t;
        db->erase(key1);
        volatile auto x = db->lookup(key1);
        db->insert(checkkey, 100);
        printf("%d\n", x);
        assert(x == 0);
    }

    {
        TransactionGuard t;
        volatile auto x = db->lookup(key1);
        assert(x == 0);
        db->insert(key1, 567);
    }

    {
        TransactionGuard t;
        volatile auto x = db->lookup(key1);
        db->insert(checkkey, 100);
        assert(x == 567);
    }

    printf("Tests pass\n");
}
