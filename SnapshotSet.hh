#include <vector>
#include <functional>
#include <utility>
#include "Interface.hh"

template <typename K, typename V, unsigned Init_size = 129, typename Hash = std::hash<K>>
class SSHashtable {
public:
    typedef std::pair<K, V> kv_type;
    typedef TransactionTid::signed_type rwlock_type;
    typedef std::pair<rwlock_type, std::vector<kv_type>> bucket_type;

    SSHashtable() {
        table_.resize(Init_size);
    }

    std::pair<bool, std::reference_wrapper<V>> insert(const K& key, const V& value) {
        bucket_type& bkt = table_[hasher_(key) % Init_size];
        TransactionTid::lock_write(bkt.first);
        for (kv_type& kv : bkt.second) {
            if (kv.first == key) {
                TransactionTid::unlock_write(bkt.first);
                return std::make_pair(false, std::ref(kv.second));
            }
        }
        // not found
        bkt.second.push_back(std::make_pair(key, value));
        V& inserted = *(bkt.second.end() - 1);
        TransactionTid::unlock_write(bkt.first);
        return std::make_pair(true, std::ref(inserted));
    }

    std::pair<bool, std::reference_wrapper<V>> find(const K& key) {
        static V default_v = V();
        std::pair<bool, std::reference_wrapper<V>> ret(false, std::ref(default_v));
        bucket_type& bkt = table_[hasher_(key) % Init_size];
        TransactionTid::lock_read(bkt.first);
        for (kv_type& kv : bkt.second) {
            if (kv.first == key) {
                ret.first = true;
                ret.second = std::ref(kv.second);
                break;
            }
        }
        TransactionTid::unlock_read(bkt.first);
        return ret;
    }

private:
    std::vector<bucket_type> table_;
    Hash hasher_;
};

class SnapshotSet {
public:
    typedef uintptr_t oid_type;
    typedef uint64_t sid_type;
    typedef std::pair<sid_type, void *> snapshot_type;
    typedef TransactionTid::signed_type rwlock_type;
    typedef std::pair<rwlock_type, std::vector<snapshot_type>> history_type;

    template <typename T>
    void insert(const oid_type& obj_key, const sid_type& sid, const T& block) {
        auto result = set_.insert(obj_key, history_type());
        rwlock_type& lock = result.second.get().first;
        TransactionTid::lock_write(lock);
        std::vector<snapshot_type>& snapshots = result.second.get().second;
        // making sure nobody sneaked in before us
        for (snapshot_type& ss : snapshots) {
            if (ss.first == sid) {
                TransactionTid::unlock_write(lock);
                return;
            }
        }
        void *ss_mem = malloc(sizeof(T));
        ss_mem = (void *)new (ss_mem) T(block);
        snapshots.push_back(std::make_pair(sid, ss_mem));
        TransactionTid::unlock_write(lock);
    }

    template <typename T>
    T* lookup(const oid_type& oid, const sid_type& sid) {
        auto result = set_.find(oid);
        if (!result.first) {
            return nullptr;
        }

        rwlock_type& lock = result.second.get().first;
        TransactionTid::lock_read(lock);
        std::vector<snapshot_type>& snapshots = result.second.get().second;
        sid_type maxmin = 0;
        void *ss_mem = nullptr;
        for (snapshot_type& ss : snapshots) {
            if (ss.first > sid)
                continue;
            if (ss.first == sid)
                return (T*)ss.second;
            // find the larget @ss.first that is still smaller than @sid
            if (ss.first >= maxmin) {
                // XXX is 0 a valid sid?
                maxmin = ss.first;
                ss_mem = ss.second;
            }
        }
        TransactionTid::unlock_read(lock);
        return (T*)ss_mem;
    }

private:
    SSHashtable<oid_type, history_type> set_;
};
