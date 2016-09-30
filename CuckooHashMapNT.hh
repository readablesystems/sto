#ifndef _CuckooHashMapNT_HH
#define _CuckooHashMapNT_HH

#include <atomic>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <vector>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cuckoohelpers.hh"

/*! CuckooHashMapNT is the hash table class. */
template <class Key, class T, unsigned Num_Buckets = 250000>
class CuckooHashMapNT {

    // Structs and functions used internally

    typedef enum {
        ok = 0,
        failure_too_slow = 1,
        failure_key_not_found = 2,
        failure_key_duplicated = 3,
        failure_space_not_enough = 4,
        failure_function_not_supported = 5,
        failure_table_full = 6,
        failure_under_expansion = 7,
        failure_key_moved = 8,
        failure_already_migrating_all = 9
    } cuckoo_status;


    struct rw_lock {
    private:
        TVersion num;

    public:
        rw_lock() {
        }

        inline size_t get_version() {
            return num.value();
        }

        inline void lock() {
#if LIBCUCKOO_DEBUG
            assert(!num.is_locked_here());
#endif
            num.lock();
        }

        inline void unlock() {
            num.set_version_unlock(num.value() + TransactionTid::increment_value);
        }

        inline bool try_lock() {
            return num.try_lock();
        }

        inline bool is_locked() const {
            return num.is_locked();
        }
    } __attribute__((aligned(64)));
    
    /* This is a hazard pointer, used to indicate which version of the
     * TableInfo is currently being used in the thread. Since
     * CuckooHashMapNT operations can run simultaneously in different
     * threads, this variable is thread local. Note that this variable
     * can be safely shared between different CuckooHashMapNT
     * instances, since multiple operations cannot occur
     * simultaneously in one thread. The hazard pointer variable
     * points to a pointer inside a global list of pointers, that each
     * map checks before deleting any old TableInfo pointers. */


    static __thread void** hazard_pointer_old;
    static __thread void** hazard_pointer_new;

    /* A GlobalHazardPointerList stores a list of pointers that cannot
     * be deleted by an expansion thread. Each thread gets its own
     * node in the list, whose data pointer it can modify without
     * contention. */
    class GlobalHazardPointerList {
        std::list<void*> hp_;
        std::mutex lock_;
    public:
        /* new_hazard_pointer creates and returns a new hazard pointer for
         * a thread. */
        void** new_hazard_pointer() {
            lock_.lock();
            hp_.emplace_back(nullptr);
            void** ret = &hp_.back();
            lock_.unlock();
            return ret;
        }

        /* delete_unused scans the list of hazard pointers, deleting
         * any pointers in old_pointers that aren't in this list.
         * If it does delete a pointer in old_pointers, it deletes
         * that node from the list. */
        template <class Ptr>
        void delete_unused(std::list<Ptr*>& old_pointers) {
            lock_.lock();
            auto it = old_pointers.begin();
            while (it != old_pointers.end()) {
                LIBCUCKOO_DBG("Hazard pointer %p\n", *it);
                bool deleteable = true;
                for (auto hpit = hp_.cbegin(); hpit != hp_.cend(); hpit++) {
                    if (*hpit == *it) {
                        deleteable = false;
                        break;
                    }
                }
                if (deleteable) {
                    LIBCUCKOO_DBG("Deleting %p\n", *it);
                    delete *it;
                    it = old_pointers.erase(it);
                } else {
                    it++;
                }
            }
            lock_.unlock();
        }
    };

    // As long as the thread_local hazard_pointer is static, which
    // means each template instantiation of a CuckooHashMapNT class
    // gets its own per-thread hazard pointer, then each template
    // instantiation of a CuckooHashMapNT class can get its own
    // global_hazard_pointers list, since different template
    // instantiations won't interfere with each other.
    static GlobalHazardPointerList global_hazard_pointers;

    /* check_hazard_pointers should be called before any public method
     * that loads a table snapshot. It checks that the thread local
     * hazard pointer pointer is not null, and gets a new pointer if
     * it is null. */
    static inline void check_hazard_pointers() {
        if (hazard_pointer_old == nullptr) {
            hazard_pointer_old = global_hazard_pointers.new_hazard_pointer();
        }
        if (hazard_pointer_new == nullptr) {
            hazard_pointer_new = global_hazard_pointers.new_hazard_pointer();
        }
    }

    /* Once a function is finished with a version of the table, it
     * calls unset_hazard_pointers so that the pointer can be freed if
     * it needs to. */
    static inline void unset_hazard_pointers() {
        *hazard_pointer_old = nullptr;
        *hazard_pointer_new = nullptr;
    }

    /* counterid stores the per-thread counter index of each
     * thread. */
    static __thread int counterid;

    /* check_counterid checks if the counterid has already been
     * determined. If not, it assigns a counterid to the current
     * thread by picking a random core. This should be called at the
     * beginning of any function that changes the number of elements
     * in the table. */
    static inline void check_counterid() {
        if (counterid < 0) {
            //counterid = rand() % kNumCores;
            counterid = numThreads.fetch_add(1,std::memory_order_relaxed) % kNumCores;
            //LIBCUCKOO_DBG("Counter id is %d", counterid);
        }
    }

    /* reserve_calc takes in a parameter specifying a certain number
     * of slots for a table and returns the smallest hashpower that
     * will hold n elements. */
    size_t reserve_calc(size_t n) {
        size_t new_hashpower = (size_t)ceil(log2((double)n / (double)SLOT_PER_BUCKET));
        assert(n <= hashsize(new_hashpower) * SLOT_PER_BUCKET);
        return new_hashpower;
    }

public:
    //! key_type is the type of keys.
    typedef Key               key_type;
    //! value_type is the type of key-value pairs.
    typedef std::pair<Key, T> value_type;
    //! mapped_type is the type of values.
    typedef T                 mapped_type;
    //! hasher is the type of the hash function.
    typedef std::hash<key_type> hasher;
    //! key_equal is the type of the equality predicate.
    typedef std::equal_to<key_type> key_equal;

    /*! The constructor creates a new hash table with enough space for
     * \p n elements. If the constructor fails, it will throw an
     * exception. */
    explicit CuckooHashMapNT(size_t n = Num_Buckets * SLOT_PER_BUCKET) {
        cuckoo_init(reserve_calc(n));
    }

    void initialize(size_t n = Num_Buckets * SLOT_PER_BUCKET) {
        cuckoo_init(reserve_calc(n));
    }
    /*! The destructor deletes any remaining table pointers managed by
     * the hash table, also destroying all remaining elements in the
     * table. */
    ~CuckooHashMapNT() {
        TableInfo *ti_old = table_info.load();
        TableInfo *ti_new = new_table_info.load();
        if (ti_old != nullptr) {
            delete ti_old;
        }
        if (ti_new != nullptr) {
            delete ti_new;
        }

        for (auto it = old_table_infos.cbegin(); it != old_table_infos.cend(); it++) {
            delete *it;
        }
    }

    /*! TODO: clear removes all the elements in the hash table, calling
     *  their destructors. */
    /*void clear() {
        check_hazard_pointers();
        expansion_lock.lock();
        TableInfo *ti = snapshot_and_lock_all();
        assert(ti != nullptr);
        cuckoo_clear(ti);
        unlock_all(ti);
        expansion_lock.unlock();
        unset_hazard_pointers();
    }*/

    /*! size returns the number of items currently in the hash table.
     * Since it doesn't lock the table, elements can be inserted
     * during the computation, so the result may not necessarily be
     * exact. */
    size_t size() {
        check_hazard_pointers();
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        size_t s1 = cuckoo_size(ti_old);
        size_t s2 = cuckoo_size(ti_new);
        LIBCUCKOO_DBG("Old table size %zu\n", s1);
        LIBCUCKOO_DBG("New table size %zu\n", s2);
        unset_hazard_pointers();
        return s1+s2;
    }

    size_t num_inserts() {
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        size_t inserts = 0;
        for (size_t i = 0; i < ti_old->num_inserts.size(); i++) {
            inserts += ti_old->num_inserts[i].num.load();
        }
        return inserts;
    }

    size_t num_deletes() {
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        size_t deletes = 0;
        for (size_t i = 0; i < ti_old->num_deletes.size(); i++) {
            deletes += ti_old->num_deletes[i].num.load();
        }
        return deletes;
    }

    /*! empty returns true if the table is empty. */
    bool empty() {
        return size() == 0;
    }

    /* undergoing_expansion returns true if there are currently two tables in use */
    bool undergoing_expansion() {
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        return (ti_new == nullptr);
    }
    /*! hashpower returns the hashpower of the table, which is log<SUB>2</SUB>(the
     * number of buckets). */
    size_t hashpower() {
        check_hazard_pointers();
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        size_t hashpower;
        if( ti_new != nullptr)
            hashpower = ti_new->hashpower_;
        else
            hashpower = ti_old->hashpower_;
        unset_hazard_pointers();
        return hashpower;
    }

    /*! bucket_count returns the number of buckets in the table. */
    size_t bucket_count() {
        check_hazard_pointers();
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        size_t buckets = hashsize(ti_old->hashpower_);
        unset_hazard_pointers();
        return buckets;
    }

    /*! load_factor returns the ratio of the number of items in the
     * table to the total number of available slots in the table. */
    float load_factor() {
        check_hazard_pointers();
        TableInfo *ti_old, *ti_new;
        snapshot_both_no_hazard(ti_old, ti_new);
        const float lf = cuckoo_loadfactor(ti_old);
        unset_hazard_pointers();
        return lf;
    }

    /*! find searches through the table for \p key, and stores
     * the associated value it finds in \p val. */
    bool find(const key_type& key, mapped_type& val) {
        check_hazard_pointers();
        size_t hv = hashed_key(key);
        size_t i1_o, i2_o, i1_n, i2_n;
        TableInfo *ti_old, *ti_new;
        cuckoo_status res;
    RETRY:
        snapshot_both_get_buckets(ti_old, ti_new, hv, i1_o, i2_o, i1_n, i2_n);
        
        res = find_one(ti_old, hv, key, val, i1_o, i2_o);

        // couldn't find key in bucket, and one of the buckets was moved to new table
        if (res == failure_key_moved) {
            if (ti_new == nullptr) { // the new table's pointer has already moved
                unset_hazard_pointers();
                LIBCUCKOO_DBG("ti_new is nullptr in failure_key_moved with ptr %p\n",ti_new);
                goto RETRY;
            }
            res = find_one(ti_new, hv, key, val, i1_n, i2_n);
            if( res == failure_key_moved ) {
                unset_hazard_pointers();
                goto RETRY;
            }

            assert( res == ok || res == failure_key_not_found);
        }
        unset_hazard_pointers(); 
        return (res == ok);
    }

    /*! This version of find does the same thing as the two-argument
     * version, except it returns the value it finds, throwing an \p
     * std::out_of_range exception if the key isn't in the table. */
    mapped_type find(const key_type& key) {
        mapped_type val;
        bool done = find(key, val);

        if (done) {
            return val;
        } else {
            throw std::out_of_range("key not found in table");
        }
    }

    /*! insert puts the given key-value pair into the table. It first
     * checks that \p key isn't already in the table, since the table
     * doesn't support duplicate keys. If the table is out of space,
     * insert will automatically expand until it can succeed. Note
     * that expansion can throw an exception, which insert will
     * propagate. */
    bool insert(const key_type& key, const mapped_type& val) {
        check_hazard_pointers();
        check_counterid();
        size_t hv = hashed_key(key);
        TableInfo *ti_old, *ti_new;
        size_t i1_o, i2_o, i1_n, i2_n; //bucket indices for old+new tables
        cuckoo_status res;
    RETRY:
        snapshot_both_get_buckets(ti_old, ti_new, hv, i1_o, i2_o, i1_n, i2_n);

        // if two tables exist, then no point in inserting into old one
        if (ti_new != nullptr) {
            res = failure_key_moved; 
        } else {
            res = insert_one(ti_old, hv, key, val, i1_o, i2_o);
        }

        // This is triggered only if we couldn't find the key in either
        // old table bucket and one of the buckets was moved
        // or this thread expanded the table (so didn't insert into key into new table yet)
        if (res == failure_key_moved || res == failure_table_full) {
            // all buckets have already been moved, and now old_table_ptr has the new one
            if (ti_new == nullptr) {
                unset_hazard_pointers();
                LIBCUCKOO_DBG("Table Info pointers have already swapped in insert");
                goto RETRY;
            }

            //migrate_something(ti_old, ti_new, i1_o, i2_o );

            // LIBCUCKOO_DBG("result is failure_key_moved or failure (too full)! with new pointer");
            res = insert_one(ti_new, hv, key, val, i1_n, i2_n);
            
            /*LIBCUCKOO_DBG("hashpower = %zu, %zu, hash_items = %zu,%zu, load factor = %.2f,%.2f), need to increase hashpower\n",
                      ti_old->hashpower_, ti_new->hashpower_, cuckoo_size(ti_old), cuckoo_size(ti_new),
                      cuckoo_loadfactor(ti_old), cuckoo_loadfactor(ti_new) );*/
            // key moved from new table, meaning that an even newer table was created
            if (res == failure_key_moved || res == failure_table_full) {
                LIBCUCKOO_DBG("Key already moved from new table, or already filled...that was fast, %d\n", res);
                unset_hazard_pointers();
                goto RETRY;
            }

            // Impossible case because of invariant with migrations
            /*if (res == failure) {
                LIBCUCKOO_DBG("hashpower = %zu, %zu, hash_items = %zu,%zu, load factor = %.2f,%.2f), need to increase hashpower\n",
                      ti_old->hashpower_, ti_new->hashpower_, cuckoo_size(ti_old), cuckoo_size(ti_new),
                      cuckoo_loadfactor(ti_old), cuckoo_loadfactor(ti_new) );
                exit(1);
            }*/

            assert(res == failure_key_duplicated || res == ok);
        }
        unset_hazard_pointers();
        return (res == ok);
    }

    /*! erase removes \p key and it's associated value from the table,
     * calling their destructors. If \p key is not there, it returns
     * false. */
    bool erase(const key_type& key) {
        check_hazard_pointers();
        check_counterid();
        size_t hv = hashed_key(key);
        TableInfo *ti_old, *ti_new;
        size_t i1_o, i2_o, i1_n, i2_n;
        cuckoo_status res;
    RETRY:
        snapshot_both_get_buckets(ti_old, ti_new, hv, i1_o, i2_o, i1_n, i2_n);

        if (ti_new != nullptr) {
            res = failure_key_moved;
        } else {
            res = delete_one(ti_old, hv, key, i1_o, i2_o);
        }
        
        if (res == failure_key_moved) {
            if (ti_new == nullptr) { // all buckets were moved, and tables swapped
                unset_hazard_pointers();
                LIBCUCKOO_DBG("ti_new is nullptr in failure_key_moved");
                goto RETRY;
            }

            //migrate_something(ti_old, ti_new, i1_o, i2_o);

            res = delete_one(ti_new, hv, key, i1_n, i2_n);

            // key moved from new table, meaning that an even newer table was created
            if (res == failure_key_moved) {
                LIBCUCKOO_DBG("Key already moved from new table...that was fast, %d\n", res);
                unset_hazard_pointers();
                goto RETRY;
            }

            assert(res == ok || res == failure_key_not_found);
        }
        unset_hazard_pointers();
        return (res == ok);
    }

    /*! hash_function returns the hash function object used by the
     * table. */
    hasher hash_function() {
        return hashfn;
    }

    /*! key_eq returns the equality predicate object used by the
     * table. */
    key_equal key_eq() {
        return eqfn;
    }

private:

    /* cacheint is a cache-aligned atomic integer type. */
    struct cacheint {
        std::atomic<size_t> num;
        cacheint() {}
        cacheint(cacheint&& x) {
            num.store(x.num.load());
        }
    } __attribute__((aligned(64)));

    static inline bool getBit( int& bitset, size_t pos) {
        return bitset & (1 << pos);
    }

    static inline void setBit( int& bitset, size_t pos) {
        bitset |= (1 << pos);
    }

    static inline void resetBit( int& bitset, size_t pos) {
        bitset &= ~(1 << pos);
    }

    /* The Bucket type holds SLOT_PER_BUCKET keys and values, and a
     * occupied bitset, which indicates whether the slot at the given
     * bit index is in the table or not. It allows constructing and
     * destroying key-value pairs separate from allocating and
     * deallocating the memory. */
    struct Bucket {
        rw_lock lock;
        int occupied;
        key_type keys[SLOT_PER_BUCKET];
        mapped_type vals[SLOT_PER_BUCKET];
        unsigned char overflow;
        //bool hasmigrated;

        void setKV(size_t pos, const key_type& k, const mapped_type& v) {
            setBit( occupied, pos);
            new (keys+pos) key_type(k);
            new (vals+pos) mapped_type(v);
        }

        void eraseKV(size_t pos) {
            resetBit( occupied, pos);
            (keys+pos)->~key_type();
            (vals+pos)->~mapped_type();
        }

        void clear() {
            for (size_t i = 0; i < SLOT_PER_BUCKET; i++) {
                if (getBit(occupied,i)) {
                    eraseKV(i);
                }
            }
            overflow = 0;
            //hasmigrated = false;
        }
    };

    /* TableInfo contains the entire state of the hashtable. We
     * allocate one TableInfo pointer per hash table and store all of
     * the table memory in it, so that all the data can be atomically
     * swapped during expansion. */
    struct TableInfo {
        // 2**hashpower is the number of buckets
        size_t hashpower_;

        // unique pointer to the array of buckets
        Bucket* buckets_;

        // per-core counters for the number of inserts and deletes
        std::vector<cacheint> num_inserts;
        std::vector<cacheint> num_deletes;
        //std::vector<cacheint> num_migrated_buckets;

        // counter for the position of the next bucket to try and migrate
        //cacheint migrate_bucket_ind;

        /* The constructor allocates the memory for the table. For
         * buckets, it uses the bucket_allocator, so that we can free
         * memory independently of calling its destructor. It
         * allocates one cacheint for each core in num_inserts and
         * num_deletes. */
        TableInfo(const size_t hashtable_init) {
            buckets_ = nullptr;
                hashpower_ = hashtable_init;
                //timeval t1, t2;
                //gettimeofday(&t1, NULL);

                if( hashpower_ > 10) {
                    // void* res = mmap(NULL, hashsize(hashpower_)*sizeof(Bucket), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
                    // if( res == (void*) MAP_FAILED) {
                    //     std::cout << "MMap Failed" << errno << std::endl;
                    //     exit(1);
                    // }
                    // buckets_ = static_cast<Bucket*>(res);
                    buckets_ = static_cast<Bucket*>( calloc(hashsize(hashpower_), sizeof(Bucket)));
                    if( buckets_ == nullptr ) {
                        throw std::bad_alloc();
                    }
                    //buckets_ = new Bucket[hashsize(hashpower_)];
                } else {
                    buckets_ = new Bucket[hashsize(hashpower_)];
                }
                /*buckets_ = static_cast<Bucket*>( calloc(hashsize(hashpower_), sizeof(Bucket)));
                if( buckets_ == nullptr ) {
                    throw std::bad_alloc();
                }*/
                //gettimeofday(&t2, NULL);
                //double elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
                //elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
                //std::cout << "To construct buckets" << elapsed_time << std::endl;
                
                num_inserts.resize(kNumCores);
                num_deletes.resize(kNumCores);
                //num_migrated_buckets.resize(kNumCores);

                for (size_t i = 0; i < kNumCores; i++) {
                    num_inserts[i].num.store(0);
                    num_deletes[i].num.store(0);
                }
                //migrate_bucket_ind.num.store(0); 
        }

        ~TableInfo() {
            if( hashpower_ > 10) {
                //munmap( buckets_, hashsize(hashpower_)*sizeof(Bucket));
                free(buckets_);
                //delete[] buckets_;
            } else {
                delete[] buckets_;
            }
        }

    };
    std::atomic<TableInfo*> table_info;
    std::atomic<TableInfo*> new_table_info;
    rw_lock snapshot_lock;

    /* old_table_infos holds pointers to old TableInfos that were
     * replaced during expansion. This keeps the memory alive for any
     * leftover operations, until they are deleted by the global
     * hazard pointer manager. */
    std::list<TableInfo*> old_table_infos;

    static hasher hashfn;
    static key_equal eqfn;
    static std::allocator<Bucket> bucket_allocator;


    /* find_one searches a specific table instance for the value corresponding to a given hash value 
     * It doesn't take any locks */
    cuckoo_status find_one(const TableInfo *ti, size_t hv, const key_type& key, mapped_type& val,
                            size_t& i1, size_t& i2) {
        size_t v1_i, v2_i, v1_f, v2_f;
        bool overflow;
        cuckoo_status st;
        
        //fast path
        get_version(ti, i1, v1_i);
        st = try_read_from_bucket(ti, key, val, i1);
        overflow = hasOverflow(ti,i1);
        get_version(ti, i1, v1_f);
        if( check_version( v1_i, v1_f) )
            if (st == ok || !overflow ) 
                return st;
        //we weren't lucky
        do {
            get_version_two(ti, i1, i2, v1_i, v2_i);
            st = cuckoo_find(key, val, hv, ti, i1, i2);
            get_version_two(ti, i1, i2, v1_f, v2_f);
        } while(!check_version_two(v1_i, v2_i, v1_f, v2_f));

        return st;
    }

    /* insert_one tries to insert a key-value pair into a specific table instance.
     * It will return a failure if the key is already in the table, we've started an expansion,
     * or a bucket was moved.
     * Regardless, it starts with the buckets unlocked and ends with buckets unlocked.
     * The possible return values are ok, failure_key_duplicated, failure_key_moved, failure_table_full 
     * In particular, failure_too_slow will trigger a retry
     */
    cuckoo_status insert_one(TableInfo *ti, size_t hv, const key_type& key,
                            const mapped_type& val, size_t& i1, size_t& i2) {
        int open1, open2;
        cuckoo_status res1, res2;
    RETRY:
        //fastpath: 
        lock( ti, i1 );
        res1 = try_add_to_bucket(ti, key, val, i1, open1);
        if (res1 == failure_key_duplicated || res1 == failure_key_moved) {
            unlock( ti, i1 );
            return res1;
        } else {
            assert( res1 == ok ); 
            
            if( !hasOverflow(ti, i1) && open1 != -1 ) {
                //LIBCUCKOO_DBG("We can shortcut in insert! Load factor: %0.2f\n", cuckoo_loadfactor( ti ));
                add_to_bucket(ti, key, val, i1, open1);
                unlock( ti, i1 );
                return ok;
            } else { //we need to try and get the second lock
                if( !try_lock(ti, i2) ) {
                    unlock(ti, i1);
                    lock_two(ti, i1, i2);
                    // we need to make sure nothing happened to the first bucket while it was unlocked
                    res1 = try_add_to_bucket(ti, key, val, i1, open1);
                    if (res1 == failure_key_duplicated || res1 == failure_key_moved) {
                        unlock_two(ti, i1, i2);
                        return res1;
                    }
                }
            }
        }

        //at this point we must have both buckets locked
        res2 = try_add_to_bucket(ti, key, val, i2, open2);
        if (res2 == failure_key_duplicated || res2 == failure_key_moved) {
            unlock_two(ti, i1, i2);
            return res2;
        }

        if (open1 != -1) {
            add_to_bucket(ti, key, val, i1, open1);
            unlock_two(ti, i1, i2);
            return ok;
        }

        if (open2 != -1) {
            ti->buckets_[i1].overflow++;
            add_to_bucket(ti, key, val, i2, open2);
            unlock_two(ti, i1, i2);
            return ok;
        }

        // we are unlucky, so let's perform cuckoo hashing
        size_t insert_bucket = 0;
        size_t insert_slot = 0;
        mapped_type oldval;
        cuckoo_status st = run_cuckoo(ti, i1, i2, insert_bucket, insert_slot);
        if (st == ok) {
            assert(!getBit(ti->buckets_[insert_bucket].occupied,insert_slot));
            assert(insert_bucket == index_hash(ti, hv) || insert_bucket == alt_index(ti, hv, index_hash(ti, hv)));
            /* Since we unlocked the buckets during run_cuckoo,
             * another insert could have inserted the same key into
             * either i1 or i2, so we check for that before doing the
             * insert. */
            if (cuckoo_find(key, oldval, hv, ti, i1, i2) == ok) {
                unlock_two(ti, i1, i2);
                return failure_key_duplicated;
            }
            size_t first_bucket = index_hash(ti, hv);
            if( insert_bucket != first_bucket) {
                ti->buckets_[first_bucket].overflow++;
            }
            add_to_bucket(ti, key, val, insert_bucket, insert_slot);
            unlock_two(ti, i1, i2);
            return ok;
        }

        assert(st == failure_too_slow || st == failure_table_full || st == failure_key_moved);
        if (st == failure_table_full) {
            assert(0);
            LIBCUCKOO_DBG("hash table is full (hashpower = %zu, hash_items = %zu, load factor = %.2f), need to increase hashpower\n",
                      ti->hashpower_, cuckoo_size(ti), cuckoo_loadfactor(ti));

            //we will create a new table and set the new table pointer to it
            if (cuckoo_expand_start(ti, ti->hashpower_+1) == failure_under_expansion) {
                LIBCUCKOO_DBG("Somebody swapped the table pointer before I did. Anyways, it's changed!\n");
            }
        } else if(st == failure_too_slow) {
            LIBCUCKOO_DBG( "We were too slow, and another insert got between us!\n");
            goto RETRY;
        }

        return st;
    }


    /* delete_one tries to delete a key-value pair into a specific table instance.
     * It will return a failure only if the key wasn't found or a bucket was moved.
     * Regardless, it starts with the buckets unlocked and ends with buckets locked 
     */
    cuckoo_status delete_one(TableInfo *ti, size_t hv, const key_type& key,
                             size_t& i1, size_t& i2) {
        i1 = index_hash(ti, hv);
        i2 = alt_index(ti, hv, i1);
        cuckoo_status res1, res2;
        
        lock( ti, i1 );
        res1 = try_del_from_bucket(ti, key, i1);
        // if we found the element there, or nothing ever was added to second bucket, then we're done
        if (res1 == ok || res1 == failure_key_moved || !hasOverflow(ti, i1) ) {
            unlock( ti, i1 );
            return res1;
        } else {
            if( !try_lock(ti, i2) ) {
                unlock(ti, i1);
                lock_two(ti, i1, i2);
                res1 = try_del_from_bucket(ti, key, i1);
                if( res1 == ok || res1 == failure_key_moved ) {
                    unlock_two(ti, i1, i2);
                    return res1;
                }
            }
        }

        res2 = try_del_from_bucket(ti, key, i2);
        if( res2 == ok ) {
            ti->buckets_[i1].overflow--;
            unlock_two(ti, i1, i2);
            return res2;
        }
        if (res2 == failure_key_moved) {
            unlock_two(ti, i1, i2);
            return res2;
        }

        unlock_two(ti, i1, i2);
        return failure_key_not_found;
    }

    /* Tries to migrate the two buckets that an attempted insert could go to.
     * Also ensures trying to migrate a new bucket. This invariant ensures that we never
     * fill up the new table before the old table has finished migrating.
     */
    /*
    bool migrate_something(TableInfo *ti_old, TableInfo *ti_new, 
                               size_t i1_o, size_t i2_o) {
        assert(0);
        assert(ti_old != ti_new);

        lock(ti_old, i1_o);
        try_migrate_bucket(ti_old, ti_new, i1_o);
        unlock(ti_old, i1_o);

        lock(ti_old, i2_o);
        try_migrate_bucket(ti_old, ti_new, i2_o);
        unlock(ti_old, i2_o);

        size_t new_migrate_bucket = ti_old->migrate_bucket_ind.num.fetch_add(1, std::memory_order_relaxed);

        //all threads wait on one expansion end thread to terminate? Alternatively, each waits on cuckoo_size to
        //go to 0 and then free for all
        if( new_migrate_bucket >= hashsize(ti_old->hashpower_) ) {
            while( new_table_info.load() == ti_new );
        } else {
            lock(ti_old, new_migrate_bucket);
            try_migrate_bucket(ti_old, ti_new, new_migrate_bucket);
            unlock(ti_old, new_migrate_bucket);
            if( new_migrate_bucket == hashsize(ti_old->hashpower_) - 1 ) {
                while(cuckoo_size( ti_old ) != 0 ) {
                //    LIBCUCKOO_DBG("Im the chosen one! %zu\n", cuckoo_size( ti_old ) );
                };
                cuckoo_expand_end(ti_old, ti_new);
            }
        }
        return true;
    }
    */
    /* assumes bucket in old table is locked the whole time
     * tries to migrate bucket, returning true on success, false on failure
     * since we return immediately if the bucket has already migrated, if continue
     * then we know that ti_new is indeed the newest table pointer
     */
    /*
    bool try_migrate_bucket(TableInfo* ti_old, TableInfo* ti_new, size_t old_bucket) {
        assert(0);
        if (ti_old->buckets_[old_bucket].hasmigrated) {
            //LIBCUCKOO_DBG("Already migrated bucket %zu\n", old_bucket);
            return false;
        }
        size_t i1, i2, hv;
        key_type key;
        mapped_type val;
        cuckoo_status res;
        for (size_t i = 0; i < SLOT_PER_BUCKET; i++) {
            if (!getBit(ti_old->buckets_[old_bucket].occupied, i)) {
                continue;
            }
            key = ti_old->buckets_[old_bucket].keys[i];
            val = ti_old->buckets_[old_bucket].vals[i];

            hv = hashed_key(key);
            i1 = index_hash(ti_new, hv);
            i2 = alt_index(ti_new, hv, i1);

            res = insert_one(ti_new, hv, key, val, i1, i2);

            // this can't happen since nothing can have moved out of ti_new (so no failure_key_moved)
            // our invariant ensures that the table can't be full (so no failure_table_full)
            // and the same key can't have been inserted already (so no failure_key_duplicated)            
            if( res != ok ) {
                LIBCUCKOO_DBG("In try_migrate_bucket: hashpower = %zu, %zu, hash_items = %zu,%zu, load factor = %.2f,%.2f), need to increase hashpower\n",
                      ti_old->hashpower_, ti_new->hashpower_, cuckoo_size(ti_old), cuckoo_size(ti_new),
                      cuckoo_loadfactor(ti_old), cuckoo_loadfactor(ti_new) );
                exit(1);
            }

            //to keep track of current number of elements in table
            ti_old->num_deletes[counterid].num.fetch_add(1, std::memory_order_relaxed);
        }

        ti_old->buckets_[old_bucket].hasmigrated = true;
        return true;
    }
    */

    static inline bool hasOverflow(const TableInfo *ti, const size_t i) {
        return (ti->buckets_[i].overflow > 0);
    }

    /* get_version gets the version for a given bucket index */
    static inline void get_version(const TableInfo *ti, const size_t i, size_t& v) {
        v = ti->buckets_[i].lock.get_version();
    }

    static inline void get_version_two(const TableInfo *ti, const size_t i1, const size_t i2, 
                                       size_t& v1, size_t& v2) {
        v1 = ti->buckets_[i1].lock.get_version();
        v2 = ti->buckets_[i2].lock.get_version();
    }

    /* check_version makes sure that the final version is the same as the initial version, and 
     * that the initial version is not dirty
     */
    static inline bool check_version(const size_t v_i, const size_t v_f) {
        return (v_i==v_f && (v_i & W) == 0);
    }

    static inline bool check_version_two(const size_t v1_i, const size_t v2_i,
                                         const size_t v1_f, const size_t v2_f) {
        return check_version(v1_i,v1_f) && check_version(v2_i, v2_f);
    }

    /* lock locks a bucket based on lock_level privileges
     */
    static inline void lock(const TableInfo *ti, const size_t i) {
        ti->buckets_[i].lock.lock();
    }

    static inline void unlock(const TableInfo *ti, const size_t i) {
        ti->buckets_[i].lock.unlock();
    }

    static inline bool try_lock(const TableInfo *ti, const size_t i) {
        return ti->buckets_[i].lock.try_lock();
    }
    
    static inline void lock_two(const TableInfo *ti, size_t i1, size_t i2) {
        if (i1 < i2) {
            lock(ti, i1);
            lock(ti, i2);
        } else if (i2 < i1) {
            lock(ti, i2);
            lock(ti, i1);
        } else {
            lock(ti, i1);
        }
    }

    static inline void unlock_two(const TableInfo *ti, size_t i1, size_t i2) {
        unlock(ti, i1);
        if (i1 != i2) {
            unlock(ti, i2);
        }
    }

    static inline void lock_three(const TableInfo *ti, size_t i1,
                                  size_t i2, size_t i3) {
        // If any are the same, we just run lock_two
        if (i1 == i2) {
            lock_two(ti, i1, i3);
        } else if (i2 == i3) {
            lock_two(ti, i1, i3);
        } else if (i1 == i3) {
            lock_two(ti, i1, i2);
        } else {
            if (i1 < i2) {
                if (i2 < i3) {
                    lock(ti, i1);
                    lock(ti, i2);
                    lock(ti, i3);
                } else if (i1 < i3) {
                    lock(ti, i1);
                    lock(ti, i3);
                    lock(ti, i2);
                } else {
                    lock(ti, i3);
                    lock(ti, i1);
                    lock(ti, i2);
                }
            } else if (i2 < i3) {
                if (i1 < i3) {
                    lock(ti, i2);
                    lock(ti, i1);
                    lock(ti, i3);
                } else {
                    lock(ti, i2);
                    lock(ti, i3);
                    lock(ti, i1);
                }
            } else {
                lock(ti, i3);
                lock(ti, i2);
                lock(ti, i1);
            }
        }
    }

    /* unlock_three unlocks the three given buckets */
    static inline void unlock_three(const TableInfo *ti, size_t i1,
                                    size_t i2, size_t i3) {
        unlock(ti, i1);
        if (i2 != i1) {
            unlock(ti, i2);
        }
        if (i3 != i1 && i3 != i2) {
            unlock(ti, i3);
        }
    }

    void snapshot_both(TableInfo*& ti_old, TableInfo*& ti_new) {
    TryAcquire:
        size_t start_version = snapshot_lock.get_version();
        ti_old = table_info.load();
        ti_new = new_table_info.load();
        *hazard_pointer_old = static_cast<void*>(ti_old);
        *hazard_pointer_new = static_cast<void*>(ti_new);
        size_t end_version = snapshot_lock.get_version();
        if( start_version % 2 == 1 || start_version != end_version) {
            goto TryAcquire;
        }
    }
    void snapshot_both_get_buckets(TableInfo*& ti_old, TableInfo*& ti_new, size_t hv, 
                                    size_t& i1_o, size_t& i2_o, size_t& i1_n, size_t& i2_n) {
        snapshot_both(ti_old, ti_new);
        i1_o = index_hash(ti_old, hv);
        i2_o = alt_index(ti_old, hv, i1_o);
        if( ti_new == nullptr ) 
            return;
        i1_n = index_hash(ti_new, hv);
        i2_n = alt_index(ti_new, hv, i1_n);
    }

    void snapshot_both_no_hazard(TableInfo*& ti_old, TableInfo*& ti_new) {
        ti_old = table_info.load();
        ti_new = new_table_info.load();
    }

    /* unlock_all increases the version of every counter (from 1mod2 to 0mod2)
     * effectively releases all the "locks" on the buckets */
    inline void unlock_all(TableInfo *ti) {
        for (size_t i = 0; i < hashsize(ti->hashpower_); i++) {
            unlock(ti, i);
        }
    }

    static const size_t W = 1;

    // key size in bytes
    static const size_t kKeySize = sizeof(key_type);

    // value size in bytes
    static const size_t kValueSize = sizeof(mapped_type);

    // size of a bucket in bytes
    static const size_t kBucketSize = sizeof(Bucket);

    // number of cores on the machine
    static const size_t kNumCores;

    //counter for number of threads
    static std::atomic<size_t> numThreads;

    // The maximum number of cuckoo operations per insert. This must
    // be less than or equal to SLOT_PER_BUCKET^(MAX_BFS_DEPTH+1)
    static const size_t MAX_CUCKOO_COUNT = 500;

    // The maximum depth of a BFS path
    static const size_t MAX_BFS_DEPTH = 4;

    // the % of moved buckets above which migrate_all is called
    //static constexpr double MIGRATE_THRESHOLD = 0.8;

    /* hashsize returns the number of buckets corresponding to a given
     * hashpower. */
    static inline size_t hashsize(const size_t hashpower) {
        return 1U << hashpower;
    }

    /* hashmask returns the bitmask for the buckets array
     * corresponding to a given hashpower. */
    static inline size_t hashmask(const size_t hashpower) {
        return hashsize(hashpower) - 1;
    }

    /* hashed_key hashes the given key. */
    static inline size_t hashed_key(const key_type &key) {
        return hashfn(key);
    }

    /* index_hash returns the first possible bucket that the given
     * hashed key could be. */
    static inline size_t index_hash (const TableInfo *ti, const size_t hv) {
        return hv & hashmask(ti->hashpower_);
    }

    /* alt_index returns the other possible bucket that the given
     * hashed key could be. It takes the first possible bucket as a
     * parameter. Note that this function will return the first
     * possible bucket if index is the second possible bucket, so
     * alt_index(ti, hv, alt_index(ti, hv, index_hash(ti, hv))) ==
     * index_hash(ti, hv). */
    static inline size_t alt_index(const TableInfo *ti,
                                   const size_t hv,
                                   const size_t index) {
        // ensure tag is nonzero for the multiply
        const size_t tag = (hv >> ti->hashpower_) + 1;
        /* 0x5bd1e995 is the hash constant from MurmurHash2 */
        return (index ^ (tag * 0x5bd1e995)) & hashmask(ti->hashpower_);
    }

    /* CuckooRecord holds one position in a cuckoo path. */
    typedef struct  {
        size_t bucket;
        size_t slot;
        key_type key;
    }  CuckooRecord;

    /* b_slot holds the information for a BFS path through the
     * table */
    struct b_slot {
        // The bucket of the last item in the path
        size_t bucket;
        // a compressed representation of the slots for each of the
        // buckets in the path.
        size_t pathcode;
        // static_assert(pow(SLOT_PER_BUCKET, MAX_BFS_DEPTH+1) <
        //               std::numeric_limits<decltype(pathcode)>::max(),
        //               "pathcode may not be large enough to encode a cuckoo path");
        // The 0-indexed position in the cuckoo path this slot
        key_type keys[MAX_BFS_DEPTH];
        size_t hvs[MAX_BFS_DEPTH];
        // occupies
        int depth;
        b_slot() {}
        b_slot(const size_t b, const size_t p, const int d)
            : bucket(b), pathcode(p), depth(d) {}
    } __attribute__((__packed__));

    /* b_queue is the queue used to store b_slots for BFS cuckoo
     * hashing. */
    class b_queue {
        b_slot slots[MAX_CUCKOO_COUNT+1];
        size_t first;
        size_t last;

    public:
        b_queue() : first(0), last(0) {}


        void enqueue(b_slot x) {
            slots[last] = x;
            last = (last == MAX_CUCKOO_COUNT) ? 0 : last+1;
            assert(last != first);
        }

        b_slot dequeue() {
            assert(first != last);
            b_slot& x = slots[first];
            first = (first == MAX_CUCKOO_COUNT) ? 0 : first+1;
            return x;
        }

        bool not_full() {
            const size_t next = (last == MAX_CUCKOO_COUNT) ? 0 : last+1;
            return next != first;
        }

        bool not_empty() {
            return first != last;
        }
    } __attribute__((__packed__));

    /* slot_search searches for a cuckoo path using breadth-first
       search. It starts with the i1 and i2 buckets, and, until it finds
       a bucket with an empty slot, adds each slot of the bucket in the
       b_slot. If the queue runs out of space, it fails. 
       No need to take locks here because not moving anything */
    static b_slot slot_search(const TableInfo *ti, const size_t i1,
                              const size_t i2) {
        b_queue q;
        // The initial pathcode informs cuckoopath_search which bucket
        // the path starts on
        q.enqueue(b_slot(i1, 0, 0));
        q.enqueue(b_slot(i2, 1, 0));
        while (q.not_full() && q.not_empty()) {
            b_slot x = q.dequeue();
            
            // Picks a random slot to start from
            for (size_t slot = 0; slot < SLOT_PER_BUCKET && q.not_full(); slot++) {

                key_type key = ti->buckets_[x.bucket].keys[slot];
                size_t hv = hashed_key(key);
                
                if (!getBit(ti->buckets_[x.bucket].occupied, slot)) {
                    // We can terminate the search here
                    size_t old_pathcode = x.pathcode;
                    x.pathcode = x.pathcode * SLOT_PER_BUCKET + slot;
                    assert(x.pathcode >= old_pathcode);
                    return x;
                }
                
                // Create a new b_slot item, that represents the
                // bucket we would look at after searching x.bucket
                // for empty slots.
                b_slot y(alt_index(ti, hv, x.bucket),
                        x.pathcode * SLOT_PER_BUCKET + slot, x.depth+1);

                // Check if any of the slots in the prospective bucket
                // are empty, and, if so, return that b_slot. We lock
                // the bucket so that no changes occur while
                // iterating.
                //if (ti->buckets_[y.bucket].hasmigrated) {
                    //return b_slot(0, 0, -2);
                //}
                for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
                    if (!getBit(ti->buckets_[y.bucket].occupied, j)) {
                        size_t old_pathcode = y.pathcode;
                        y.pathcode = y.pathcode * SLOT_PER_BUCKET + j;
                        assert(y.pathcode >= old_pathcode);
                        return y;
                    }
                }

                // No empty slots were found, so we push this onto the
                // queue
                if (y.depth != static_cast<int>(MAX_BFS_DEPTH)) {
                    q.enqueue(y);
                }
            }
        }
        // We didn't find a short-enough cuckoo path, so the queue ran
        // out of space. Return a failure value.
        return b_slot(0, 0, -1);
    }

    /* cuckoopath_search finds a cuckoo path from one of the starting
     * buckets to an empty slot in another bucket. It returns the
     * depth of the discovered cuckoo path on success, -1 for too long of 
     * a cuckoo path, and -2 for a moved bucket found along the way.
     * Since it doesn't take locks on the buckets it
     * searches, the data can change between this function and
     * cuckoopath_move. Thus cuckoopath_move checks that the data
     * matches the cuckoo path before changing it. */
    static int cuckoopath_search(const TableInfo *ti, CuckooRecord* cuckoo_path,
                                 const size_t i1, const size_t i2) {
        b_slot x;
        CuckooRecord *curr, *prev;
        size_t prevhv;
        
        x = slot_search(ti, i1, i2);
        if (x.depth < 0) {
            return x.depth;
        }
        // Fill in the cuckoo path slots from the end to the beginning
        for (int i = x.depth; i >= 0; i--) {
            cuckoo_path[i].slot = x.pathcode % SLOT_PER_BUCKET;
            x.pathcode /= SLOT_PER_BUCKET;
        }
        /* Fill in the cuckoo_path buckets and keys from the beginning
         * to the end, using the final pathcode to figure out which
         * bucket the path starts on. Since data could have been
         * modified between slot_search and the computation of the
         * cuckoo path, this could be an invalid cuckoo_path. */
        curr = cuckoo_path;
        if (x.pathcode == 0) {
            curr->bucket = i1;
            if (!getBit(ti->buckets_[curr->bucket].occupied, curr->slot)) {
                // We can terminate here
                return 0;
            }
            curr->key = ti->buckets_[curr->bucket].keys[curr->slot];
        } else {
            assert(x.pathcode == 1);
            curr->bucket = i2;
            if (!getBit(ti->buckets_[curr->bucket].occupied, curr->slot)) {
                // We can terminate here
                return 0;
            }
            curr->key = ti->buckets_[curr->bucket].keys[curr->slot];
        }
        for (int i = 1; i <= x.depth; i++) {
            prev = curr++;
            prevhv = hashed_key(prev->key);
           
            assert((prev->bucket == index_hash(ti, prevhv) ||
                   prev->bucket == alt_index(ti, prevhv, index_hash(ti, prevhv))));
            
            // We get the bucket that this slot is on by computing the
            // alternate index of the previous bucket
            curr->bucket = alt_index(ti, prevhv, prev->bucket);
            if (!getBit(ti->buckets_[curr->bucket].occupied, curr->slot)) {
                // We can terminate here
                return i;
            }
            // XXX the problem here is that the key could have changed since we
            // last found item. Therefore, the cuckoo path found by the slot_search 
            // may no longer be accurate.
            curr->key = ti->buckets_[curr->bucket].keys[curr->slot];
        }
        return x.depth;
    }

    /* cuckoopath_move moves keys along the given cuckoo path in order
     * to make an empty slot in one of the buckets in cuckoo_insert.
     * Before the start of this function, the two insert-locked
     * buckets were unlocked in run_cuckoo. At the end of the
     * function, if the function returns ok (success), then the last
     * bucket it looks at (which is either i1 or i2 in run_cuckoo)
     * remains locked. If the function is unsuccessful, then both
     * insert-locked buckets will be unlocked. */
    static cuckoo_status cuckoopath_move(TableInfo *ti, CuckooRecord* cuckoo_path,
                                size_t depth, const size_t i1, const size_t i2) {

        if (depth == 0) {
            /* There is a chance that depth == 0, when
             * try_add_to_bucket sees i1 and i2 as full and
             * cuckoopath_search finds one empty. In this case, we
             * lock both buckets. If the bucket that cuckoopath_search
             * found empty isn't empty anymore, we unlock them and
             * return false. Otherwise, the bucket is empty and
             * insertable, so we hold the locks and return true. */
            const size_t bucket = cuckoo_path[0].bucket;
            assert(bucket == i1 || bucket == i2);
            lock_two(ti, i1, i2);
            //if (ti->buckets_[i1].hasmigrated || ti->buckets_[i2].hasmigrated) {
                //unlock_two(ti, i1, i2);
                //return failure_key_moved;
            //}
            if (!getBit(ti->buckets_[bucket].occupied, cuckoo_path[0].slot)) {
                return ok;
            } else {
                unlock_two(ti, i1, i2);
                return failure_too_slow;
            }
        }

        while (depth > 0) {
            CuckooRecord *from = cuckoo_path + depth - 1;
            CuckooRecord *to   = cuckoo_path + depth;
            size_t fb = from->bucket;
            size_t fs = from->slot;
            size_t tb = to->bucket;
            size_t ts = to->slot;

            size_t ob = 0;
            if (depth == 1) {
                /* Even though we are only swapping out of i1 or i2,
                 * we have to lock both of them along with the slot we
                 * are swapping to, since at the end of this function,
                 * i1 and i2 must be locked. */
                ob = (fb == i1) ? i2 : i1;
                lock_three(ti, fb, tb, ob);
            } else {
                lock_two(ti, fb, tb);
            }

            /*
            if (ti->buckets_[fb].hasmigrated ||
                ti->buckets_[tb].hasmigrated ||
                ti->buckets_[ob].hasmigrated) {

                if (depth == 1) {
                    unlock_three(ti, fb, tb, ob);
                } else {
                    unlock_two(ti, fb, tb);
                }
                return failure_key_moved;
            }
            */
            /* We plan to kick out fs, but let's check if it is still
             * there; there's a small chance we've gotten scooped by a
             * later cuckoo. If that happened, just... try again. Also
             * the slot we are filling in may have already been filled
             * in by another thread, or the slot we are moving from
             * may be empty, both of which invalidate the swap. */
            if (!eqfn(ti->buckets_[fb].keys[fs], from->key) ||
                getBit(ti->buckets_[tb].occupied, ts) ||
                !getBit(ti->buckets_[fb].occupied, fs) ) {

                if (depth == 1) {
                    unlock_three(ti, fb, tb, ob);
                } else {
                    unlock_two(ti, fb, tb);
                }
                return failure_too_slow;
            }
            size_t hv = hashed_key( ti->buckets_[fb].keys[fs]);
            size_t first_bucket = index_hash(ti, hv);
            if( fb == first_bucket ) { //we're moving from first bucket to alternate
                ti->buckets_[first_bucket].overflow++;
            } else { //we're moving from alternate to first bucket
                assert( tb == first_bucket );
                ti->buckets_[first_bucket].overflow--;
            }

            ti->buckets_[tb].setKV(ts, ti->buckets_[fb].keys[fs], ti->buckets_[fb].vals[fs]);
            ti->buckets_[fb].eraseKV(fs);
            
            if (depth == 1) {
                // Don't unlock fb or ob, since they are needed in
                // cuckoo_insert. Only unlock tb if it doesn't unlock
                // the same bucket as fb or ob.
                if (tb != fb && tb != ob) {
                    unlock(ti, tb);
                }
            } else {
                unlock_two(ti, fb, tb);
            }
            depth--;
        }
        return ok;
    }

    /* run_cuckoo performs cuckoo hashing on the table in an attempt
     * to free up a slot on either i1 or i2. On success, the bucket
     * and slot that was freed up is stored in insert_bucket and
     * insert_slot. In order to perform the search and the swaps, it
     * has to unlock both i1 and i2, which can lead to certain
     * concurrency issues, the details of which are explained in the
     * function. If run_cuckoo returns ok (success), then the slot it
     * freed up is still locked. Otherwise it is unlocked. */
    cuckoo_status run_cuckoo(TableInfo *ti, const size_t i1, const size_t i2,
                             size_t &insert_bucket, size_t &insert_slot) {

        CuckooRecord cuckoo_path[MAX_BFS_DEPTH+1];
        cuckoo_status res;

        // We must unlock i1 and i2 here, so that cuckoopath_search
        // and cuckoopath_move can lock buckets as desired without
        // deadlock. cuckoopath_move has to look at either i1 or i2 as
        // its last slot, and it will lock both buckets and leave them
        // locked after finishing. This way, we know that if
        // cuckoopath_move succeeds, then the buckets needed for
        // insertion are still locked. If cuckoopath_move fails, the
        // buckets are unlocked and we try again. This unlocking does
        // present two problems. The first is that another insert on
        // the same key runs and, finding that the key isn't in the
        // table, inserts the key into the table. Then we insert the
        // key into the table, causing a duplication. To check for
        // this, we search i1 and i2 for the key we are trying to
        // insert before doing so (this is done in cuckoo_insert, and
        // requires that both i1 and i2 are locked). Another problem
        // is that an expansion runs and changes table_info, meaning
        // the cuckoopath_move and cuckoo_insert would have operated
        // on an old version of the table, so the insert would be
        // invalid. For this, we check that ti == table_info.load()
        // after cuckoopath_move, signaling to the outer insert to try
        // again if the comparison fails.
        unlock_two(ti, i1, i2);

        while (true) {
            int depth = cuckoopath_search(ti, cuckoo_path, i1, i2);
            if (depth == -1) {
                return failure_table_full; //happens if path too long
            }

            if (depth == -2) {
                return failure_key_moved; //happens if found moved bucket along path
            }
            res = cuckoopath_move(ti, cuckoo_path, depth, i1, i2);
            if (res == ok) {
                insert_bucket = cuckoo_path[0].bucket;
                insert_slot = cuckoo_path[0].slot;
                assert(insert_bucket == i1 || insert_bucket == i2);
#if LIBCUCKOO_DEBUG
                assert(ti->buckets_[i1].lock.is_locked());
                assert(ti->buckets_[i2].lock.is_locked());
#endif
                assert(!getBit(ti->buckets_[insert_bucket].occupied, insert_slot));
                return ok;
            }

            if (res == failure_key_moved) {
                LIBCUCKOO_DBG("Found moved bucket when running cuckoopath_move");
                return failure_key_moved;
            }
        }
        LIBCUCKOO_DBG("Should never reach here");
        return failure_function_not_supported;
    }
    
    /* try_read_from-bucket will search the bucket for the given key
     * and store the associated value if it finds it. */
    static cuckoo_status try_read_from_bucket(const TableInfo *ti, 
                                     const key_type &key, mapped_type &val,
                                     const size_t i) {
        //if (ti->buckets_[i].hasmigrated) {
            //return failure_key_moved;
        //}

        for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
            if (!getBit(ti->buckets_[i].occupied, j)) {
                continue;
            }

            if (eqfn(key, ti->buckets_[i].keys[j])) {
                val = ti->buckets_[i].vals[j];
                return ok;
            }
        }
        return failure_key_not_found;
    }

    /* add_to_bucket will insert the given key-value pair into the
     * slot. */
    static inline void add_to_bucket(TableInfo *ti, 
                                     const key_type &key, const mapped_type &val,
                                     const size_t i, const size_t j) {
        assert(!getBit(ti->buckets_[i].occupied, j));
        
        ti->buckets_[i].setKV(j, key, val);
        ti->num_inserts[counterid].num.fetch_add(1, std::memory_order_relaxed);
    }

    /* try_add_to_bucket will search the bucket and store the index of
     * an empty slot if it finds one, or -1 if it doesn't. Regardless,
     * it will search the entire bucket and return false if it finds
     * the key already in the table (duplicate key error) and true
     * otherwise. */
    static cuckoo_status try_add_to_bucket(TableInfo *ti, 
                                  const key_type &key, const mapped_type &val,
                                  const size_t i, int& j) {
        (void)val;
        j = -1;
        bool found_empty = false;

        //if (ti->buckets_[i].hasmigrated) {
            //return failure_key_moved;
        //}
        for (size_t k = 0; k < SLOT_PER_BUCKET; k++) {
            if (getBit(ti->buckets_[i].occupied, k)) {
                if (eqfn(key, ti->buckets_[i].keys[k])) {
                    return failure_key_duplicated;
                }
            } else {
                if (!found_empty) {
                    found_empty = true;
                    j = k;
                }
            }
        }
        return ok;
    }

    /* try_del_from_bucket will search the bucket for the given key,
     * and set the slot of the key to empty if it finds it. */
    static cuckoo_status try_del_from_bucket(TableInfo *ti, 
                                    const key_type &key, const size_t i) {
        //if (ti->buckets_[i].hasmigrated) {
            //return failure_key_moved;
        //}

        for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
            if (!getBit(ti->buckets_[i].occupied, j)) {
                continue;
            }
            if (eqfn(ti->buckets_[i].keys[j], key)) {
                ti->buckets_[i].eraseKV(j);
                ti->num_deletes[counterid].num.fetch_add(1, std::memory_order_relaxed);
                return ok;
            }
        }
        return failure_key_not_found;
    }

    /* cuckoo_find searches the table for the given key and value,
     * storing the value in the val if it finds the key. It expects
     * the locks to be taken and released outside the function. */
    static cuckoo_status cuckoo_find(const key_type& key, mapped_type& val,
                                     const size_t hv, const TableInfo *ti,
                                     const size_t i1, const size_t i2) {
        (void)hv;
        cuckoo_status res1, res2;
        res1 = try_read_from_bucket(ti, key, val, i1);
        if (res1 == ok || !hasOverflow(ti, i1) ) {
            return res1;
        } 

        res2 = try_read_from_bucket(ti, key, val, i2);
        if(res2 == ok) {
            return ok;
        }

        if (res1 == failure_key_moved || res2 == failure_key_moved) {
            return failure_key_moved;
        }

        return failure_key_not_found;
    }

    /* cuckoo_find searches the table for the given key and value,
     * storing the value in the val if it finds the key. It expects
     * the locks to be taken and released outside the function. */
    static cuckoo_status cuckoo_find_old(const key_type& key, mapped_type& val,
                                     const size_t hv, const TableInfo *ti,
                                     const size_t i1, const size_t i2) {
        cuckoo_status res1, res2;
        res1 = try_read_from_bucket(ti, key, val, i1);
        if (res1 == ok) {
            return ok;
        }
        res2 = try_read_from_bucket(ti, key, val, i2);
        if(res2 == ok) {
            return ok;
        }

        if (res1 == failure_key_moved || res2 == failure_key_moved) {
            return failure_key_moved;
        }

        return failure_key_not_found;
    }

    /* cuckoo_init initializes the hashtable, given an initial
     * hashpower as the argument. */
    cuckoo_status cuckoo_init(const size_t hashtable_init) {
        //timeval t1, t2;
        //gettimeofday(&t1, NULL);
        table_info.store(new TableInfo(hashtable_init));
        new_table_info.store(nullptr);
        //cuckoo_clear(table_info.load());

        //gettimeofday(&t2, NULL);
        //double elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
        //elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
        //std::cout << "To initialize table" << elapsed_time << std::endl;
        return ok;
    }

    /* count_migrated_buckets returns the number of migrated buckets in the given
     * table. */
    /*
    size_t count_migrated_buckets(const TableInfo *ti) {
        size_t num_migrated = 0;
        for (size_t i = 0; i < ti->num_inserts.size(); i++) {
            num_migrated += ti->num_migrated_buckets[i].num.load();
        }
        return num_migrated;
    }
    */

    /* cuckoo_clear empties the table, calling the destructors of all
     * the elements it removes from the table. It assumes the locks
     * are taken as necessary. */
    cuckoo_status cuckoo_clear(TableInfo *ti) {
        for (size_t i = 0; i < hashsize(ti->hashpower_); i++) {
            ti->buckets_[i].clear();
        }

        for (size_t i = 0; i < ti->num_inserts.size(); i++) {
            ti->num_inserts[i].num.store(0);
            ti->num_deletes[i].num.store(0);
        }
        //ti->migrate_bucket_ind.num.store(0);

        return ok;
    }

    /* cuckoo_size returns the number of elements in the given
     * table. */
    size_t cuckoo_size(const TableInfo *ti) {
        if (ti == nullptr) {
            LIBCUCKOO_DBG("New table doesn't exist yet\n");
            return 0;
        }
        size_t inserts = 0;
        size_t deletes = 0;
        for (size_t i = 0; i < ti->num_inserts.size(); i++) {
            inserts += ti->num_inserts[i].num.load();
            deletes += ti->num_deletes[i].num.load();
        }
        return inserts-deletes;
    }

    /* cuckoo_loadfactor returns the load factor of the given table. */
    float cuckoo_loadfactor(const TableInfo *ti) {
        return 1.0 * cuckoo_size(ti) / SLOT_PER_BUCKET / hashsize(ti->hashpower_);
    }

    /* cuckoo_expand_start tries to create a new table, succeeding as long as 
     * there is no ongoing expansion and no other thread has already created a new table
     * (new_table_pointer != nullptr) */
    cuckoo_status cuckoo_expand_start(TableInfo *ti_old_expected, size_t n) {
        TableInfo *ti_old_actual;
        TableInfo *ti_new_actual;
        TableInfo *ti_new_expected = nullptr;
        snapshot_lock.lock();
        snapshot_both_no_hazard( ti_old_actual, ti_new_actual);
        //LIBCUCKOO_DBG( "start expand %p, %p    %p, %p\n", ti_old_expected, ti_old_actual, ti_new_expected, ti_new_actual);
        
        //we only want to create a new table if there is no ongoing expansion already
        //Also accouunts for possibility of somebody already swapping the table pointer
        if (ti_old_expected != ti_old_actual || ti_new_expected != ti_new_actual) {
            snapshot_lock.unlock();
            return failure_under_expansion;
        }
        //timeval t1, t2;
        //gettimeofday(&t1, NULL);
        new_table_info.store(new TableInfo(n));
        //LIBCUCKOO_DBG("Some stuff: %zu, %zu\n", cuckoo_size(new_table_info.load()), new_table_info.load()->num_inserts[0].num.load());
        //gettimeofday(&t2, NULL);
        //double elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
        //elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
        //std::cout << "To store a new tableinfo ptr" << elapsed_time << std::endl;
        //cuckoo_clear(new_table_info);
        /*gettimeofday(&t1, NULL);
        elapsed_time = (t1.tv_sec - t2.tv_sec) * 1000.0; // sec to ms
        elapsed_time += (t1.tv_usec - t2.tv_usec) / 1000.0; // us to ms
        std::cout << "To zero the new table" << elapsed_time << std::endl;*/
        snapshot_lock.unlock();
        return ok;
    }

    /* cuckoo_expand_end is triggered once all buckets from the old table have been moved over.
     * It tries to swap the new_table pointer to the old, succeeding as long as no other thread
     * has already done so. It then adds the old_table pointer to a list of pointers that will
     * be garbage collected, and sets the new table pointer to nullptr (so for a small period
     * of time we can have both old_table and new_table being the same) */
    cuckoo_status cuckoo_expand_end(TableInfo *ti_old_expected, TableInfo *ti_new_expected) {
        TableInfo *ti_old_actual;
        TableInfo *ti_new_actual;
        snapshot_lock.lock();
        snapshot_both_no_hazard( ti_old_actual, ti_new_actual);
        // LIBCUCKOO_DBG( "end expand %p, %p    %p, %p\n", ti_old_expected, ti_old_actual, ti_new_expected, ti_new_actual);

        if (ti_old_expected != ti_old_actual || ti_new_expected != ti_new_actual) {
            snapshot_lock.unlock();
            return failure_under_expansion;
        }
        // LIBCUCKOO_DBG( "Size of old table is %zu, new one is %zu", cuckoo_size(ti_old_actual), cuckoo_size( ti_new_expected));
        assert( cuckoo_size(ti_old_actual) == 0 );
        table_info.store(ti_new_expected);
        new_table_info.store(nullptr);

        // Rather than deleting ti now, we store it in
        // old_table_infos. The hazard pointer manager will delete it
        // if no other threads are using the pointer.
        old_table_infos.push_back(ti_old_expected);
        timeval t1, t2;
        gettimeofday(&t1, NULL);
        global_hazard_pointers.delete_unused(old_table_infos);
        gettimeofday(&t2, NULL);
        double elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000.0; // sec to ms
        elapsed_time += (t2.tv_usec - t1.tv_usec) / 1000.0; // us to ms
        LIBCUCKOO_DBG("Time to delete old table pointer: %0.04f\n", elapsed_time);
        //std::cout << "Time to delete old table pointer" << elapsed_time << std::endl;
        snapshot_lock.unlock();
        return ok;
    }
};

// Initializing the static members
template <class Key, class T, unsigned Num_Buckets>
__thread void** CuckooHashMapNT<Key, T, Num_Buckets>::hazard_pointer_old = nullptr;

template <class Key, class T, unsigned Num_Buckets>
__thread void** CuckooHashMapNT<Key, T, Num_Buckets>::hazard_pointer_new = nullptr;

template <class Key, class T, unsigned Num_Buckets>
__thread int CuckooHashMapNT<Key, T, Num_Buckets>::counterid = -1;

template <class Key, class T, unsigned Num_Buckets>
typename std::allocator<typename CuckooHashMapNT<Key, T, Num_Buckets>::Bucket>
CuckooHashMapNT<Key, T, Num_Buckets>::bucket_allocator;

template <class Key, class T, unsigned Num_Buckets>
typename CuckooHashMapNT<Key, T, Num_Buckets>::GlobalHazardPointerList
CuckooHashMapNT<Key, T, Num_Buckets>::global_hazard_pointers;

template <class Key, class T, unsigned Num_Buckets>
const size_t CuckooHashMapNT<Key, T, Num_Buckets>::kNumCores =
    std::thread::hardware_concurrency() == 0 ?
    sysconf(_SC_NPROCESSORS_ONLN) : std::thread::hardware_concurrency();

template <class Key, class T, unsigned Num_Buckets>
std::atomic<size_t> CuckooHashMapNT<Key, T, Num_Buckets>::numThreads(0);

template <class Key, class T, unsigned Num_Buckets>
typename CuckooHashMapNT<Key, T, Num_Buckets>::key_equal 
CuckooHashMapNT<Key, T, Num_Buckets>::eqfn;

template <class Key, class T, unsigned Num_Buckets>
typename CuckooHashMapNT<Key, T, Num_Buckets>::hasher
CuckooHashMapNT<Key, T, Num_Buckets>::hashfn;

#endif
