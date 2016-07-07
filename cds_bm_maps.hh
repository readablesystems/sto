#pragma once

#include "cds_benchmarks.hh"
#include "cds/container/cuckoo_map.hh"
#include "cds/container/michael_map.hh"
#include "cds/container/feldman_hashmap_hp.hh"
#include "cds/container/skip_list_map.hh"
#include "cds/container/split_list_map.hh"
#include "cds/container/striped_map.hh"

// value types
#define RANDOM_VALS 10
#define DECREASING_VALS 11

// types of map_operations available
enum map_op {insert, remove, find};
std::array<map_op, 2> map_ops_array = {insert, remove, find};

template <typename DS> struct CDSMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    CDSMapHarness() {};
    bool insert(key_type k, value_type v) { return v_.insert(k, v); }
    bool remove(key_type k) { return erase(k); }
    void find(key_type k) { return v_.contains(k); }
    void init_insert(key_type k, value_type v) { assert(v_.insert(k, v)); }
    void init_remove() { v_.clear(); return false; }
private:
    DS v_;
};

template <typename DS> struct STOMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    bool insert(key_type k, value_type v) { return v_.transInsert(k, v); }
    bool remove(key_type k) { return transDelete(k); }
    void find(key_type k) { mapped_type ret; return v_.transGet(k, ret); }
    void init_insert(key_type k, value_type v) { assert(v_.nontrans_insert(k, v)); }
    void init_remove() { 
        for (it = v_.begin(); it < v_.end(); ++it) nontrans_remove(it.first);
        return false; 
    }
private:
    DS v_;
};

/* 
 * Hashtable Templates
 */
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,true>> : 
    public STOMapHarness<HashTable<K,T,true>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,false>> : 
    public STOMapHarness<HashTable<K,T,false>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::CuckooMap<K,T>> {
    public CDSMapHarness<cds::container::CuckooMap<K,T>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::FeldmanHashMap<K,T>> {
    public CDSMapHarness<cds::container::FeldmanHashMap<K,T>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::MichaelHashmap<K,T>> {
    public CDSMapHarness<cds::container::MichaelHashmap<K,T>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::SkipListMap<K,T>> {
    public CDSMapHarness<cds::container::SkipListMap<K,T>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::StripedMap<K,T>> {
    public CDSMapHarness<cds::container::StripedMap<K,T>>{};
template <typename K, typename T> struct DatatypeHarness<cds::container::SplitListMap<K,T>> {
    public CDSMapHarness<cds::container::SplitListMap<K,T>>{};

/*
 * Test interfaces and templates.
 */
template <typename DH> class DHMapTest : public GenericTest {
public:
    MapTest() {};
    void initialize(size_t init_sz) {
        for (unsigned i = 0; i < init_sz; ++i) {
            v_.init_push(i,i); 
        }
    }
    
    void cleanup() {
        while (v_.cleanup_pop()){/*keep popping*/}
    }

    inline void do_map_op(map_op map_op, int val) {
        switch (map_op) {
            case insert: v_.insert(); break;
            case remove: v_.remove(val); break;
            case find: v_.find(val); break;
            default: assert(0);
        }
    }
    inline void inc_ctrs(map_op map_op, int me) {
        switch(map_op) {
            case insert: global_thread_ctrs[me].insert++; break;
            case remove: global_thread_ctrs[me].remove++; break;
            case find: global_thread_ctrs[me].find++; break;
            default: assert(0);
        }
    }

protected:
    DH v_;
};

template <typename DH> class RandomSingleOpTest : public MapTest<DH> {
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
                        this->do_map_op(my_map_op, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {}
                }
                this->inc_ctrs(my_map_op, me);
            } else {
                my_map_op = map_ops_array[map_ops_array[i%3]];
                this->do_map_op(my_map_op, rand_vals[(i*me + i) % arraysize(rand_vals)]);
                this->inc_ctrs(my_map_op, me);
            }
        }
    }
private:
    int ds_type_;
};
