#pragma once

#include "cds_benchmarks.hh"

#include <cds/container/michael_list_hp.h>
#include <cds/container/michael_set.h>
#include <cds/container/split_list_map.h>
#include <cds/container/cuckoo_map.h>
#include <cds/container/michael_map.h>
#include <cds/container/michael_kvlist_hp.h>
#include <cds/container/feldman_hashmap_hp.h>
#include <cds/container/skip_list_map_hp.h>
#include <cds/container/striped_map.h>
#include <cds/container/striped_map/std_map.h>
#include <cds/details/binary_functor_wrapper.h>
#include <cds/opt/hash.h> 
#include <cds/algo/atomic.h> 

#include <map> 
#include <functional> 

#include "Hashtable.hh" 

// value types
#define RANDOM_VALS 10
#define DECREASING_VALS 11

// types of map_operations available
enum map_op {insert, erase, find};
std::array<map_op, 3> map_ops_array = {insert, erase, find};

template <typename DS> struct CDSMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    CDSMapHarness() {};
    bool insert(key_type k, mapped_type v) { return v_.insert(k, v); }
    bool erase(key_type k) { return v_.erase(k); }
    bool find(key_type k) { return v_.contains(k); }
    void init_insert(key_type k, mapped_type v) { assert(v_.insert(k, v)); }
    bool cleanup_erase() { v_.clear(); return false; }
private:
    DS v_;
};

template <typename DS> struct STOMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    bool insert(key_type k, mapped_type v) { return v_.transInsert(k, v); }
    bool erase(key_type k) { return v_.transDelete(k); }
    bool find(key_type k) { mapped_type ret; return v_.transGet(k, ret); }
    void init_insert(key_type k, mapped_type v) { assert(v_.nontrans_insert(k, v)); }
    bool cleanup_erase() { 
        v_.nontrans_clear();
        return false; 
    }
private:
    DS v_;
};

/* 
 * Hashtable Templates
 */


template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,true>> : 
    public STOMapHarness<Hashtable<K,T,true>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,false>> : 
    public STOMapHarness<Hashtable<K,T,false>>{};

template <typename K>
struct hash1 { size_t operator()(K const& k) const { return std::hash<K>()( k ); } };
template <typename K>
struct hash2: private hash1<K> {
    size_t operator()(K const& k) const
    {
        size_t h = ~( hash1<K>::operator()(k));
        return ~h + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
};
template <typename K>
struct cuckoo_traits: public cds::container::cuckoo::traits
{
    typedef std::equal_to< K > equal_to;
    typedef cds::opt::hash_tuple< hash1<K>, hash2<K> >  hash;
    static bool const store_hash = true;
};
template <typename K, typename T> struct DatatypeHarness<cds::container::CuckooMap<K,T,cuckoo_traits<K>>> :
    public CDSMapHarness<cds::container::CuckooMap<K,T,cuckoo_traits<K>>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::FeldmanHashMap<cds::gc::HP,K,T>> :
    public CDSMapHarness<cds::container::FeldmanHashMap<cds::gc::HP,K,T>>{};

#define MICHAELMAP(K,T) \
    cds::container::MichaelHashMap< \
        cds::gc::HP,  \
        cds::container::MichaelKVList<  \
            cds::gc::HP, K, T, \
            typename cds::container::michael_list::make_traits<cds::container::opt::less<std::less<K>>>::type \
        >, \
        typename cds::container::michael_map::make_traits<\
            cds::opt::hash<cds::opt::v::hash<K>>, \
            cds::opt::item_counter<cds::atomicity::item_counter>, \
            cds::opt::allocator<CDS_DEFAULT_ALLOCATOR>\
        >::type\
    >

template <typename K, typename T> struct DatatypeHarness<MICHAELMAP(K,T)> {
    typedef MICHAELMAP(K,T) DS;
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    DatatypeHarness() : v_(100000, 5) {};
    bool insert(key_type k, mapped_type v) { return v_.insert(k, v); }
    bool erase(key_type k) { return v_.erase(k); }
    bool find(key_type k) { return v_.contains(k); }
    void init_insert(key_type k, mapped_type v) { assert(v_.insert(k, v)); }
    bool cleanup_erase() { v_.clear(); return false; }
private:
    DS v_;
};
template <typename K, typename T> struct DatatypeHarness<cds::container::SkipListMap<cds::gc::HP,K,T>> :
    public CDSMapHarness<cds::container::SkipListMap<cds::gc::HP,K,T>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::StripedMap<std::map<K,T>>> :
    public CDSMapHarness<cds::container::StripedMap<std::map<K,T>>>{};

template <typename K>
struct map_traits: public cds::container::split_list::traits
{
    typedef cds::container::michael_list_tag   ordered_list    ;   // what type of ordered list we want to use
    typedef std::hash<K>         hash            ;   // hash functor for the key stored in split-list map
    // Type traits for our MichaelList class
    struct ordered_list_traits: public cds::container::michael_list::traits
    {
    typedef std::less<K> less   ;   // use our std::less predicate as comparator to order list nodes
    };
};
template <typename K, typename T> struct DatatypeHarness<
        cds::container::SplitListMap<cds::gc::HP,K,T,map_traits<K>>> :
    public CDSMapHarness<cds::container::SplitListMap<cds::gc::HP,K,T,map_traits<K>>>{};

/*
 * Test interfaces and templates.
 */
template <typename DH> class DHMapTest : public GenericTest {
public:
    DHMapTest() {};
    void initialize(size_t init_sz) {
        for (unsigned i = 0; i < init_sz; ++i) {
            v_.init_insert(i,i); 
        }
    }
    
    void cleanup() {
        while (v_.cleanup_erase()){/*keep popping*/}
    }

    inline void do_map_op(map_op map_op, int key, int val) {
        switch (map_op) {
            case insert: v_.insert(key,val); break;
            case erase: v_.erase(key); break;
            case find: v_.find(key); break;
            default: assert(0);
        }
    }
    inline void inc_ctrs(map_op map_op, int me) {
        switch(map_op) {
            case insert: global_thread_ctrs[me].insert++; break;
            case erase: global_thread_ctrs[me].erase++; break;
            case find: global_thread_ctrs[me].find++; break;
            default: assert(0);
        }
    }

protected:
    DH v_;
};

template <typename DH> class RandomSingleOpTest : public DHMapTest<DH> {
public:
    RandomSingleOpTest(int ds_type) : ds_type_(ds_type) {};
    void run(int me) {
        map_op my_map_op;
        for (int i = NTRANS; i > 0; --i) {
            if (ds_type_ == STO) {
                while (1) {
                    Sto::start_transaction();
                    try {
                        my_map_op = map_ops_array[map_ops_array[i % 3]];
                        this->do_map_op(my_map_op, 
                            rand_vals[(i*me + i + 1) % arraysize(rand_vals)], 
                            rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {}
                }
                this->inc_ctrs(my_map_op, me);
            } else {
                my_map_op = map_ops_array[map_ops_array[i%3]];
                this->do_map_op(my_map_op, 
                    rand_vals[(i*me + i + 1) % arraysize(rand_vals)], 
                    rand_vals[(i*me + i) % arraysize(rand_vals)]);
                this->inc_ctrs(my_map_op, me);
            }
        }
    }
private:
    int ds_type_;
};
