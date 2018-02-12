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
typedef RBTree<int, int, true> tree_type;
TransactionTid::type lock;
// initialize the tree: contains (1,1), (2,2), (3,3)
void reset_tree(tree_type& tree) {
    TestTransaction t(1);
    t.use();
    // initialize the tree: contains (1,1), (2,2), (3,3)
    tree[1] = 1;
    tree[2] = 2;
    tree[3] = 3;
    assert(t.try_commit());
}
/**** update <-> update conflict; update <-> erase; update <-> count counflicts ******/
void update_conflict_tests() {
    {
        tree_type tree;
        TestTransaction t1(1), t2(2);
        t1.use();
        tree[55] = 56;
        tree[57] = 58;
        t2.use();
        int x = tree[58];
        assert(x == 0);
        assert(t2.try_commit());
        assert(t1.try_commit());
    }
    {
        tree_type tree;
        TestTransaction t1(1), t2(2);
        t1.use();
        tree[10] = 10;
        t2.use();
        int x = tree[58];
        assert(x == 0);
        assert(t2.try_commit());
        t1.use();
        assert(t1.try_commit());
    }
}
/***** erase <-> count; erase <-> erase conflicts ******/
void erase_conflict_tests() {
    {
        // t1:count - t1:erase - t2:count - t1:commit - t2:abort
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), t2(2), after(3);
        t1.use();
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        t2.use();
        tree[50] = 50; // prevent read-only txn
        assert(tree.count(1) == 1);
        assert(t1.try_commit());
        assert(!t2.try_commit());
        // check that the commit did its job
        after.use();
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
    {
        // t1:count - t1:erase - t2:count - t2:commit - t1:commit
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), t2(2), after(3);
        t1.use();
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        t2.use();
        assert(tree.count(1) == 1);
        assert(t2.try_commit());
        assert(t1.try_commit());
        after.use();
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
    {
        // t1:count - t1:erase - t1:count - t2:erase - t2:commit - t1:abort
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), t2(2), after(3);
        t1.use();
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        assert(tree.count(1) == 0); // bug in test?
        t2.use();
        assert(tree.erase(1) == 1);
        assert(t2.try_commit());
        assert(!t1.try_commit());
        after.use();
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
    {
        // t1:count - t1:erase - t1:count - t2:erase - t1:commit - t2:abort XXX technically t2 doesn't have to abort?
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), t2(2), after(3);
        t1.use();
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        assert(tree.count(1) == 0); // bug in test?
        t2.use();
        assert(tree.erase(1) == 1);
        assert(t1.try_commit());
        assert(!t2.try_commit());
        after.use();
        assert(tree.count(1)==0);
        assert(after.try_commit());
    }
}

void insert_then_delete_tests() {
    {
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), after(2);
        t1.use();
        tree[5] = 5;
        tree[4] = 4;
        assert(tree.count(4) == 1);
        // insert-then-delete
        assert(tree.erase(4) == 1);
        assert(tree.count(4) == 0);
        assert(tree.erase(4) == 0);
        // insert-delete-insert
        tree[4] = 44;
        assert(tree[4] == 44);
        assert(tree.count(4) == 1);
        assert(t1.try_commit());
        // check insert-delete-insert
        // is actually installed
        after.use();
        assert(tree.count(4) == 1);
        assert(tree[4] == 44);
        for (int i = 1; i <= 5; ++i) {
            if (i != 4)
                assert(tree[i] == i);
        }
        assert(after.try_commit());
    }
    {
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), after(2);
        t1.use();
        // absent read of key 4
        // reads nodeversion of key 3
        assert(tree.count(4) == 0);
        // increments nodeversion of key 3
        tree[5] = 5;
        // absent read of key 4 again
        assert(tree.count(4) == 0);
        tree[4] = 4;
        assert(tree.count(4) == 1);
        assert(t1.try_commit());
        after.use();
        for (int i = 0; i <= 5; ++i)
            assert(tree[i] == i);
        assert(after.try_commit());
    }
    {
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), t2(2), t3(3), after(4);
        t1.use();
        // t1: update
        tree[3] = 13;

        t2.use();
        // t2: delete key 3
        assert(tree.erase(3) == 1);
        // t2 committed
        assert(t2.try_commit());

        t3.use();
        // t3: checks that key 3 is not in
        // the tree, and inserts 3
        assert(tree.count(3) == 0);
        tree[3] = 33;
        // t3 committed
        assert(t3.try_commit());

        t1.use();
        // t1 cannot commit in the current scheme
        assert(!t1.try_commit());

        after.use();
        assert(tree[3] == 33);
        assert(after.try_commit());
    }
}

void mem_tests() {
    {
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1), t2(2), after(3);

        t1.use();
        // prevent read-only txn
        tree[50] = 50;
        // absent get of key 4
        assert(tree.count(4) == 0);
        t2.use();
        tree[5] = 5;
        assert(t2.try_commit());
        t1.use();
        // t1 should abort as a result
        assert(!t1.try_commit());

        after.use();
        assert(tree.count(4) == 0);
        assert(tree[5] == 5);
        assert(after.try_commit());
    }
    {
        tree_type tree;
        reset_tree(tree);
        TestTransaction t1(1);

        t1.use();
        tree.erase(1);
        tree.erase(2);
        tree.erase(3);
        assert(t1.try_commit());
    }
}

int main() {
    TThread::txn = nullptr;
    TThread::set_id(0);
    // test single-threaded operations
    {
        tree_type tree;
        TestTransaction t(1);
        // read_my_inserts
        assert(tree.size() == 0);
        for (int i = 0; i < 100; ++i) {
            tree[i] = i;
            assert(tree[i]==i);
            tree[i] = 100-i;
            assert(tree[i]==100-i);
        }
        assert(tree.size() == 100);

        // iterators
        //int i = 100;
        //for (auto it = tree.begin(); it != tree.end(); it++) {
        //    std::cout << "iterator is " << *it << std::endl;
        //    assert((*it) == i--);
        //}
        
        // count_my_inserts
        for (int i = 0; i < 100; ++i) {
            assert(tree.count(i) == 1);
        }
        assert(tree.size() == 100);
        // delete_my_inserts and read_my_deletes
        for (int i = 0; i < 100; ++i) {
            assert(tree.erase(i) == 1);
            assert(tree.count(i) == 0);
        }
        assert(tree.size() == 0);
        // delete_my_deletes
        for (int i = 0; i < 100; ++i) {
            assert(tree.erase(i) == 0);
            assert(tree.count(i) == 0);
        }
        assert(tree.size() == 0);
        // insert_my_deletes
        for (int i = 0; i < 100; ++i) {
            tree[i] = 1;
            assert(tree.count(i) == 1);
        }
        assert(tree.size() == 100);
        // operator[] inserts empty value
        int x = tree[102];
        assert(x==0);
        assert(tree.count(102)==1);
        assert(tree.size() == 101);
        assert(t.try_commit());
    }
    erase_conflict_tests();
    update_conflict_tests();
    insert_then_delete_tests();
    mem_tests();
    // test abort-cleanup
    std::cout << "ALL TESTS PASS!!" << std:: endl;
    return 0;
}

// Removed serializability test
// trans_test.cc and Testers.hh is the current test framework for fuzz testing
