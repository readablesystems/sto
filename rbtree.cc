#define INTERVAL_TREE_DEBUG 1
#define rbaccount(x) ++rbaccount_##x
unsigned long long rbaccount_rotation, rbaccount_flip, rbaccount_insert, rbaccount_erase;
#include <iostream>
#include <utility>
#include <map>
#include <vector>
#include <string.h>
#include "RBTree.hh"
#include <sys/time.h>
#include <sys/resource.h>

void rbaccount_report() {
    unsigned long long all = rbaccount_insert + rbaccount_erase;
    fprintf(stderr, "{\"insert\":%llu,\"erase\":%llu,\"rotation_per_operation\":%g,\"flip_per_operation\":%g}\n",
            rbaccount_insert, rbaccount_erase, (double) rbaccount_rotation / all, (double) rbaccount_flip / all);
}

#define PAIR(k,v) std::pair<int, int>(k, v)
typedef RBTree<int, int> tree_type;
// initialize the tree: contains (1,1), (2,2), (3,3)
void reset_tree(tree_type& tree) {
    Transaction init;
    // initialize the tree: contains (1,1), (2,2), (3,3)
    Sto::set_transaction(&init);
    tree[1] = 1;
    tree[2] = 2;
    tree[3] = 3;
    assert(init.try_commit());
}
/**** update <-> update conflict; update <-> erase; update <-> count counflicts ******/
void update_conflict_tests() {
    {
        tree_type tree;
        Transaction t1, t2;
        Sto::set_transaction(&t1);
        tree[55] = 56;
        tree[57] = 58;
        Sto::set_transaction(&t2);
        int x = tree[58];
        assert(x == 0);
        assert(t2.try_commit());
        Sto::set_transaction(&t1);
        assert(t1.try_commit());
    }
    {
        tree_type tree;
        Transaction t1, t2;
        Sto::set_transaction(&t1);
        tree[10] = 10;
        Sto::set_transaction(&t2);
        int x = tree[58];
        assert(x == 0);
        assert(t2.try_commit());
        Sto::set_transaction(&t1);
        assert(t1.try_commit());
    }
}
/***** erase <-> count; erase <-> erase conflicts ******/
void erase_conflict_tests() {
    {
        // t1:count - t1:erase - t2:count - t1:commit - t2:abort
        tree_type tree;
        Transaction t1, t2, after;
        reset_tree(tree);
        Sto::set_transaction(&t1);
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        Sto::set_transaction(&t2);
        assert(tree.count(1) == 1);
        Sto::set_transaction(&t1);
        assert(t1.try_commit());
        Sto::set_transaction(&t2);
        assert(!t2.try_commit());
        // check that the commit did its job
        Sto::set_transaction(&after);
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
    {
        // t1:count - t1:erase - t2:count - t2:commit - t1:commit
        tree_type tree;
        Transaction t1, t2, after;
        reset_tree(tree);
        Sto::set_transaction(&t1);
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        Sto::set_transaction(&t2);
        assert(tree.count(1) == 1);
        Sto::set_transaction(&t2);
        assert(t2.try_commit());
        Sto::set_transaction(&t1);
        assert(t1.try_commit());
        Sto::set_transaction(&after);
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
    {
        // t1:count - t1:erase - t1:count - t2:erase - t2:commit - t1:abort
        tree_type tree;
        Transaction t1, t2, after;
        reset_tree(tree);
        Sto::set_transaction(&t1);
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        assert(tree.count(1) == 1);
        Sto::set_transaction(&t2);
        assert(tree.erase(1) == 1);
        Sto::set_transaction(&t2);
        assert(t2.try_commit());
        Sto::set_transaction(&t1);
        assert(!t1.try_commit());
        Sto::set_transaction(&after);
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
    {
        // t1:count - t1:erase - t1:count - t2:erase - t1:commit - t2:abort XXX technically t2 doesn't have to abort?
        tree_type tree;
        Transaction t1, t2, after;
        reset_tree(tree);
        Sto::set_transaction(&t1);
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        assert(tree.count(1) == 1);
        Sto::set_transaction(&t2);
        assert(tree.erase(1) == 1);
        Sto::set_transaction(&t1);
        assert(t1.try_commit());
        Sto::set_transaction(&t2);
        assert(!t2.try_commit());
        Sto::set_transaction(&after);
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
}

int main() {
    // test single-threaded operations
    {
        tree_type tree;
        Transaction t;
        Sto::set_transaction(&t);
        // read_my_inserts
        for (int i = 0; i < 100; ++i) {
            tree[i] = i;
            assert(tree[i]==i);
            tree[i] = 100-i;
            assert(tree[i]==100-i);
        }
        // count_my_inserts
        for (int i = 0; i < 100; ++i) {
            assert(tree.count(i) == 1);
        }
        // delete_my_inserts and read_my_deletes
        for (int i = 0; i < 100; ++i) {
            assert(tree.erase(i) == 1);
            assert(tree.count(i) == 0);
        }
        // delete_my_deletes
        for (int i = 0; i < 100; ++i) {
            assert(tree.erase(i) == 0);
            assert(tree.count(i) == 0);
        }
        // insert_my_deletes
        for (int i = 0; i < 100; ++i) {
            tree[i] = 1;
            assert(tree.count(i) == 1);
        }
        // operator[] inserts empty value
        int x = tree[102];
        assert(x==0);
        assert(tree.count(102)==1);
        assert(t.try_commit());
    }
    erase_conflict_tests();
    update_conflict_tests();
    // test abort-cleanup
    return 0;
}


// Serializability testing infrastructure -- still working on it

class txnop_type {
protected:
    int txid; // transaction (thread) id
    int type; // 0: read, 1: write
    int key;
    int value;
public:
    txnop_type(int txid_, int type_, int key_, int value_) :
        txid(txid_), type(type_), key(key_), value(value_) {}
};

typedef std::pair<std::vector<txnop_type>, std::map<int, int>> record_type;
typedef std::function<void(tree_type&, record_type&, bool)> txn_func_type;

void prepare_serial_order(record_type& record, const txn_func_type& txn1,
                const txn_func_type& txn2, int order) {
    tree_type tree;
    if (order) {
        txn2(tree, record, true);
        txn1(tree, record, true);
    } else {
        txn1(tree, record, true);
        txn2(tree, record, true);
    }
}

void record_read(int txid, int key, int value, record_type& record) {
    record.first.push_back(txnop_type(txid, 0, key, value));
}

void record_write(int txid, int key, int value, record_type& record, bool write_map) {
    record.first.push_back(txnop_type(txid, 1, key, value));
    if (write_map)
        (record.second)[key] = value;
}

#define REC_READ(k, v)                             \
        if (serial) {                              \
            record_read(txid, k, v, record);       \
        } else {                                   \
            record_read(txid, k, v, trans_record); \
        }

#define REC_WRITE(k, v)                                     \
        if (serial) {                                       \
            record_write(txid, k, v, record, true);         \
        } else {                                            \
            record_write(txid, k, v, trans_record, false);  \
        }

#define RBTREE_PUT(k, v) tree[k] = v; REC_WRITE(k, v)
#define RBTREE_GET(k) {auto x = tree[k]; REC_READ(k, x)}

void trans_thread1(tree_type& tree, record_type& record, bool serial) {
    int txid = 1;
    record_type trans_record;
    TRANSACTION {
        if (!serial)
            trans_record.first.clear();
        RBTREE_PUT(40, 41);
        RBTREE_PUT(143, 144);
        RBTREE_PUT(87, 88);
        RBTREE_GET(90);
        RBTREE_GET(16);
    } RETRY(true)

    if (!serial) {
        // return the internal (transactional consistent) record
        record.swap(trans_record);
    }
}

void run_parallel_chk(const txn_func_type& txn1, const txn_func_type& txn2,
                const record_type& order_one, const record_type& order_two) {
    //XXX not implemented
    (void)txn1; (void)txn2;
    (void)order_one; (void)order_two;
    return;
}

void serializability_test(txn_func_type txn1, txn_func_type txn2) {
    // for now we only check two serialization orders
    // we will need a vector if we want to check more
    record_type order_one;
    record_type order_two;

    prepare_serial_order(order_one, txn1, txn2, 0);
    prepare_serial_order(order_two, txn1, txn2, 1);

    run_parallel_chk(txn1, txn2, order_one, order_two);
}
