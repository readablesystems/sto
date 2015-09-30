#define INTERVAL_TREE_DEBUG 1
#define rbaccount(x) ++rbaccount_##x
unsigned long long rbaccount_rotation, rbaccount_flip, rbaccount_insert, rbaccount_erase;
#include <iostream>
#include <utility>
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

int main() {
    RBTree<int, int> tree;
    // test single-threaded operations
    {
        Transaction t;
        Sto::set_transaction(&t);
        for (int i = 0; i < 100; ++i) {
            tree[i] = i;
            assert(tree[i]==i);
            tree[i] = 100-i;
            assert(tree[i]==100-i);
        }
        for (int i = 0; i < 100; ++i) {
            assert(tree.count(i) == 1);
        }
        for (int i = 0; i < 100; ++i) {
            assert(tree.erase(i) == 1);
            assert(tree.count(i) == 0);
        }
        int x = tree[102];
        assert(x==0);
        assert(t.try_commit());
    }
    return 0;
}
