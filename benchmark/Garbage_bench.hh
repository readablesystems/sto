#include <thread>

#include "DB_index.hh"
#include "TBox.hh"
#include "TCounter.hh"

namespace garbage_bench {

using access_t = bench::access_t;

struct garbage_row {
    enum class NamedColumn : int { value = 0 };
    int value;

    garbage_row() {}
    explicit garbage_row(int v) : value(v) {}
};

struct garbage_key {
    uint64_t k;

    explicit garbage_key(uint64_t p_k) : k(p_k) {}
    explicit garbage_key(lcdf::Str& mt_key) { 
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), mt_key.length());
    }
    operator lcdf::Str() const {
        return lcdf::Str((char *)this, sizeof(*this));
    }
};

};  // namespace garbage_bench

namespace bench {

using garbage_bench::garbage_row;

template <>
struct SplitParams<garbage_row> {
  using split_type_list = std::tuple<garbage_row>;
  using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
  static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

  static constexpr auto split_builder = std::make_tuple(
    [](const garbage_row& in) -> garbage_row {
      garbage_row out;
      out.value = in.value;
      return out;
    }
  );

  static constexpr auto split_merger = std::make_tuple(
    [](garbage_row* out, const garbage_row& in) -> void {
      out->value = in.value;
    }
  );

  static constexpr auto map = [](int col_n) -> int {
    (void)col_n;
    return 0;
  };
};


template <typename A>
class RecordAccessor<A, garbage_row> {
 public:
  
  const int& value() const {
    return impl().value_impl();
  }


  void copy_into(garbage_row* dst) const {
    return impl().copy_into_impl(dst);
  }

 private:
  const A& impl() const {
    return *static_cast<const A*>(this);
  }
};

template <>
class UniRecordAccessor<garbage_row> : public RecordAccessor<UniRecordAccessor<garbage_row>, garbage_row> {
 public:
  UniRecordAccessor(const garbage_row* const vptr) : vptr_(vptr) {}

 private:
  
  const int& value_impl() const {
    return vptr_->value;
  }

  
  void copy_into_impl(garbage_row* dst) const {
    
    if (vptr_) {
      dst->value = vptr_->value;
    }
  }


  const garbage_row* vptr_;
  friend RecordAccessor<UniRecordAccessor<garbage_row>, garbage_row>;
};

template <>
class SplitRecordAccessor<garbage_row> : public RecordAccessor<SplitRecordAccessor<garbage_row>, garbage_row> {
 public:
   static constexpr size_t num_splits = SplitParams<garbage_row>::num_splits;

   SplitRecordAccessor(const std::array<void*, num_splits>& vptrs)
     : vptr_0_(reinterpret_cast<garbage_row*>(vptrs[0])) {}

 private:
  
  const int& value_impl() const {
    return vptr_0_->value;
  }
  

  void copy_into_impl(garbage_row* dst) const {
    
    if (vptr_0_) {
      dst->value = vptr_0_->value;
    }

  }


  const garbage_row* vptr_0_;

  friend RecordAccessor<SplitRecordAccessor<garbage_row>, garbage_row>;
};

};  // namespace bench

namespace garbage_bench {

template <typename DBParams>
class garbage_db {
public:
    template <typename K, typename V>
    using OIndex = typename std::conditional<DBParams::MVCC,
          bench::mvcc_ordered_index<K, V, DBParams>,
          bench::ordered_index<K, V, DBParams>>::type;
    typedef OIndex<garbage_key, garbage_row> table_type;

    table_type& table() {
        return tbl_;
    }
    size_t size() const {
        return size_;
    }
    size_t& size() {
        return size_;
    }
private:
    size_t size_;
    table_type tbl_;
};

template <typename DBParams>
void initialize_db(garbage_db<DBParams>& db, size_t db_size) {
    db.table().thread_init();
    for (size_t i = 0; i < db_size; i += 2)
        db.table().nontrans_put(garbage_key(i), garbage_row(0));
    db.size() = db_size;
}

template <typename DBParams>
class garbage_runner {
public:
    typedef garbage_row::NamedColumn nc;
    void run_txn(size_t key) {
        bool inc_inserts = false;
        bool inc_deletes = false;
        RWTRANSACTION {
            inc_inserts = false;
            inc_deletes = false;
            auto [success, found, row, value] = db.table().select_split_row(garbage_key(key), {{nc::value, access_t::read}});
            (void) row;
            (void) value;
            if (success) {
                if (found) {
                    std::tie(success, found) = db.table().delete_row(garbage_key(key));
                    TXN_DO(success);
                    if (found) {
                        inc_deletes = true;
                    }
                } else {
                    std::tie(success, found) = db.table().insert_row(garbage_key(key), Sto::tx_alloc<garbage_row>());
                    TXN_DO(success);
                    if (!found) {
                        inc_inserts = true;
                    }
                }
            }
        } RETRY(true);
        if (inc_inserts) {
            TXP_INCREMENT(txp_gc_inserts);
        }
        if (inc_deletes) {
            TXP_INCREMENT(txp_gc_deletes);
        }
    }

    // returns the total number of transactions committed
    size_t run() {
        std::vector<std::thread> thrs;
        std::vector<size_t> txn_cnts((size_t)num_runners, 0);
        for (int i = 0; i < num_runners; ++i)
            thrs.emplace_back(
                &garbage_runner::runner_thread, this, i, std::ref(txn_cnts[i]));
        for (auto& t : thrs)
            t.join();
        size_t combined_txn_count = 0;
        for (auto c : txn_cnts)
            combined_txn_count += c;
        return combined_txn_count;
    }

    garbage_runner(int nthreads, double time_limit, garbage_db<DBParams>& database)
        : num_runners(nthreads), tsc_elapse_limit(), db(database) {
        using db_params::constants;
        tsc_elapse_limit = (uint64_t)(time_limit * constants::processor_tsc_frequency * constants::billion);
    }

private:
    void runner_thread(int runner_id, size_t& committed_txns) {
        ::TThread::set_id(runner_id);
        db.table().thread_init();
        std::mt19937 gen(runner_id);
        std::uniform_int_distribution<uint64_t> dist(0, db.size() - 1);

        size_t thread_txn_count = 0;
        auto tsc_begin = read_tsc();
        size_t key = dist(gen) % db.size();
        while (true) {
            run_txn(key);
            key = (key + 1) % db.size();
            ++thread_txn_count;
            if ((thread_txn_count & 0xfful) == 0) {
                if (read_tsc() - tsc_begin >= tsc_elapse_limit)
                    break;
            }
        }

        committed_txns = thread_txn_count;
    }

    int num_runners;
    uint64_t tsc_elapse_limit;
    garbage_db<DBParams> db;
};

};
