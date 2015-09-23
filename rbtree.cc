#include <iostream>
#include <assert.h>
#include <stdio.h>

#include "RBTree.hh"
#include "Transaction.hh"

#define N 100

template <> class rbwrapper<int> {
  public:
    template <typename... Args> inline rbwrapper(int x)
	: x_(x) {
    }
    inline int value() const {
	return x_;
    }
    int x_;
    rblinks<rbwrapper<int> > rblinks_;
};

std::ostream& operator<<(std::ostream& s, rbwrapper<int> x) {
    return s << x.value();
}

int main() {
    // run on both Hashtable and MassTrans
    rbtree<rbwrapper<int>> rbt;
    rbt.insert(1);
    rbt.count(1);
    // TODO eventually will want to run basic map tests (like hash/masstree)
    // basicMapTests(rbt);
    return 0;
}

