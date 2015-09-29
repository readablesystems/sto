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
    {
        Transaction t;
        Sto::set_transaction(&t);
        tree[1] = 1;
        tree[2] = 2;
        tree[3] = 3;
        assert(tree[1]==1);
        assert(tree[2]==2);
        assert(tree[3]==3);
        assert(tree.count(1) == 1);
        assert(tree.erase(1) == 1);
        assert(t.try_commit());
    }
    return 0;
}
