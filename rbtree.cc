#define INTERVAL_TREE_DEBUG 1
#define rbaccount(x) ++rbaccount_##x
unsigned long long rbaccount_rotation, rbaccount_flip, rbaccount_insert, rbaccount_erase;
#include <iostream>
#include <string.h>
#include "RBTree.hh"
#include <sys/time.h>
#include <sys/resource.h>

template <typename T>
class rbwrapper : public T {
  public:
    template <typename... Args> inline rbwrapper(Args&&... args)
	: T(std::forward<Args>(args)...) {
    }
    inline rbwrapper(const T& x)
	: T(x) {
    }
    inline rbwrapper(T&& x) noexcept
	: T(std::move(x)) {
    }
    inline const T& value() const {
	return *this;
    }
    rblinks<rbwrapper<T> > rblinks_;
};

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

template <typename T>
class default_comparator<rbwrapper<T> > {
  public:
    inline int operator()(const rbwrapper<T> &a, const rbwrapper<T> &b) const {
	return default_compare(a.value(), b.value());
    }
    inline int operator()(const rbwrapper<T> &a, const T &b) const {
	return default_compare(a.value(), b);
    }
    inline int operator()(const T &a, const rbwrapper<T> &b) const {
	return default_compare(a, b.value());
    }
    inline int operator()(const T &a, const T &b) const {
	return default_compare(a, b);
    }
};

template <typename T> class less {};
template <typename T>
class less<rbwrapper<T> > {
  public:
    inline bool operator()(const rbwrapper<T>& a, const rbwrapper<T>& b) const {
	return a.value() < b.value();
    }
    inline bool operator()(const rbwrapper<T>& a, const T& b) const {
	return a.value() < b;
    }
    inline bool operator()(const T& a, const rbwrapper<T>& b) const {
	return a < b.value();
    }
    inline bool operator()(const T& a, const T& b) const {
	return a < b;
    }
};

template <typename A, typename B>
struct semipair {
    A first;
    B second;
    inline semipair() {
    }
    inline semipair(const A &a)
	: first(a) {
    }
    inline semipair(const A &a, const B &b)
	: first(a), second(b) {
    }
    inline semipair(const semipair<A, B> &x)
	: first(x.first), second(x.second) {
    }
    template <typename X, typename Y> inline semipair(const semipair<X, Y> &x)
	: first(x.first), second(x.second) {
    }
    template <typename X, typename Y> inline semipair(const std::pair<X, Y> &x)
	: first(x.first), second(x.second) {
    }
    inline semipair<A, B> &operator=(const semipair<A, B> &x) {
	first = x.first;
	second = x.second;
	return *this;
    }
    template <typename X, typename Y>
    inline semipair<A, B> &operator=(const semipair<X, Y> &x) {
	first = x.first;
	second = x.second;
	return *this;
    }
    template <typename X, typename Y>
    inline semipair<A, B> &operator=(const std::pair<X, Y> &x) {
	first = x.first;
	second = x.second;
	return *this;
    }
};

template <typename A, typename B>
std::ostream &operator<<(std::ostream &s, const semipair<A, B> &x) {
    return s << '<' << x.first << ", " << x.second << '>';
}

struct compare_first {
    template <typename T, typename U>
    inline int operator()(const T &a, const std::pair<T, U> &b) const {
	return default_compare(a, b.first);
    }
    template <typename T, typename U, typename V>
    inline int operator()(const std::pair<T, U> &a, const std::pair<T, V> &b) const {
	return default_compare(a.first, b.first);
    }
    template <typename T, typename U>
    inline int operator()(const T &a, const semipair<T, U> &b) const {
	return default_compare(a, b.first);
    }
    template <typename T, typename U, typename V>
    inline int operator()(const semipair<T, U> &a, const semipair<T, V> &b) const {
	return default_compare(a.first, b.first);
    }
};

void rbaccount_report() {
    unsigned long long all = rbaccount_insert + rbaccount_erase;
    fprintf(stderr, "{\"insert\":%llu,\"erase\":%llu,\"rotation_per_operation\":%g,\"flip_per_operation\":%g}\n",
            rbaccount_insert, rbaccount_erase, (double) rbaccount_rotation / all, (double) rbaccount_flip / all);
}

int main() {
    {
        rbtree<rbwrapper<int> > tree;
        tree.insert(*new rbwrapper<int>(0));
        auto x = new rbwrapper<int>(1);
        tree.insert(*x);
        tree.insert(*new rbwrapper<int>(0));
        tree.insert(*new rbwrapper<int>(-2));
        auto y = new rbwrapper<int>(0);
        tree.insert(*y);
        std::cerr << tree << "\n";
        tree.erase(*x);
        std::cerr << tree << "\n";
        tree.erase(*y);
        std::cerr << tree << "\n";
    }

    {
        rbtree<rbwrapper<int> > tree;
        tree.insert(*new rbwrapper<int>(0));
        tree.insert(*new rbwrapper<int>(2));
        for (int i = 0; i != 40; ++i)
            tree.insert(*new rbwrapper<int>(1));
    }
}
