#pragma once

#include <utility>
#include <tuple>
#include <vector>
#include <iomanip>
#include <iostream>
#include "Interface.hh"

#ifndef rbaccount
# define rbaccount(x)
#endif

template <typename T>
class rbnodeptr {
  public:
    // what is unspecified_bool_type doing? 
    typedef bool (rbnodeptr<T>::*unspecified_bool_type)() const;

    rbnodeptr() = default;
    inline rbnodeptr(T* x, bool color);
    explicit inline rbnodeptr(uintptr_t ivalue);

    inline operator unspecified_bool_type() const;
    inline bool operator!() const;
    inline uintptr_t ivalue() const;

    inline T* node() const;
    inline rbnodeptr<T>& child(bool isright) const;
    inline bool children_same_color() const;
    inline bool find_child(T* node) const;
    inline rbnodeptr<T>& load_color();
    inline rbnodeptr<T> set_child(bool isright, rbnodeptr<T> x, T*& root) const;
    inline T*& parent() const;
    inline rbnodeptr<T> black_parent() const;

    inline bool red() const;
    inline rbnodeptr<T> change_color(bool color) const;
    inline rbnodeptr<T> reverse_color() const;

    inline rbnodeptr<T> rotate(bool isright) const;
    inline rbnodeptr<T> flip() const;

    size_t size() const;
    template <typename C>
    void check(T* parent, int this_black_height, int& black_height,
               T* root, T* begin, T* end, C& compare) const;
    void output(std::ostream& s, int indent, T* highlight) const;

  private:
    uintptr_t x_;
};

template <typename T>
class rblinks {
  public:
    T* p_;
    rbnodeptr<T> c_[2];
}; 

namespace rbpriv {
template <typename Compare>
struct rbcompare : private Compare {
    inline rbcompare(const Compare &compare);
    template <typename T>
    inline int node_compare(const T& a, const T& b) const;
    template <typename A, typename B>
    inline int compare(const A &a, const B &b) const;
    inline Compare& get_compare() const;
};

template <typename T>
struct default_comparator {
    inline int operator()(const T &a, const T &b) const {
    return a < b ? -1 : (b < a ? 1 : 0);
    }
};

template <typename T>
inline int default_compare(const T &a, const T &b) {
    return default_comparator<T>()(a, b);
}

template <typename T, typename Compare>
struct rbrep : public rbcompare<Compare> {
    T* root_;
    T* limit_[2];
    typedef rbcompare<Compare> rbcompare_type;
    inline rbrep(const Compare &compare);
};

template <typename C, typename Ret> class rbcomparator;

template <typename C>
class rbcomparator<C, bool> {
  public:
    inline rbcomparator(C comp)
        : comp_(comp) {
    }
    template <typename A, typename B>
    inline bool less(const A& a, const B& b) {
        return comp_(a, b);
    }
    template <typename A, typename B>
    inline bool greater(const A& a, const B& b) {
        return comp_(b, a);
    }
    template <typename A, typename B>
    inline int compare(const A& a, const B& b) {
        return comp_(a, b) ? -1 : comp_(b, a);
    }
  private:
    C comp_;
};

template <typename C>
class rbcomparator<C, int> {
  public:
    inline rbcomparator(C comp)
        : comp_(comp) {
    }
    template <typename A, typename B>
    inline bool less(const A& a, const B& b) {
        return comp_(a, b) < 0;
    }
    template <typename A, typename B>
    inline bool greater(const A& a, const B& b) {
        return comp_(a, b) > 0;
    }
    template <typename A, typename B>
    inline int compare(const A& a, const B& b) {
        return comp_(a, b);
    }
  private:
    C comp_;
};

template <typename K, typename T, typename C>
inline auto make_compare(C comp) -> rbcomparator<C, decltype(comp(*(K*)nullptr, *(T*)nullptr))> {
    return rbcomparator<C, decltype(comp(*(K*)nullptr, *(T*)nullptr))>(comp);
}

} // namespace rbpriv

template <typename T>
class rbalgorithms {
  public:
    static inline T* next_node(T* n);
    static inline T* prev_node(T* n);
    static inline T* prev_node(T* n, T* last);
    static inline T* step_node(T* n, bool forward);
    static inline T* edge_node(T* n, bool forward);
};


template <typename T, typename Compare = rbpriv::default_comparator<T>>
class rbtree {
  public:
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef Compare value_compare;
    typedef T node_type;

    typedef TVersion Version;
    typedef std::tuple<T*, Version> node_info_type;
    typedef std::pair<node_info_type, node_info_type> boundaries_type;

    inline rbtree(const value_compare &compare = value_compare());
    ~rbtree();

    inline node_type *root();

    // lookup
    inline bool empty() const;
    inline size_t size() const;

    // modifiers
    inline rbnodeptr<T> insert(reference n);
    inline T* erase(reference x);
  
    template <typename TT, typename CC>
    friend std::ostream &operator<<(std::ostream &s, const rbtree<TT, CC> &tree);
    int check() const;

  private:
    rbpriv::rbrep<T, Compare> r_;
    // moved from RBTree.hh
    Version treeversion_;

    template <typename K, typename Comp>
    inline std::tuple<T*, Version, bool, boundaries_type> find_any(const K& key, Comp comp) const;

    template <typename K, typename Comp>
    inline std::tuple<T*, Version, bool, boundaries_type, node_info_type> find_insert(K& key, Comp comp);

    template <typename K, typename Comp>
    inline std::tuple<rbnodeptr<T>, bool> find_or_parent(const K& key, Comp comp) const;

    void insert_commit(T* x, rbnodeptr<T> p, bool side);
    T* delete_node(T* victim, T* successor_hint);
    void delete_node_fixup(rbnodeptr<T> p, bool side);

    template<typename K, typename V, bool GlobalSize> friend class RBTree;
};

template <typename T>
inline rbnodeptr<T>::rbnodeptr(T* x, bool color)
    : x_(reinterpret_cast<uintptr_t>(x) | color) {
}

template <typename T>
inline rbnodeptr<T>::rbnodeptr(uintptr_t x)
    : x_(x) {
}

template <typename T>
inline rbnodeptr<T>::operator unspecified_bool_type() const {
    return x_ != 0 ? &rbnodeptr<T>::operator! : 0;
}

template <typename T>
inline bool rbnodeptr<T>::operator!() const {
    return !x_;
}

template <typename T>
inline uintptr_t rbnodeptr<T>::ivalue() const {
    return x_;
}

template <typename T>
inline T* rbnodeptr<T>::node() const {
    return reinterpret_cast<T*>(x_ & ~uintptr_t(1));
}

template <typename T>
inline rbnodeptr<T>& rbnodeptr<T>::child(bool isright) const {
    return node()->rblinks_.c_[isright];
}

template <typename T>
inline bool rbnodeptr<T>::children_same_color() const {
    return ((node()->rblinks_.c_[0].x_ ^ node()->rblinks_.c_[1].x_) & 1) == 0;
}

template <typename T>
inline bool rbnodeptr<T>::find_child(T* n) const {
    return x_ && node()->rblinks_.c_[1].node() == n;
}

template <typename T>
inline rbnodeptr<T>& rbnodeptr<T>::load_color() {
    assert(!red());
    rbnodeptr<T> p;
    if (x_ && (p = black_parent()) && p.child(p.find_child(node())).red())
        x_ |= 1;
    return *this;
}

template <typename T>
inline rbnodeptr<T> rbnodeptr<T>::set_child(bool isright, rbnodeptr<T> x, T*& root) const {
    if (x_)
    child(isright) = x;
    else
    root = x.node();
    return x;
}

template <typename T>
inline T*& rbnodeptr<T>::parent() const {
    return node()->rblinks_.p_;
}

template <typename T>
inline rbnodeptr<T> rbnodeptr<T>::black_parent() const {
    return rbnodeptr<T>(node()->rblinks_.p_, false);
}

template <typename T>
inline bool rbnodeptr<T>::red() const {
    return x_ & 1;
}

template <typename T>
inline rbnodeptr<T> rbnodeptr<T>::change_color(bool color) const {
    return rbnodeptr<T>(color ? x_ | 1 : x_ & ~uintptr_t(1));
}

template <typename T>
inline rbnodeptr<T> rbnodeptr<T>::reverse_color() const {
    return rbnodeptr<T>(x_ ^ 1);
}

template <typename T>
inline rbnodeptr<T> rbnodeptr<T>::rotate(bool side) const {
    rbaccount(rotation);
    rbnodeptr<T> x = child(!side);
    // XXX no need to track rotations if using boundary nodes
    // increment the nodeversions of these nodes
    // node()->inc_nodeversion();
    // x.node()->inc_nodeversion();
    // if (x.child(side)) x.child(side).node()->inc_nodeversion();
    // perform the rotation
    if ((child(!side) = x.child(side)))
        x.child(side).parent() = node();
    bool old_color = red();
    x.child(side) = change_color(true);
    x.parent() = parent();
    parent() = x.node();
    return x.change_color(old_color);
}

template <typename T>
inline rbnodeptr<T> rbnodeptr<T>::flip() const {
    rbaccount(flip);
    child(false) = child(false).reverse_color();
    child(true) = child(true).reverse_color();
    return reverse_color();
}

namespace rbpriv {
template <typename Compare>
inline rbcompare<Compare>::rbcompare(const Compare& compare)
    : Compare(compare) {
}

template <typename Compare> template <typename T>
inline int rbcompare<Compare>::node_compare(const T& a, const T& b) const {
    int cmp = this->operator()(a, b);
    return cmp ? cmp : rbpriv::default_compare(&a, &b);
}

template <typename Compare> template <typename A, typename B>
inline int rbcompare<Compare>::compare(const A& a, const B& b) const {
    return this->operator()(a, b);
}

template <typename T, typename C>
inline rbrep<T, C>::rbrep(const C &compare) : rbcompare_type(compare) {
    this->root_ = this->limit_[0] = this->limit_[1] = 0;
}

template <typename Compare>
inline Compare& rbcompare<Compare>::get_compare() const {
    return *const_cast<Compare*>(static_cast<const Compare*>(this));
}

} // namespace rbpriv

// RBTREE FUNCTION DEFINITIONS
template <typename T, typename C>
inline rbtree<T, C>::rbtree(const value_compare &compare)
    : r_(compare), treeversion_(Sto::initialized_tid()) {
}

template <typename T, typename C>
rbtree<T, C>::~rbtree() {
}

template <typename T, typename C>
void rbtree<T, C>::insert_commit(T* x, rbnodeptr<T> p, bool side) {
    // link in new node; it's red
    x->rblinks_.p_ = p.node();
    x->rblinks_.c_[0] = x->rblinks_.c_[1] = rbnodeptr<T>(0, false);

    // maybe set limits
    if (p) {
        p.child(side) = rbnodeptr<T>(x, true);
        if (p.node() == r_.limit_[side])
            r_.limit_[side] = x;
    } else
        r_.root_ = r_.limit_[0] = r_.limit_[1] = x;

    // flip up the tree
    // invariant: we are looking at the `side` of `p`
    while (p.red()) {
        rbnodeptr<T> gp = p.black_parent(), z;
        if (gp.child(0).red() && gp.child(1).red()) {
            z = gp.flip();
            p = gp.black_parent().load_color();
        } else {
            bool gpside = gp.find_child(p.node());
            if (gpside != side) {
                gp.child(gpside) = p.rotate(gpside);
            } 
            z = gp.rotate(!gpside); 
            p = z.black_parent();
        }
        side = p.find_child(gp.node());
        p.set_child(side, z, r_.root_);
    }
}

template <typename T, typename C>
rbnodeptr<T> rbtree<T, C>::insert(reference x) {
    rbaccount(insert);

    // find insertion point
    rbnodeptr<T> p(r_.root_, false);
    bool side = false;
    while (p && (side = r_.node_compare(x, *p.node()) > 0,
                 p.child(side)))
        p = p.child(side);

    insert_commit(&x, p, side);
    return p;
}

template <typename T, typename C>
T* rbtree<T, C>::delete_node(T* victim_node, T* succ) {
    using std::swap;
    // find the node's color
    rbnodeptr<T> victim(victim_node, false);
    rbnodeptr<T> p = victim.black_parent();
    bool side = p.find_child(victim_node);

    // swap with successor if necessary
    if (victim.child(0) && victim.child(1)) {
        if (!succ)
            for (succ = victim.child(true).node();
                 succ->rblinks_.c_[0];
                 succ = succ->rblinks_.c_[0].node())
                /* nada */;
        rbnodeptr<T> succ_p = rbnodeptr<T>(succ->rblinks_.p_, false);
        bool sside = succ == succ_p.child(true).node();
        if (p)
            p.child(side) = rbnodeptr<T>(succ, p.child(side).red());
        else
            r_.root_ = succ;
        swap(succ->rblinks_, victim.node()->rblinks_);
        if (sside)
            succ->rblinks_.c_[sside] = victim.change_color(succ->rblinks_.c_[sside].red());
        succ->rblinks_.c_[0].parent() = succ->rblinks_.c_[1].parent() = succ;
        p = victim.black_parent();
        side = sside;
    } else
        succ = nullptr;

    // splice out victim
    bool active = !victim.child(false);
    rbnodeptr<T> x = victim.child(active);
    bool color = p && p.child(side).red();
    p.set_child(side, x, r_.root_);
    if (x)
        x.parent() = p.node();

    // maybe set limits
    for (int i = 0; i != 2; ++i)
        if (victim.node() == this->r_.limit_[i]) {
            rbnodeptr<T> b = (x ? x : p);
            while (b && b.child(i))
                b = b.child(i);
            this->r_.limit_[i] = b.node();
        }

    if (!color)
        delete_node_fixup(p, side);
   
    // return parent node to increment nodeversion
    return p.node();
}

template <typename T, typename C>
void rbtree<T, C>::delete_node_fixup(rbnodeptr<T> p, bool side) {
    while (p && !p.child(0).red() && !p.child(1).red()
           && !p.child(!side).child(0).red()
           && !p.child(!side).child(1).red()) {
        p.child(!side) = p.child(!side).change_color(true);
        T* node = p.node();
        p = p.black_parent();
        side = p.find_child(node);
    }

    if (p && !p.child(side).red()) {
        rbnodeptr<T> gp = p.black_parent();
        bool gpside = gp.find_child(p.node());

        if (p.child(!side).red()) {
            // invariant: p is black (b/c one of its children is red)
            gp.set_child(gpside, p.rotate(side), r_.root_);
            gp = p.black_parent(); // p is now further down the tree
            gpside = side;         // (since we rotated in that direction)
        }

        rbnodeptr<T> w = p.child(!side);
        if (!w.child(0).red() && !w.child(1).red()) {
            p.child(!side) = w.change_color(true);
            p = p.change_color(false);
        } else {
            if (!w.child(!side).red()) {
                p.child(!side) = w.rotate(!side);
            }
            bool gpside = gp.find_child(p.node());
            if (gp)
                p = gp.child(gpside); // fetch correct color for `p`
            p = p.rotate(side);
            p.child(0) = p.child(0).change_color(false);
            p.child(1) = p.child(1).change_color(false);
        }
        gp.set_child(gpside, p, r_.root_);
    } else if (p)
        p.child(side) = p.child(side).change_color(false);
}

template <typename T, typename C>
inline T* rbtree<T, C>::root() {
    return r_.root_;
}

// Return a pair of node, bool: if bool is true, then the node is the found node, 
// else if bool is false the node is the parent of the absent read. If (null, false), we have
// an empty tree
template <typename T, typename C> template <typename K, typename Comp>
inline std::tuple<T*, typename rbtree<T, C>::Version, bool,
       typename rbtree<T, C>::boundaries_type>
rbtree<T, C>::find_any(const K& key, Comp comp) const {

    rbnodeptr<T> n(r_.root_, false);
    rbnodeptr<T> p(nullptr, false);

    T* lhs = r_.limit_[0];
    T* rhs = r_.limit_[1];
    boundaries_type boundary = std::make_pair(std::make_tuple(lhs, lhs ? lhs->nodeversion() : 0),
                    std::make_tuple(rhs, rhs ? rhs->nodeversion() : 0));

    while (n.node()) {
        int cmp = comp.compare(key, *n.node());
        if (cmp == 0)
            break;

        // narrow down to find the boundary nodes
        // update the LEFT boundary when going RIGHT, and vice versa
        if (cmp > 0) {
            T* nb = n.node();
            boundary.first = std::make_tuple(nb, nb->nodeversion());
        } else {
            T* nb = n.node();
            boundary.second = std::make_tuple(nb, nb->nodeversion());
        }
        p = n;
        n = n.node()->rblinks_.c_[cmp > 0];
    }

    bool found = (n.node() != nullptr);
    T* retnode = found ? n.node() : p.node();
    Version retver = retnode ? retnode->version() : treeversion_;

    return std::make_tuple(retnode, retver, found, boundary);
}

template <typename T, typename C> template <typename K, typename Comp>
inline std::tuple<rbnodeptr<T>, bool> rbtree<T, C>::find_or_parent(const K& key, Comp comp) const {
    rbnodeptr<T> n(r_.root_, false);
    rbnodeptr<T> p(nullptr, false);

    while (n.node()) {
        int cmp = comp.compare(key, *n.node());
        if (cmp == 0)
            break;

        p = n;
        n = n.node()->rblinks_.c_[cmp > 0];
    }

    bool found = (n.node() != nullptr);

    return std::make_tuple(found ? n : p, found);
}

template <typename K, typename T>
class rbpair;

template <typename T, typename C> template <typename K, typename Comp>
inline std::tuple<T*, typename rbtree<T, C>::Version, bool,
       typename rbtree<T, C>::boundaries_type, typename rbtree<T, C>::node_info_type>
rbtree<T, C>::find_insert(K& key, Comp comp) {
    // lookup part, almost identical to find_any()
    rbnodeptr<T> n(r_.root_, false);
    rbnodeptr<T> p(nullptr, false);

    T* lhs = r_.limit_[0];
    T* rhs = r_.limit_[1];
    boundaries_type boundary = std::make_pair(std::make_tuple(lhs, lhs ? lhs->nodeversion() : 0),
                    std::make_tuple(rhs, rhs ? rhs->nodeversion() : 0));

    int cmp = 0;
    while (n.node()) {
        cmp = comp.compare(key, *n.node());
        if (cmp == 0)
            break;
        if (cmp > 0) {
            T* nb = n.node();
            boundary.first = std::make_tuple(nb, nb->nodeversion());
        } else {
            T* nb = n.node();
            boundary.second = std::make_tuple(nb, nb->nodeversion());
        }
        p = n;
        n = n.node()->rblinks_.c_[cmp > 0];
    }

    bool found = (n.node() != nullptr);
    T* retnode = n.node();
    Version retver = retnode ? (found ? retnode->version() : 0) : 0;
    node_info_type parent = std::make_tuple(p.node(), p.node() ? p.node()->nodeversion() : 0);

    // perform the insertion if not found
    if (!found) {
        retnode = (T*)malloc(sizeof(T));
        new (retnode) T((rbpair<typename K::key_type, typename K::value_type>)key);
        retver = retnode->nodeversion();
        insert_commit(retnode, p, (cmp > 0));

        // no more inc_nodeversion; nodeversion updates now occur at commit time
    }

    return std::make_tuple(retnode, retver, found, boundary, parent);
}

template <typename T, typename C>
inline T* rbtree<T, C>::erase(T& node) {
    rbaccount(erase);
    T* ret = delete_node(&node, nullptr);
    return ret;
}

// RBNODEPTR FUNCTION DEFINITIONS
template <typename T>
void rbnodeptr<T>::output(std::ostream &s, int indent, T* highlight) const {
    if (!*this)
    s << "<empty>\n";
    else {
    if (child(0))
        child(0).output(s, indent + 2, highlight);
    s << std::setw(indent) << "" << (const T&) *node() << " @" << (void*) node() << (red() ? " red" : "");
    if (highlight && highlight == node())
        s << " ******  p@" << (void*) node()->rblinks_.p_ << "\n";
    else
        s << "\n";
    if (child(1))
        child(1).output(s, indent + 2, highlight);
    }
}

template <typename T, typename C>
std::ostream &operator<<(std::ostream &s, const rbtree<T, C> &tree) {
    rbnodeptr<T>(tree.r_.root_, false).output(s, 0, 0);
    return s;
}

template <typename T>
size_t rbnodeptr<T>::size() const {
    return 1 + (child(false) ? child(false).size() : 0)
    + (child(true) ? child(true).size() : 0);
}

template <typename T, typename C>
inline bool rbtree<T, C>::empty() const {
    return !r_.root_;
}

template <typename T, typename C>
inline size_t rbtree<T, C>::size() const {
    return r_.root_ ? rbnodeptr<T>(r_.root_, false).size() : 0;
}

#define rbcheck_assert(x) do { if (!(x)) { rbnodeptr<T>(root, false).output(std::cerr, 0, node()); assert(x); } } while (0)

template <typename T> template <typename C>
void rbnodeptr<T>::check(T* parent, int this_black_height, int& black_height,
                         T* root, T* begin, T* end, C& compare) const {
    rbcheck_assert(node()->rblinks_.p_ == parent);
    if (parent) {
        int cmp = compare(*node(), *parent);
        if (parent->rblinks_.c_[0].node() == node())
            rbcheck_assert(cmp < 0 || (cmp == 0 && node() < parent));
        else
            rbcheck_assert(cmp > 0 || (cmp == 0 && node() > parent));
    }
    if (red())
        rbcheck_assert(!child(false).red() && !child(true).red());
    else {
        //rbcheck_assert(child(false).red() || !child(true).red()); /* LLRB */
        ++this_black_height;
    }
    if (begin && !child(false))
        assert(begin == node());
    if (end && !child(true))
        assert(end == node());
    for (int i = 0; i < 2; ++i)
        if (child(i))
            child(i).check(node(), this_black_height, black_height,
                           root, i ? 0 : begin, i ? end : 0, compare);
        else if (black_height == -1)
            black_height = this_black_height;
        else
            assert(black_height == this_black_height);
}

#undef rbcheck_assert

template <typename T, typename C>
int rbtree<T, C>::check() const {
    int black_height = -1;
    if (r_.root_)
        rbnodeptr<T>(r_.root_, false).check(0, 0, black_height, r_.root_,
                                            r_.limit_[0], r_.limit_[1], r_.get_compare());
    else
        assert(!r_.limit_[0] && !r_.limit_[1]);
    return black_height;
}

// RBALGORITHMS FUNCTION DEFINITIONS
template <typename T>
inline T* rbalgorithms<T>::edge_node(T* n, bool forward) {
    while (n->rblinks_.c_[forward])
        n = n->rblinks_.c_[forward].node();
    return n;
}

template <typename T>
inline T* rbalgorithms<T>::step_node(T* n, bool forward) {
    if (n->rblinks_.c_[forward])
        n = edge_node(n->rblinks_.c_[forward].node(), !forward);
    else {
        T* prev;
        do {
            prev = n;
            n = n->rblinks_.p_;
        } while (n && n->rblinks_.c_[!forward].node() != prev);
    }
    return n;
}

template <typename T>
inline T* rbalgorithms<T>::next_node(T* n) {
    return step_node(n, true);
}

template <typename T>
inline T* rbalgorithms<T>::prev_node(T* n) {
    return step_node(n, false);
}

template <typename T>
inline T* rbalgorithms<T>::prev_node(T* n, T* last) {
    return n ? step_node(n, false) : last;
}
