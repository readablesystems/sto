#ifndef _CuckooHashMapNA_HH
#define _CuckooHashMapNA_HH

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

#include "TaggedLow.hh"
#include "Interface.hh"
#include "TWrapped.hh"
#include "Transaction.hh"
#include "cuckoohelpers.hh"

/*! CuckooHashMapNA is the hash table class. */
template <class Key, class T, unsigned Num_Buckets = 250000, bool Opacity=false>
class CuckooHashMapNA : public Shared {

    // Structs and functions used internally

    typedef enum {
        ok = 0,
        failure_too_slow = 1,
        failure_key_not_found = 2,
        failure_key_duplicated = 3,
        failure_space_not_enough = 4,
        failure_function_not_supported = 5,
        failure_table_full = 6,
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
            if (!is_locked()) {
                return num.try_lock();
            }
            return true;
        }

        inline bool is_locked() const {
            return num.is_locked();
        }
    } __attribute__((aligned(64)));
    
    /* This is a hazard pointer, used to indicate which version of the
     * TableInfo is currently being used in the thread. Since
     * CuckooHashMapNA operations can run simultaneously in different
     * threads, this variable is thread local. Note that this variable
     * can be safely shared between different CuckooHashMapNA
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
    // means each template instantiation of a CuckooHashMapNA class
    // gets its own per-thread hazard pointer, then each template
    // instantiation of a CuckooHashMapNA class can get its own
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
    explicit CuckooHashMapNA(size_t n = Num_Buckets * SLOT_PER_BUCKET) {
        cuckoo_init(reserve_calc(n));
    }

    void initialize(size_t n = Num_Buckets * SLOT_PER_BUCKET) {
        cuckoo_init(reserve_calc(n));
    }
    /*! The destructor deletes any remaining table pointers managed by
     * the hash table, also destroying all remaining elements in the
     * table. */
    ~CuckooHashMapNA() {
        TableInfo *ti = table_info.load();
        if (ti != nullptr) {
            delete ti;
        }

        for (auto it = old_table_infos.cbegin(); it != old_table_infos.cend(); it++) {
            delete *it;
        }
    }

    // STO functions
    bool check(TransItem& item, Transaction&) override {
        if (is_bucket(item)) {
            auto read_version = item.template read_value<version_type>();
            auto b = unpack_bucket(item);
            return b->bucketversion.check_version(read_version);
        } else {
            auto key = unpack_key(item);
            size_t hv = hashed_key(key);
            size_t i1, i2;
            TableInfo *ti;
            snapshot_get_buckets(ti, hv, i1, i2);
            internal_elem* e;
            cuckoo_status res = find_one_ptr(ti, key, e, i1, i2);
            
            if (has_insert(item)) {
                return (res == failure_key_not_found);
            } else if (has_delete(item)) {
                return (res == ok);
            } else { // was a find
                if (res == failure_key_not_found) {
                    return false;
                } else {
                    auto read_version = item.template read_value<version_type>();
                    return !is_erased(*e) && e->version.check_version(read_version);
                }
            }
        }
    }

    bool lock(TransItem& item, Transaction& txn) override {
        if (is_bucket(item)) {
            auto b = unpack_bucket(item);
            if (!b->bucketversion.is_locked_here()) {
                return txn.try_lock(item, b->bucketversion);
            } else {
                return true;
            }
        } else {
            auto key = unpack_key(item);
            size_t hv = hashed_key(key);
            size_t i1, i2;
            TableInfo *ti;
            snapshot_get_buckets(ti, hv, i1, i2);
            internal_elem* e;
            cuckoo_status res = find_one_ptr(ti, key, e, i1, i2);

            return (res == ok) && try_lock_bucketversions(txn, item, ti, i1, i2) && txn.try_lock(item, e->version);
        }
    }

    void install(TransItem& item, Transaction& t) override {
        if (is_bucket(item)) {
            auto b = unpack_bucket(item);
            b->inc_version();
        } else {
            auto key = unpack_key(item);
            size_t hv = hashed_key(key);
            size_t i1, i2;
            TableInfo *ti;
            snapshot_get_buckets(ti, hv, i1, i2);
            internal_elem* e;

            mapped_type val = item.template write_value<mapped_type>();
            if (has_insert(item)) {
                assert(insert_one(ti, hv, key, val, i1, i2, e) == ok);
                if (Opacity) {
                    e->version.set_version(t.commit_tid());
                } else {
                    e->version.inc_nonopaque_version();
                    e->version.set_version_locked(e->version.value());
                }
            } else if (has_delete(item)) {
                assert(delete_one(ti, hv, key, i1, i2, e) == ok);
                if (Opacity) {
                    e->version.set_version(t.commit_tid());
                } else {
                    e->version.inc_nonopaque_version();
                    e->version.set_version_locked(e->version.value() & erased_bit);
                    e->version.unlock(); 
                }
            }
        }
        assert(0);
    }

    void unlock(TransItem& item) override {
        if (is_bucket(item)) {
            auto b = unpack_bucket(item);
            if (b->bucketversion.is_locked_here()) {
                b->bucketversion.unlock();
            }
        } else {
            auto key = unpack_key(item);
            size_t hv = hashed_key(key);
            size_t i1, i2;
            TableInfo *ti;
            snapshot_get_buckets(ti, hv, i1, i2);
            unlock_bucketversions(ti, i1, i2);
        }
    }

    void cleanup(TransItem&, bool) override {
        return;
    }


    size_t num_inserts() {
        TableInfo *ti;
        snapshot_no_hazard(ti);
        size_t inserts = 0;
        for (size_t i = 0; i < ti->num_inserts.size(); i++) {
            inserts += ti->num_inserts[i].num.load();
        }
        return inserts;
    }

    size_t num_deletes() {
        TableInfo *ti;
        snapshot_no_hazard(ti);
        size_t deletes = 0;
        for (size_t i = 0; i < ti->num_deletes.size(); i++) {
            deletes += ti->num_deletes[i].num.load();
        }
        return deletes;
    }

    bool find(const key_type& key, mapped_type& val) {
        check_hazard_pointers();
        size_t hv = hashed_key(key);
        size_t i1_o, i2_o;
        TableInfo *ti;
        cuckoo_status res;
        internal_elem e;
        snapshot_get_buckets(ti, hv, i1_o, i2_o);
        
        res = find_one(ti, key, e, i1_o, i2_o);
        val = e.value.access();

        unset_hazard_pointers(); 
        return (res == ok);
    }

    bool insert(const key_type& key, const mapped_type& val) {
        internal_elem e = create_internal_elem(key, val);
        check_hazard_pointers();
        size_t hv = hashed_key(key);
        size_t i1_o, i2_o;
        TableInfo *ti;
        cuckoo_status res;
        
        snapshot_get_buckets(ti, hv, i1_o, i2_o);
        //XXX adding additional reads of value but oh well
        res = find_one(ti, key, e, i1_o, i2_o);

        // we're actually going to insert!
        if (res == failure_key_not_found) {
            //XXX going to write to both buckets for now
            Sto::item(this, pack_bucket(&ti->buckets_[i1_o])).add_write();
            Sto::item(this, pack_bucket(&ti->buckets_[i2_o])).add_write();
            // make sure we insert at commit time
            auto item = Sto::item(this, pack_key(e.key));
            item.template add_write<mapped_type>(val);
            item.clear_flags(delete_tag).add_flags(insert_tag);
        }
        unset_hazard_pointers(); 

        // we don't need to add any writes---just need to make sure buckets aren't modified
        return (res == failure_key_not_found);
    }

    bool erase(const key_type& key) {
        check_hazard_pointers();
        size_t hv = hashed_key(key);
        size_t i1_o, i2_o;
        TableInfo *ti;
        cuckoo_status res;
        internal_elem e;
        
        snapshot_get_buckets(ti, hv, i1_o, i2_o);
        //XXX adding additional reads of value but oh well
        res = find_one(ti, key, e, i1_o, i2_o);

        // we are going to actually erase this
        if (res == ok) {
            // make sure we erase at commit time
            Sto::item(this, pack_key(e.key)).add_write().add_flags(delete_tag);
        }
        unset_hazard_pointers(); 

        // XXX we're not going to read our own inserts---so we might end up deleting our own insert
        // during install. oh well.
        return (res == ok);
    }

private:
    typedef typename std::conditional<Opacity, TVersion, TNonopaqueVersion>::type version_type;
    typedef typename std::conditional<Opacity, TWrapped<mapped_type>, TNonopaqueWrapped<mapped_type>>::type 
        wrapped_type;

    // used to mark if item has been inserted by the currently running txn
    // mostly used at commit time to know whether to remove the item during cleanup
    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    // used to mark if item has been deleted by the currently running txn
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr TransactionTid::type erased_bit = TransactionTid::user_bit<<1;
    static constexpr TransactionTid::type inserted_bit = TransactionTid::user_bit<<2;

    struct internal_elem {
        key_type key;
        version_type version;
        wrapped_type value;
        
        bool erased() {return version.value() & erased_bit;}
    };

    static internal_elem create_internal_elem(const key_type& k, const mapped_type& val) {
        internal_elem e;
        e.key = k;
        e.value = val;
        e.version = Sto::initialized_tid();
        return e;
    }

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
        internal_elem elems[SLOT_PER_BUCKET];
        unsigned char overflow;
        version_type bucketversion;

        void setKV(size_t pos, internal_elem elem) {
            assert(!getBit(occupied, pos));
            setBit(occupied, pos);
            elems[pos] = elem;
        }

        // sets the value in the bucket to NULL, but doesn't actually
        // free it (used for transfers)
        void eraseKV(size_t pos) {
            assert(getBit(occupied, pos));
            resetBit(occupied, pos);
            elems[pos].key = 0;
            (elems + pos)->~internal_elem();
        }

        void clear() {
            for (size_t i = 0; i < SLOT_PER_BUCKET; i++) {
                if (getBit(occupied,i)) {
                    eraseKV(i);
                }
            }
            overflow = 0;
        }

        // should lock around calling this
        void inc_version() {
#if LIBCUCKOO_DEBUG
            assert(bucketversion.is_locked_here());
#endif
            bucketversion.inc_nonopaque_version();
        }
        void lock_and_inc_version() {
            if (!bucketversion.is_locked_here()) bucketversion.lock();
            inc_version();
            bucketversion.unlock();
        }
    };

    typedef struct packed_type {
        bool bucket_bit;
        union {
            key_type key;
            Bucket* bucket;
        };

        inline bool operator==(const packed_type& x) const {
          if (bucket_bit)
             return x.bucket_bit && bucket == x.bucket;
          else
             return !x.bucket_bit && key == x.key;
       }
    } packed_type;

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

        /* The constructor allocates the memory for the table. For
         * buckets, it uses the bucket_allocator, so that we can free
         * memory independently of calling its destructor. It
         * allocates one cacheint for each core in num_inserts and
         * num_deletes. */
        TableInfo(const size_t hashtable_init) {
            buckets_ = nullptr;
                hashpower_ = hashtable_init;

                if( hashpower_ > 10) {
                    buckets_ = static_cast<Bucket*>( calloc(hashsize(hashpower_), sizeof(Bucket)));
                    if( buckets_ == nullptr ) {
                        throw std::bad_alloc();
                    }
                    //buckets_ = new Bucket[hashsize(hashpower_)];
                } else {
                    buckets_ = new Bucket[hashsize(hashpower_)];
                }
                
                num_inserts.resize(kNumCores);
                num_deletes.resize(kNumCores);

                for (size_t i = 0; i < kNumCores; i++) {
                    num_inserts[i].num.store(0);
                    num_deletes[i].num.store(0);
                }
        }

        ~TableInfo() {
            if( hashpower_ > 10) {
                free(buckets_);
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

    static key_equal eqfn;
    static std::allocator<Bucket> bucket_allocator;

    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_tag;
    }

    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_tag;
    }

    static bool is_erased(internal_elem& e) {
        return e.erased();
    }

    static bool is_bucket(const TransItem& item) {
        return item.key<packed_type>().bucket_bit;
    }
    packed_type pack_bucket(Bucket* bucket) {
        packed_type pt;
        pt.bucket = bucket;
        pt.bucket_bit = true;
        return pt; 
    }
    packed_type pack_key(key_type& key) {
        packed_type pt;
        pt.key = key;
        pt.bucket_bit = false;
        return pt; 
    }
    Bucket* unpack_bucket(const TransItem& item) {
        return item.key<packed_type>().bucket;
    }
    key_type unpack_key(const TransItem& item) {
        return item.key<packed_type>().key;
    }

    /* find_one searches a specific table instance for the value corresponding to a given hash value 
     * both i1 and i2 are locked */
    cuckoo_status find_one_ptr(const TableInfo *ti, key_type& key, internal_elem*& e, size_t& i1, size_t& i2) {
        cuckoo_status st;
        st = try_read_ptr_from_bucket(ti, key, e, i1);
        if (st == ok || !hasOverflow(ti, i1) ) {
            if (st == ok) {
                return ok;
            } else {
                return failure_key_not_found;
            }
        } 
        st = try_read_ptr_from_bucket(ti, key, e, i2);
        if(st == ok) {
            return ok;
        }
        return failure_key_not_found;
    }

    cuckoo_status try_read_ptr_from_bucket(const TableInfo* ti, key_type& key, internal_elem*& e, size_t& i) {
        for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
            internal_elem* elem = &ti->buckets_[i].elems[j];
            if (!getBit(ti->buckets_[i].occupied, j)) {
                continue;
            }

            if (eqfn(key, elem->key)) {
                e = elem;
                return ok;
            }
        }
        return failure_key_not_found;
    }


    /* find_one searches a specific table instance for the value corresponding to a given hash value 
     * It doesn't take any locks */
    cuckoo_status find_one(const TableInfo *ti, key_type key, internal_elem& e, size_t& i1, size_t& i2) {
        size_t v1_i, v2_i, v1_f, v2_f;
        cuckoo_status st;
        do {
            get_version_two(ti, i1, i2, v1_i, v2_i);
            st = try_read_from_bucket(ti, key, e, i1);
            if (st == ok || !hasOverflow(ti, i1) ) {
                if (st == failure_key_not_found) {
                    // add reads here of only the first bucket in which the item could possibly be located. 
                    // Don't need to read the 2nd because the bucket had no overflow
                    Sto::item(this, pack_bucket(&ti->buckets_[i1])).observe(ti->buckets_[i1].bucketversion);
                    return failure_key_not_found;
                } 
                // we found the item!
                else if (st == ok) {
                    goto item_found;
                }
            } 

            st = try_read_from_bucket(ti, key, e, i2);
            if(st == ok) {
                goto item_found;
            }
            get_version_two(ti, i1, i2, v1_f, v2_f);
        } while(!check_version_two(v1_i, v2_i, v1_f, v2_f));
        
        // didn't find item
        Sto::item(this, pack_bucket(&ti->buckets_[i1])).observe(ti->buckets_[i1].bucketversion);
        Sto::item(this, pack_bucket(&ti->buckets_[i2])).observe(ti->buckets_[i2].bucketversion);
        return failure_key_not_found;

item_found:
        auto item = Sto::item(this, pack_key(e.key));
        // we don't need to update our read versions here because the previous add of 
        // has_write must check the proper versions
        if (item.has_write()) {
            if (has_delete(item)) {
                return failure_key_not_found;
            } else { // we deleted and then inserted this item, so we need to update its value
                e.value = item.template write_value<mapped_type>();
                return ok;
            }
        }
        // add a read of the item so we know the value hasn't changed @ commit
        e.value = e.value.read(item, e.version);
        // this checks if the item has been deleted since we found it
        if (is_erased(e)) {
            // no need to unlock here -- there are no locks!
            Sto::abort_because("erased element in find"); assert(0);
        } 
        return ok;
    }

    /* insert_one tries to insert a key-value pair into a specific table instance.
     * It will return a failure if the key is already in the table, we've started an expansion,
     * or a bucket was moved.
     * Regardless, it starts with the buckets unlocked and ends with buckets unlocked.
     * In particular, failure_too_slow will trigger a retry
     */
    cuckoo_status insert_one(TableInfo *ti, size_t hv, const key_type& key,
                            const mapped_type& val, size_t& i1, size_t& i2, internal_elem*& e) {
        int open1, open2;
        cuckoo_status res1, res2;
    RETRY:
        //fastpath: 
        lock( ti, i1 );
        res1 = try_add_to_bucket(ti, key, val, i1, open1);
        if (res1 == failure_key_duplicated) {
            unlock( ti, i1 );
            return res1;
        } else {
            assert( res1 == ok ); 
            
            if( !hasOverflow(ti, i1) && open1 != -1 ) {
                add_to_bucket(ti, key, val, i1, open1, e);
                unlock( ti, i1 );
                return ok;
            } else { //we need to try and get the second lock
                if( !try_lock(ti, i2) ) {
                    unlock(ti, i1);
                    lock_two(ti, i1, i2);
                    // we need to make sure nothing happened to the first bucket while it was unlocked
                    res1 = try_add_to_bucket(ti, key, val, i1, open1);
                    if (res1 == failure_key_duplicated) {
                        unlock_two(ti, i1, i2);
                        return res1;
                    }
                }
            }
        }

        //at this point we must have both buckets locked
        res2 = try_add_to_bucket(ti, key, val, i2, open2);
        if (res2 == failure_key_duplicated) {
            unlock_two(ti, i1, i2);
            return res2;
        }

        if (open1 != -1) {
            add_to_bucket(ti, key, val, i1, open1, e);
            unlock_two(ti, i1, i2);
            return ok;
        }

        if (open2 != -1) {
            ti->buckets_[i1].overflow++;
            add_to_bucket(ti, key, val, i2, open2, e);
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
            add_to_bucket(ti, key, val, insert_bucket, insert_slot, e);
            unlock_two(ti, i1, i2);
            return ok;
        }

        if (st == failure_table_full) {
            assert(0);
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
                             size_t& i1, size_t& i2, internal_elem*& e) {
        i1 = index_hash(ti, hv);
        i2 = alt_index(ti, hv, i1);
        cuckoo_status res1, res2;
        
        lock( ti, i1 );
        res1 = try_del_from_bucket(ti, key, i1, e);
        // if we found the element there, or nothing ever was added to second bucket, then we're done
        if (res1 == ok || !hasOverflow(ti, i1) ) {
            unlock( ti, i1 );
            return res1;
        } else {
            if( !try_lock(ti, i2) ) {
                unlock(ti, i1);
                lock_two(ti, i1, i2);
                res1 = try_del_from_bucket(ti, key, i1, e);
                if( res1 == ok) {
                    unlock_two(ti, i1, i2);
                    return res1;
                }
            }
        }

        res2 = try_del_from_bucket(ti, key, i2, e);
        if( res2 == ok ) {
            ti->buckets_[i1].overflow--;
            unlock_two(ti, i1, i2);
            return res2;
        }

        unlock_two(ti, i1, i2);
        return failure_key_not_found;
    }

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

    static inline bool try_lock_bucketversions(Transaction& txn, TransItem& item, const TableInfo *ti, size_t i1, size_t i2) {
        bool locked = true;
        if (i1 < i2) {
            if (!ti->buckets_[i1].bucketversion.is_locked_here()) {
                locked &= txn.try_lock(item, (ti->buckets_[i1]).bucketversion);
            }
            if (!ti->buckets_[i2].bucketversion.is_locked_here()) {
                locked &= txn.try_lock(item, (ti->buckets_[i2]).bucketversion);
            }
        } else if (i2 < i1) {
            if (!ti->buckets_[i2].bucketversion.is_locked_here()) {
                locked &= txn.try_lock(item, (ti->buckets_[i2]).bucketversion);
            }
            if (!ti->buckets_[i1].bucketversion.is_locked_here()) {
                locked &= txn.try_lock(item, (ti->buckets_[i1]).bucketversion);
            }
        } else {
            if (!ti->buckets_[i1].bucketversion.is_locked_here()) {
                locked &= txn.try_lock(item, (ti->buckets_[i1]).bucketversion);
            }
        }
        return locked;
    }

    static inline void unlock_bucketversions(const TableInfo *ti, size_t i1, size_t i2) { 
        if (ti->buckets_[i1].bucketversion.is_locked_here()) {
            (ti->buckets_[i1]).bucketversion.unlock();
        }
        if (i1 != i2) {
            if (ti->buckets_[i2].bucketversion.is_locked_here()) {
                (ti->buckets_[i2]).bucketversion.unlock();
            }
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

    void snapshot(TableInfo*& ti) {
    TryAcquire:
        size_t start_version = snapshot_lock.get_version();
        ti = table_info.load();
        *hazard_pointer_old = static_cast<void*>(ti);
        size_t end_version = snapshot_lock.get_version();
        if( start_version % 2 == 1 || start_version != end_version) {
            goto TryAcquire;
        }
    }
    void snapshot_get_buckets(TableInfo*& ti, size_t hv, size_t& i1_o, size_t& i2_o) {
        snapshot(ti);
        i1_o = index_hash(ti, hv);
        i2_o = alt_index(ti, hv, i1_o);
    }

    void snapshot_no_hazard(TableInfo*& ti) {
        ti = table_info.load();
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
        internal_elem elem;

        // The initial pathcode informs cuckoopath_search which bucket
        // the path starts on
        q.enqueue(b_slot(i1, 0, 0));
        q.enqueue(b_slot(i2, 1, 0));
        while (q.not_full() && q.not_empty()) {
            b_slot x = q.dequeue();
            // Picks a random slot to start from
            for (size_t slot = 0; slot < SLOT_PER_BUCKET && q.not_full(); slot++) {
                elem = ti->buckets_[x.bucket].elems[slot];
                const size_t hv = hashed_key<key_type>(elem.key);

                if (!getBit(ti->buckets_[x.bucket].occupied, slot)) {
                    // We can terminate the search here
                    x.pathcode = x.pathcode * SLOT_PER_BUCKET + slot;
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
                for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
                    if (!getBit(ti->buckets_[y.bucket].occupied, j)) {
                        y.pathcode = y.pathcode * SLOT_PER_BUCKET + j;
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
        b_slot x = slot_search(ti, i1, i2);
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
        CuckooRecord *curr = cuckoo_path;
        internal_elem elem;
        if (x.pathcode == 0) {
            curr->bucket = i1;
            elem = ti->buckets_[curr->bucket].elems[curr->slot];
            if (!getBit(ti->buckets_[curr->bucket].occupied, curr->slot)) {
                // We can terminate here
                return 0;
            }
            curr->key = elem.key;
        } else {
            assert(x.pathcode == 1);
            curr->bucket = i2;
            elem = ti->buckets_[curr->bucket].elems[curr->slot];
            if (!getBit(ti->buckets_[curr->bucket].occupied, curr->slot)) {
                // We can terminate here
                return 0;
            }
            curr->key = elem.key; 
        }
        for (int i = 1; i <= x.depth; i++) {
            CuckooRecord *prev = curr++;
            const size_t prevhv = hashed_key<key_type>(prev->key);
            assert(prev->bucket == index_hash(ti, prevhv) ||
                   prev->bucket == alt_index(ti, prevhv, index_hash(ti, prevhv)));
            // We get the bucket that this slot is on by computing the
            // alternate index of the previous bucket
            curr->bucket = alt_index(ti, prevhv, prev->bucket);
            elem = ti->buckets_[curr->bucket].elems[curr->slot];
            if (!getBit(ti->buckets_[curr->bucket].occupied, curr->slot)) {
                // We can terminate here
                return i;
            }
            curr->key = elem.key;
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

        internal_elem elem;
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
            elem = ti->buckets_[bucket].elems[cuckoo_path[0].slot];
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
            /* We plan to kick out fs, but let's check if it is still
             * there; there's a small chance we've gotten scooped by a
             * later cuckoo. If that happened, just... try again. Also
             * the slot we are filling in may have already been filled
             * in by another thread, or the slot we are moving from
             * may be empty, both of which invalidate the swap. */
            if (!eqfn(ti->buckets_[fb].elems[fs].key, from->key) ||
                getBit(ti->buckets_[tb].occupied, ts) ||
                !getBit(ti->buckets_[fb].occupied, fs) ) {
                if (depth == 1) {
                    unlock_three(ti, fb, tb, ob);
                } else {
                    unlock_two(ti, fb, tb);
                }
                return failure_too_slow;
            }
            size_t hv = hashed_key( ti->buckets_[fb].elems[fs].key);
            size_t first_bucket = index_hash(ti, hv);
            if( fb == first_bucket ) { //we're moving from first bucket to alternate
                auto old_overflow = ti->buckets_[first_bucket].overflow++;
                if (old_overflow) ti->buckets_[first_bucket].lock_and_inc_version();
            } else { //we're moving from alternate to first bucket
                assert( tb == first_bucket );
                ti->buckets_[first_bucket].overflow--;
            }

            ti->buckets_[tb].setKV(ts, ti->buckets_[fb].elems[fs]);
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

        }
        LIBCUCKOO_DBG("Should never reach here");
        return failure_function_not_supported;
    }
    
    static cuckoo_status try_read_from_bucket(const TableInfo *ti, const key_type& key,
                                     internal_elem& e, const size_t i) {
        for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
            internal_elem elem = ti->buckets_[i].elems[j];
            if (!getBit(ti->buckets_[i].occupied, j)) {
                continue;
            }

            if (eqfn(key, elem.key)) {
                e = elem;
                return ok;
            }
        }
        return failure_key_not_found;
    } 

    /* add_to_bucket will insert the given key-value pair into the
     * slot. */
    static inline void add_to_bucket(TableInfo *ti, 
                                     const key_type &key, const mapped_type &val,
                                     const size_t i, const size_t j, internal_elem*& e) {
        assert(!getBit(ti->buckets_[i].occupied, j));
        
        internal_elem elem = create_internal_elem(key, val);
        ti->buckets_[i].setKV(j, elem);
        e = &ti->buckets_[i].elems[j];
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
        internal_elem elem_in_table;
        for (size_t k = 0; k < SLOT_PER_BUCKET; k++) {
            elem_in_table = ti->buckets_[i].elems[k];
            if (getBit(ti->buckets_[i].occupied, k)) {
                if (eqfn(key, elem_in_table.key)) {
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
                                    const key_type &key, const size_t i, internal_elem*& e) {
        for (size_t j = 0; j < SLOT_PER_BUCKET; j++) {
            e = &ti->buckets_[i].elems[j];
            if (!getBit(ti->buckets_[i].occupied, j)) {
                continue;
            }
            if (eqfn(key, e->key)) {
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
                                     size_t hv, const TableInfo *ti,
                                     const size_t i1, const size_t i2) {
        (void)hv;
        cuckoo_status res1, res2;
        internal_elem e;
        res1 = try_read_from_bucket(ti, key, e, i1);
        val = e.value.access();
        if (res1 == ok || !hasOverflow(ti, i1) ) {
            return res1;
        } 
        res2 = try_read_from_bucket(ti, key, e, i2);
        val = e.value.access();
        if(res2 == ok) {
            return ok;
        }
        return res2;
    }

    /* cuckoo_init initializes the hashtable, given an initial
     * hashpower as the argument. */
    cuckoo_status cuckoo_init(const size_t hashtable_init) {
        table_info.store(new TableInfo(hashtable_init));
        new_table_info.store(nullptr);
        return ok;
    }

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

        return ok;
    }
};

// Initializing the static members
template <class Key, class T, unsigned Num_Buckets, bool Opacity>
__thread void** CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::hazard_pointer_old = nullptr;

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
__thread void** CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::hazard_pointer_new = nullptr;

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
__thread int CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::counterid = -1;

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
typename std::allocator<typename CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::Bucket>
CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::bucket_allocator;

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
typename CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::GlobalHazardPointerList
CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::global_hazard_pointers;

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
const size_t CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::kNumCores =
    std::thread::hardware_concurrency() == 0 ?
    sysconf(_SC_NPROCESSORS_ONLN) : std::thread::hardware_concurrency();

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
std::atomic<size_t> CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::numThreads(0);

template <class Key, class T, unsigned Num_Buckets, bool Opacity>
typename CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::key_equal
CuckooHashMapNA<Key, T, Num_Buckets, Opacity>::eqfn;

#endif
