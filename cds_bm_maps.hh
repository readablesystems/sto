#pragma once

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
#include <vector> 
#include <functional> 

#include "Hashtable.hh" 
#include "CuckooHashMapIE.hh"
#include "CuckooHashMapKF.hh"
#include "CuckooHashMapWK.hh"
#include "CuckooHashMapNA2.hh"
#include "CuckooHashMapNT.hh"
#include "city_hasher.hh"
#include "cds_benchmarks.hh"
#include "cuckoohelpers.hh"

// types of map_operations available
enum map_op {insert, erase, find};
std::array<map_op, 3> map_ops_array = {insert, erase, find};

template <typename DS> struct CDSMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    CDSMapHarness() {};
    void init() { v_ = new DS(); }
    bool insert(key_type k, mapped_type v) { return v_->insert(k, v); }
    bool erase(key_type k) { return v_->erase(k); }
    bool find(key_type k) { return v_->contains(k); }
    void init_insert(key_type k, mapped_type v) { v_->insert(k, v); }
    void cleanup() { delete v_;}
private:
    DS* v_;
};

template <typename DS> struct STOMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    STOMapHarness() {};
    void init() { v_ = new DS(); }
    bool insert(key_type k, mapped_type v) { return v_->transInsert(k, v); }
    bool erase(key_type k) { return v_->transDelete(k); }
    bool find(key_type k) { mapped_type ret; return v_->transGet(k, ret); }
    void init_insert(key_type k, mapped_type v) { v_->nontrans_insert(k, v); }
    void cleanup() { delete v_; }
private:
    DS* v_;
};

template <typename DS> struct CuckooHashMapHarness { 
    typedef typename DS::mapped_type mapped_type;
    typedef typename DS::key_type key_type;
public:
    CuckooHashMapHarness() {};
    void init() { v_ = new DS(); }
    bool insert(key_type k, mapped_type v) { return v_->insert(k, v); }
    bool erase(key_type k) { return v_->erase(k); }
    bool find(key_type k) { mapped_type v; return v_->find(k,v); }
    void init_insert(key_type k, mapped_type v) { 
        Sto::start_transaction();
        v_->insert(k, v); 
        assert(Sto::try_commit());
    }
    void cleanup() { delete v_; }
private:
    DS* v_;
};

/* 
 * Hashtable Templates
 */
// STO-CHAINING
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,true,1000000>> :
    public STOMapHarness<Hashtable<K,T,true,1000000>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,false,1000000>> :
    public STOMapHarness<Hashtable<K,T,false,1000000>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,true,125000>> :
    public STOMapHarness<Hashtable<K,T,true,125000>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,false,125000>> :
    public STOMapHarness<Hashtable<K,T,false,125000>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,true,10000>> :
    public STOMapHarness<Hashtable<K,T,true,10000>>{};
template <typename K, typename T> struct DatatypeHarness<Hashtable<K,T,false,10000>> :
    public STOMapHarness<Hashtable<K,T,false,10000>>{};

// STO-CUCKOO KF
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,1000000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,1000000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,125000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,125000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,10000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,10000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,1000000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,1000000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,125000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,125000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,10000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,10000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,1000000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,1000000,false, 15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,125000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,125000,false, 15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapKF<K,T,10000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapKF<K,T,10000,false, 15>>{};

// STO-CUCKOO IE
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,1000000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,1000000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,125000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,125000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,10000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,10000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,1000000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,1000000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,125000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,125000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,10000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,10000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,1000000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,1000000,false, 15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,125000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,125000,false, 15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapIE<K,T,10000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapIE<K,T,10000,false, 15>>{};

// STO-CUCKOO WK
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapWK<K,T,1000000,false>> :
    public CuckooHashMapHarness<CuckooHashMapWK<K,T,1000000,false>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapWK<K,T,125000,false>> :
    public CuckooHashMapHarness<CuckooHashMapWK<K,T,125000,false>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapWK<K,T,10000,false>> :
    public CuckooHashMapHarness<CuckooHashMapWK<K,T,10000,false>>{};

// STO-CUCKOO NOALLOC
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,1000000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,1000000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,125000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,125000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,10000,false, 5>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,10000,false, 5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,1000000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,1000000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,125000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,125000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,10000,false, 10>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,10000,false, 10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,1000000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,1000000,false, 15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,125000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,125000,false, 15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNA<K,T,10000,false, 15>> :
    public CuckooHashMapHarness<CuckooHashMapNA<K,T,10000,false, 15>>{};

// NT CUCKOO
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,1000000,5>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,1000000,5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,125000,5>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,125000,5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,10000,5>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,10000,5>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,1000000,10>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,1000000,10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,125000,10>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,125000,10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,10000,10>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,10000,10>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,1000000,15>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,1000000,15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,125000,15>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,125000,15>>{};
template <typename K, typename T> struct DatatypeHarness<CuckooHashMapNT<K,T,10000,15>> :
    public CuckooHashMapHarness<CuckooHashMapNT<K,T,10000,15>>{};

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
    DatatypeHarness() {};
    void init() { v_ = new DS(1000000, 5);}
    bool insert(key_type k, mapped_type v) { return v_->insert(k, v); }
    bool erase(key_type k) { return v_->erase(k); }
    bool find(key_type k) { return v_->contains(k); }
    void init_insert(key_type k, mapped_type v) { v_->insert(k, v); }
    void cleanup() { delete v_; }
private:
    DS* v_;
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
    typedef typename DH::mapped_type mapped_type;
    typedef typename DH::key_type key_type;
public:
    DHMapTest(int p_insert, int p_erase, size_t size, int fullness) : 
        p_insert_(p_insert), p_erase_(p_erase), size_(size), fullness_(fullness), opdist_(0,99) {};

    void initialize(size_t) {
        v_.init();
        for (unsigned i = 0; i < (size_*fullness_)/2; ++i) {
            v_.init_insert(i,i);
        }
        keydist_ = std::uniform_int_distribution<long>(0,size_*fullness_*1.5);
    }

    void cleanup() {
        v_.cleanup();
    }
    
    inline void do_map_op(int me, Rand& transgen, int num_ops) {
        map_op my_op;
        int numOps = num_ops ? opdist_(transgen) % num_ops + 1 : 0;
        for (int j = 0; j < numOps; j++) {
            int p = opdist_(transgen);
            if (p > 100 - p_erase_) my_op = erase;
            else if (p < p_insert_) my_op = insert;
            else my_op = find;
            int key = keydist_(transgen); 
            switch (my_op) {
                case erase:
                    if (v_.erase(key)) {
                        //std::cout << "erased! " << global_thread_ctrs[me].ke_erase << std::endl;
                        global_thread_ctrs[me].ke_erase++;
                    }
                    global_thread_ctrs[me].erase++;
                    break;
                case insert:
                    if (v_.insert(key,me)) {
                        //std::cout << "inserted! " << global_thread_ctrs[me].ke_insert << std::endl;
                        global_thread_ctrs[me].ke_insert++;
                    }
                    global_thread_ctrs[me].insert++;
                    break;
                case find:
                    if (v_.find(key)) global_thread_ctrs[me].ke_find++;
                    global_thread_ctrs[me].find++;
                    break;
            }
        }
        /*
        fprintf(global_verbose_stats_file, "\n");
        int total_ke_insert, total_ke_find, total_ke_erase, total_inserts, total_find, total_erase;
        total_ke_insert = total_ke_find = total_ke_erase = total_inserts = total_find = total_erase = 0;
        for (int i = 0; i < 8; ++i) {
            total_ke_insert += global_thread_ctrs[i].ke_insert;
            total_ke_find += global_thread_ctrs[i].ke_find;
            total_ke_erase += global_thread_ctrs[i].ke_erase;
            total_inserts += global_thread_ctrs[i].insert;
            total_erase += global_thread_ctrs[i].erase;
            total_find += global_thread_ctrs[i].find;
        }
        fprintf(global_verbose_stats_file, "Success Inserts: %f%%\t Success Finds: %f%%\t Success Erase: %f%%\t\n", 
                100 - 100*(double)total_ke_insert/total_inserts,
                100 - 100*(double)total_ke_find/total_find,
                100*(double)total_ke_erase/total_erase);
        */

    }
protected:
    DH v_;
    int p_insert_;
    int p_erase_;
    size_t size_;
    int fullness_;
    std::uniform_int_distribution<long> keydist_;
    std::uniform_int_distribution<long> opdist_;
};

template <typename DH> class MapOpTest : public DHMapTest<DH> {
public:
    MapOpTest(int ds_type, size_t size, int fullness, int num_ops, int p_insert, int p_erase) : 
        DHMapTest<DH>(p_insert, p_erase, size, fullness), ds_type_(ds_type), num_ops_(num_ops) {};
    void run(int me) {
        Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);
        if (ds_type_ == STO) {
            for (int i = NTRANS; i > 0; --i) {
                Rand transgen_snap = transgen;
                while (1) {
                    Sto::start_transaction();
                    try {
                        this->do_map_op(me, transgen, num_ops_);
                        if (Sto::try_commit()) break;
                    } catch (Transaction::Abort e) {transgen = transgen_snap;}
                }
            }
        } else {
            for (int i = NTRANS; i > 0; --i) {
                this->do_map_op(me, transgen, num_ops_);
            }
        }
    }
private:
    int ds_type_;
    int num_ops_;
};


#define MAKE_MAP_TESTS(desc, test, key, val, size, fullness, ...) \
    {desc, "STO Nonopaque Hashtable", new test<DatatypeHarness<Hashtable<key,val,false,size>>>(STO, size, fullness,  ## __VA_ARGS__)},                                  \
    {desc, "STO IE CuckooMap", new test<DatatypeHarness<CuckooHashMapIE<key,val,size, false, fullness>>>(STO, size, fullness, ## __VA_ARGS__)}, \
    {desc, "STO KF CuckooMap", new test<DatatypeHarness<CuckooHashMapKF<key,val,size, false, fullness>>>(STO, size, fullness, ## __VA_ARGS__)},                \
    {desc, "STO NA CuckooMap", new test<DatatypeHarness<CuckooHashMapNA<key,val,size, false, fullness>>>(STO, size, fullness, ## __VA_ARGS__)} ,\
    {desc, "CuckooMapNT", new test<DatatypeHarness<CuckooHashMapNT<key, val, size, fullness>>>(CDS, size, fullness, ## __VA_ARGS__)}, 
    //{desc, "STO WK CuckooMap", new test<DatatypeHarness<CuckooHashMapWK<key,val,size, false>>>(STO, size, ## __VA_ARGS__)}, 
    //{desc, "MichaelHashMap", new test<DatatypeHarness<MICHAELMAP(key,val)>>(CDS, ## __VA_ARGS__)},        
    //{desc, "STO Opaque Hashtable", new test<DatatypeHarness<Hashtable<key,val,true,size>>>(STO, ## __VA_ARGS__)},                                  
    //{desc, "SplitListMap", new test<DatatypeHarness<cds::container::SplitListMap<cds::gc::HP,key,val,map_traits<key>>>>(CDS, ## __VA_ARGS__)},
    //{desc, "FeldmanHashMap", new test<DatatypeHarness<cds::container::FeldmanHashMap<cds::gc::HP,key,val>>>(CDS, ## __VA_ARGS__)},                    
    //{desc, "StripedMap", new test<DatatypeHarness<cds::container::StripedMap<std::map<key,val>>>>(CDS, ## __VA_ARGS__)}, 
    //{desc, "SkipListMap", new test<DatatypeHarness<cds::container::SkipListMap<cds::gc::HP,key,val>>>(CDS, ## __VA_ARGS__)}, 
    //{desc, "CuckooMap", new test<DatatypeHarness<cds::container::CuckooMap<key,val,cuckoo_traits<key>>>>(CDS, ## __VA_ARGS__)},                

std::vector<Test> make_map_tests() {
    return {
        //MAKE_MAP_TESTS("HM125K:F34,I33,E33", MapOpTest, int, int, 125000, 10, 1, 33, 33)
        //MAKE_MAP_TESTS("HM125K:F90,I5,E5", MapOpTest, int, int, 125000, 10, 1, 5, 5)
        //MAKE_MAP_TESTS("HM1M:F34,I33,E33", MapOpTest, int, int, 1000000, 10, 1, 33, 33)
        //MAKE_MAP_TESTS("HM1M:F90,I5,E5", MapOpTest, int, int, 1000000, 10, 1, 5, 5)
        //MAKE_MAP_TESTS("HM10K:F34,I33,E33", MapOpTest, int, int, 10000, 10, 1, 33, 33)
        //MAKE_MAP_TESTS("HM10K:F90,I5,E5", MapOpTest, int, int, 10000, 10, 1, 5, 5)
        MAKE_MAP_TESTS("HM125K:F34,I33,E33", MapOpTest, int, int, 125000, 15, 1, 33, 33)
        MAKE_MAP_TESTS("HM125K:F90,I5,E5", MapOpTest, int, int, 125000, 15, 1, 5, 5)
        MAKE_MAP_TESTS("HM1M:F34,I33,E33", MapOpTest, int, int, 1000000, 15, 1, 33, 33)
        MAKE_MAP_TESTS("HM1M:F90,I5,E5", MapOpTest, int, int, 1000000, 15, 1, 5, 5)
        MAKE_MAP_TESTS("HM10K:F34,I33,E33", MapOpTest, int, int, 10000, 15, 1, 33, 33)
        MAKE_MAP_TESTS("HM10K:F90,I5,E5", MapOpTest, int, int, 10000, 15, 1, 5, 5)
        //MAKE_MAP_TESTS("HM125K:F34,I33,E33", MapOpTest, int, int, 125000, 5, 1, 33, 33)
        //MAKE_MAP_TESTS("HM125K:F90,I5,E5", MapOpTest, int, int, 125000, 5, 1, 5, 5)
        //MAKE_MAP_TESTS("HM1M:F34,I33,E33", MapOpTest, int, int, 1000000, 5, 1, 33, 33)
        //MAKE_MAP_TESTS("HM1M:F90,I5,E5", MapOpTest, int, int, 1000000, 5, 1, 5, 5)
        //MAKE_MAP_TESTS("HM10K:F34,I33,E33", MapOpTest, int, int, 10000, 5, 1, 33, 33)
        //MAKE_MAP_TESTS("HM10K:F90,I5,E5", MapOpTest, int, int, 10000, 5, 1, 5, 5)
        //MAKE_MAP_TESTS("HM1MMultiOp:F34,I33,E33", MapOpTest, int, int, 1000000, 5, 33, 33)
        //MAKE_MAP_TESTS("HM1MMultiOp:F90,I5,E5", MapOpTest, int, int, 1000000, 5, 5, 5)
    };
}
int num_maps = 5;
